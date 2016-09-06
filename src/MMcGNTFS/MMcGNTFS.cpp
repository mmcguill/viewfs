/////////////////////////////////////////////////////////////////////
//
// Original Filename: MMcGNTFS.cpp
// Author: Mark McGuill
// Create Date: 20/03/2005
//
//
/////////////////////////////////////////////////////////////////////

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "..\..\inc\viewfs_types.h"
#include "MMcGNTFS.H"

/////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
	#define _CRTDBG_MAP_ALLOC
	#include <crtdbg.h>

	#define ASSERT(x) _ASSERTE(x)
#else
	#define ASSERT(x)
#endif

/////////////////////////////////////////////////////////////////////

#define INIT_HARDCODED_SECTOR_SIZE 0x200

/////////////////////////////////////////////////////////////////////

using namespace MMcGNTFS;

/////////////////////////////////////////////////////////////////////

NTFSReader::NTFSReader()
{
	m_fInit = false;
	m_hFile = NULL;
	m_dwLastError = 0;
	
	m_cbSector =		INIT_HARDCODED_SECTOR_SIZE;
	m_cbCluster =		0;
	m_cbIndexBlock =	0;	
	m_llLCNMFT =		0;	
	m_llLCNMFTMirr =	0;	
	m_llTotalSectors =	0;
	m_pGetAttributeMemCache = NULL;
	m_pGetMFTEntryCache = NULL;
	m_llGetAttributeLastFile = -1;
	m_llGetMFTEntryLastFile = -1;

	/*_CrtSetDbgFlag(_CRTDBG_DELAY_FREE_MEM_DF | 
		_CRTDBG_CHECK_CRT_DF | _CRTDBG_ALLOC_MEM_DF |
		_CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_LEAK_CHECK_DF);*/

}

/////////////////////////////////////////////////////////////////////

NTFSReader::~NTFSReader()
{
	Close();
}

/////////////////////////////////////////////////////////////////////

bool NTFSReader::Init(HANDLE hFile, LONGLONG llBeginFileOffset)
{
	if (m_fInit)
	{
		ASSERT(!m_fInit);
		m_dwLastError = ERR_CALL_CLOSE_FIRST;
		return false;
	}

	// Duplicate File Handle So Caller Can forget it

	HANDLE hProc = GetCurrentProcess();
	if (!DuplicateHandle(hProc,hFile,hProc,&m_hFile,0,false,
		DUPLICATE_SAME_ACCESS))
	{
		m_dwLastError = ERR_OPENING_FILE;
		return false;
	}

	m_llBeginFileOffset = llBeginFileOffset;

	
	// Read First Sector will contain BOOT_SECTOR

	LARGE_INTEGER liTmp;
	liTmp.QuadPart = m_llBeginFileOffset;
	if(!SetFilePointerEx(m_hFile,liTmp,NULL,FILE_BEGIN))
	{
		m_dwLastError = ERR_READING_FILE;
		CloseHandle(m_hFile);
		return false;
	}

	DWORD cbRead = 0;
	BYTE *pBuffer = new BYTE[m_cbSector];
	if(NULL == pBuffer)
	{
		m_dwLastError = ERR_OUT_OF_MEMORY;
		CloseHandle(m_hFile);
		return false;
	}

	if(!ReadFile(m_hFile,pBuffer,m_cbSector,&cbRead,NULL))
	{
		delete pBuffer;
		m_dwLastError = ERR_READING_FILE;
		CloseHandle(m_hFile);
		return false;
	}

	PBOOT_SECTOR pbs = (PBOOT_SECTOR)pBuffer;

	
	// State we will need

	m_cbSector =		pbs->bpb.BytesPerSector;
	m_cbCluster =		pbs->bpb.SectorsPerCluster * m_cbSector;
	m_cbIndexBlock =	pbs->bpbex.ClustersPerIndexBlock * 
							m_cbCluster;
	m_llLCNMFT =		pbs->bpbex.LogClusterMFT;
	m_llLCNMFTMirr =	pbs->bpbex.LogClusterMFTMirr;
	m_llTotalSectors =	pbs->bpbex.TotalSectors;

    delete pBuffer;

	
	// Initialize Master File Table

	if(!InitMFT())
	{
		m_dwLastError = ERR_INITIALIZING;
		CloseHandle(m_hFile);
		return false;
	}

	
	// Alloc Get attribute Cache (Perf)

	m_pGetAttributeMemCache = new BYTE[HARDCODED_MFT_ENTRY_SIZE];
	
	if(NULL == m_pGetAttributeMemCache)
	{
		m_dwLastError = ERR_OUT_OF_MEMORY;
		CloseHandle(m_hFile);
		return false;
	}

	m_pGetMFTEntryCache = new BYTE[HARDCODED_MFT_ENTRY_SIZE];
	if(NULL == m_pGetMFTEntryCache)
	{
		m_dwLastError = ERR_OUT_OF_MEMORY;
		CloseHandle(m_hFile);
		return false;
	}

	// Initialize our File Data Helper...

	if(	!m_drhFile.Init(m_hFile, m_llBeginFileOffset, m_cbCluster))
	{
		m_dwLastError = ERR_INITIALIZING;
		CloseHandle(m_hFile);
		return false;
	}

	m_fInit = true;
	return true;
}

/////////////////////////////////////////////////////////////////////

bool NTFSReader::Close()
{
	m_fInit = false;

	if(m_hFile)
	{
		CloseHandle(m_hFile);
	}

	m_hFile = NULL;
	
	m_dwLastError = 0;

	m_cbSector =		INIT_HARDCODED_SECTOR_SIZE;
	m_cbCluster =		0;
	m_cbIndexBlock =	0;	
	m_llLCNMFT =		0;	
	m_llLCNMFTMirr =	0;	
	m_llTotalSectors =	0;

	if(m_pGetAttributeMemCache)
	{
		delete m_pGetAttributeMemCache;
	}
	m_pGetAttributeMemCache = NULL;

	if(m_pGetMFTEntryCache)
	{
		delete m_pGetMFTEntryCache;
	}
	m_pGetMFTEntryCache = NULL;

	m_llGetAttributeLastFile = -1;
	m_llGetMFTEntryLastFile = -1;

	m_drhFile.Clear();
	m_drhMFT.Clear();

	return true;
}

/////////////////////////////////////////////////////////////////////

DWORD NTFSReader::GetLastError()
{
	return m_dwLastError;
}

/////////////////////////////////////////////////////////////////////

bool NTFSReader::InitMFT()
{
	// Actually called by Init so cannot assert on m_fInit
	//ASSERT(m_fInit); 

	LARGE_INTEGER lrgOffset;
	lrgOffset.QuadPart = m_llBeginFileOffset + (m_llLCNMFT * m_cbCluster);

	BYTE *pTmp = new BYTE[HARDCODED_MFT_ENTRY_SIZE];
	if(NULL == pTmp)
	{
		m_dwLastError = ERR_OUT_OF_MEMORY;
		return false;
	}

	DWORD cbRead;
	if (!SetFilePointerEx(m_hFile,lrgOffset,NULL,FILE_BEGIN) || 
		!ReadFile(m_hFile,pTmp,HARDCODED_MFT_ENTRY_SIZE,
		&cbRead,NULL))
	{
		delete pTmp;
		m_dwLastError = ERR_READING_FILE;
		return false;
	}

	// Parse For $DATA attribute
	// TODO: What if there's an $ATTRIBUTE_LIST in here!!

	PMFT_FILE_ENTRY pmfe = (PMFT_FILE_ENTRY)pTmp;
	if(pmfe->Magic != MFT_ENTRY_MAGIC)
	{
		delete pTmp;
		m_dwLastError = ERR_INVALID_MFT_ENTRY;
		return false;
	}

	// Deal with Update Seq Numbers => These are designed to 
	// detect corruption

	if(pmfe->SizeUpdateSeq)
	{
		WORD *pUpdate = (WORD*)(pTmp + pmfe->OffsetUpdateSeq + 
			sizeof(WORD));
		for(int i=1;i<pmfe->SizeUpdateSeq;i++)
		{
			// TODO: Hardcoded Sector Size!!! 0x200 fix
			*(WORD*)(pTmp + (i * 0x200) - sizeof(WORD)) = *pUpdate;
			pUpdate++;
		}
	}


	PATTR_HEADER pah = (PATTR_HEADER)(pTmp + pmfe->offsetFirstAttr);
	bool fFoundData = false;

	while(((BYTE*)pah < 
		(pTmp + pmfe->used_size - SIZEOF_ATTR_HEADER_RESIDENT)))
	{
		if(pah->Type == ATTR_TYPE_ATTRIBUTE_LIST)
		{
			// TODO: Assert
			delete pTmp;
			m_dwLastError = ERR_CANNOT_PARSE;
			return false;
		}

		if(pah->Type == ATTR_TYPE_DATA && pah->NameLength == 0)
		{
			fFoundData = true;
			break;
		}

		pah = (PATTR_HEADER)(((BYTE*)pah) + pah->Length);
	}

	if(!fFoundData)
	{
		delete pTmp;
		m_dwLastError = ERR_INVALID_MFT_ENTRY;
		return false;
	}

	// Load it up

	BYTE* pRuns = ((BYTE*)pah) + pah->extra.non_resident.OffsetDataRuns;

	if( !m_drhMFT.Init(m_hFile,m_llBeginFileOffset,m_cbCluster) || 
		!m_drhMFT.SetAttributeHeader(pah))
	{
		delete pTmp;
		m_dwLastError = ERR_INVALID_MFT_ENTRY;
		return false;
	}

	// Loaded!!

	delete pTmp;
	return true;
}

/////////////////////////////////////////////////////////////////////

bool NTFSReader::ReadItemDataToFile(LONGLONG llFile, LPTSTR lpszPath,
									bool fCompressed)
{
	ASSERT(m_fInit);
	if(false == m_fInit)
	{
		m_dwLastError = ERR_NOT_INIT;
		return false;
	}

	ASSERT(lpszPath);
	if(NULL == lpszPath)
	{
		m_dwLastError = ERR_INVALID_PARAM;
		return false;
	}

	
	// Create the file...

	HANDLE hWrite = CreateFile(	lpszPath,GENERIC_WRITE,
								FILE_SHARE_READ,
								NULL,CREATE_NEW,	
								FILE_ATTRIBUTE_NORMAL,NULL);

	if(INVALID_HANDLE_VALUE == hWrite)
	{
		m_dwLastError = (ERROR_FILE_EXISTS == ::GetLastError()) ? 
						ERR_FILE_ALREADY_EXISTS	: 
						ERR_CREATE_FILE;
		return false;
	}


	// The hard way...

	
	// TODO: ARE ADS streams compressible?

	if(!GetAttributeToFile(llFile,ATTR_TYPE_DATA,NULL,hWrite,fCompressed))
	{
		CloseHandle(hWrite);
		DeleteFile(lpszPath);
        return FALSE;
	}

	CloseHandle(hWrite);
	return true;
}

/////////////////////////////////////////////////////////////////////

bool NTFSReader::ReadDirectory(	LONGLONG llFile, NTFSItem* pFiles, 
								PLONGLONG pcFiles)
{
	ASSERT(m_fInit);
	if(false == m_fInit)
	{
		m_dwLastError = ERR_NOT_INIT;
		return false;
	}

	if(NULL == pcFiles)
	{
		m_dwLastError = ERR_INVALID_PARAM;
		return false;
	}
	
	// Load Index Root

	ULONGLONG cbBuf = 0;
	if(!GetAttribute(	llFile,ATTR_TYPE_INDEX_ROOT,DIRECTORY_STREAM_NAME,
						NULL,&cbBuf,NULL))
	{
		return false;
	}

	BYTE *pBuf = new BYTE[(DWORD)cbBuf];
	if(NULL == pBuf)
	{
		m_dwLastError = ERR_OUT_OF_MEMORY;	
		return false;
	}

	if(!GetAttribute(	llFile,ATTR_TYPE_INDEX_ROOT,DIRECTORY_STREAM_NAME,
						pBuf,&cbBuf,NULL))
	{
		delete pBuf;
		return false;
	}


	// Parse Index Root 

	PATTR_INDEX_ROOT pair = (PATTR_INDEX_ROOT)pBuf;

	if (pair->Type != ATTR_TYPE_FILE_NAME || 
		0 == pair->Header.OffsetFirstIndexEntry)
	{
		delete pBuf;
		m_dwLastError = ERR_CANNOT_PARSE;
		return false;
	}
	
	PINDEX_ENTRY pie = (PINDEX_ENTRY)
		(((BYTE*)(&pair->Header)) + pair->Header.OffsetFirstIndexEntry);

	// Large or Small?

	LONGLONG cFiles = 0;
	BYTE *pAlloc = NULL;
	ULONGLONG cbAlloc = 0;
	BYTE *pBitmap = NULL;
	LONGLONG cbBitmap = 0;


	// LARGE => Load Index Alloc

	if(pair->Header.Flags == INDEX_HEADER_FLAG_LARGE)
	{
		if(!GetAttribute(llFile,ATTR_TYPE_INDEX_ALLOCATION,
			DIRECTORY_STREAM_NAME,NULL,&cbAlloc,NULL))
		{
			delete pBuf;
			return false;
		}

		pAlloc = new BYTE[(DWORD)cbAlloc];
		if(NULL == pAlloc)
		{
			m_dwLastError = ERR_OUT_OF_MEMORY;	
			delete pBuf;
			return false;
		}		

		if(!GetAttribute(llFile,ATTR_TYPE_INDEX_ALLOCATION,
			DIRECTORY_STREAM_NAME,pAlloc,&cbAlloc,NULL))
		{
			delete pAlloc;
			delete pBuf;
            return false;
		}

		// removed below, it seems unnecessary so far!

		//GetAttribute(llFile,ATTR_TYPE_BITMAP,
		//	DIRECTORY_STREAM_NAME,0,NULL,&cbBitmap);

		//pBitmap = new BYTE[(DWORD)cbBitmap];
		//if(NULL == pBitmap)
		//{
		//	m_dwLastError = ERR_OUT_OF_MEMORY;	
		//	return false;
		//}		

		//if(!GetAttribute(llFile,ATTR_TYPE_BITMAP,
		//	DIRECTORY_STREAM_NAME,0,pBitmap,&cbBitmap))
		//{
		//	delete pBitmap;
		//	delete pAlloc;
		//	delete pBuf;
  //          return false;
		//}
	}

	if(!RecReadDir(pie,pAlloc,(DWORD)cbAlloc,pBitmap,(DWORD)cbBitmap,
			pFiles,&cFiles))
	{
		if(pAlloc)
		{
			delete pBitmap;
			delete pAlloc;
		}

		delete pBuf;
		return false;
	}

	if(pAlloc)
	{
		delete pAlloc;
	}
	
	if(pBitmap)
	{
		delete pBitmap;
	}

	
	// Return...

	if(*pcFiles < cFiles || NULL == pFiles)
	{
		*pcFiles = cFiles;
		delete pBuf;
		m_dwLastError = ERR_BUFFER_TOO_SMALL;
		return true;
	}

	*pcFiles = cFiles;
	delete pBuf;
	return true;
}

/////////////////////////////////////////////////////////////////////

// This function does not check there is enough room in pFiles
// it is internal and expects correct size from ReadDirectory!

bool NTFSReader::RecReadDir(PINDEX_ENTRY pie, BYTE* pAlloc,DWORD cbAlloc, 
							BYTE* pBitmap, DWORD cbBitmap, NTFSItem* pFiles, 
							PLONGLONG pcFiles)
{
	if(NULL == pcFiles || NULL == pie)
	{
		m_dwLastError = ERR_INVALID_PARAM;
		return false;
	}

	LONGLONG cFiles = *pcFiles;

	while(1) // TODO: Look at some way of Bounds Checking here in 
		     // case of corrupt entries
	{
		// Parent => Recurse

		if (INDEX_ENTRY_PARENT == ((pie->IndexFlags) & INDEX_ENTRY_PARENT))
		{			
			if(NULL == pAlloc && NULL == pFiles)
			{
				ASSERT(false); // Not counting & NULL pAlloc??!! 0xBAD

				// Small Directory with INDEX_ENTRY_PARENT specified?
				// didn't think this was possible => oops...
				m_dwLastError = ERR_CANNOT_PARSE;
				return false;
			}

			
			// Read the Index Alloc Block at this VCN

			LONGLONG VCN = *(PLONGLONG)(((BYTE*)pie) + pie->Size - sizeof(LONGLONG));
			DWORD offset = (DWORD)VCN * m_cbCluster; // int ?

		/*	char szDbg[64];
			wsprintf(szDbg,"0x%I64u\n",VCN);
			OutputDebugString(szDbg);*/

			if(offset > cbAlloc) // Quick sanity bounds check
			{
				ASSERT(offset <= cbAlloc);
				m_dwLastError = ERR_CORRUPT_ATTRIBUTE;
				return false;
			}
	
			PATTR_INDEX_ALLOCATION paia = 
				(PATTR_INDEX_ALLOCATION)(&pAlloc[offset]);

			ASSERT(0x58444e49 == paia->magic);
			if(0x58444e49 != paia->magic)
			{
				m_dwLastError = ERR_CORRUPT_ATTRIBUTE;
				return false;
			}
		
			// Deal with Update Seq Numbers => These are designed to
			// detect corruption

			if(paia->SizeUpdSeq)
			{
				WORD *pUpdate = (WORD*)
					(&pAlloc[offset] + paia->offsetUpdSeq + sizeof(WORD));
				for(int i=1;i<paia->SizeUpdSeq;i++)
				{
					// TODO: Hardcoded Sector Size!!! 0x200 fix
					*(WORD*)
						(&pAlloc[offset + (i * 0x200) - sizeof(WORD)]) = 
						*pUpdate;
					pUpdate++;
				}
			}



            PINDEX_ENTRY pieNew = (PINDEX_ENTRY)
				(((BYTE*)(&paia->Header)) +
				paia->Header.OffsetFirstIndexEntry);

			if(!RecReadDir(pieNew,pAlloc,cbAlloc, pBitmap, cbBitmap,pFiles,&cFiles))
			{
				return false;
			}
		} // if(INDEX_ENTRY_PARENT == ...)


		// Standard Directory Parsing

		if(INDEX_ENTRY_LAST != (pie->IndexFlags & INDEX_ENTRY_LAST) && NULL != pFiles)
		{
			// Copy Name

			PATTR_FILE_NAME pafn = (PATTR_FILE_NAME)
				(((BYTE*)(pie)) + SIZEOF_INDEX_ENTRY);

			pFiles[cFiles].FilenameNamespace= pafn->FilenameNamespace;
			pFiles[cFiles].dwAttr =		pafn->Flags;
			pFiles[cFiles].fDeleted =	false;
			pFiles[cFiles].ftAccess =	pafn->RTime;
			pFiles[cFiles].ftCreate =	pafn->CTime;
			pFiles[cFiles].ftMod =		pafn->MTime;
			pFiles[cFiles].size =		pafn->RealSize;
			pFiles[cFiles].mftIndex =   FILE_ENTRY_FROM_FILEREF(pie->MFTRef);

			// Copy Name

			LPWSTR lpszN = (LPWSTR)(pafn + 1);
			//wcsncpy_s(pFiles[cFiles].szName,FILE_NAME_LEN-1,lpszN,_TRUNCATE);
			wcsncpy(pFiles[cFiles].szName,lpszN,FILE_NAME_LEN-1);
			pFiles[cFiles].szName[
				((pafn->cchFilename > FILE_NAME_LEN-1) ?
					FILE_NAME_LEN-1 : 
					pafn->cchFilename)] = 0;
		}

		// bail out if we're on last entry

		if(INDEX_ENTRY_LAST == (pie->IndexFlags & INDEX_ENTRY_LAST))
		{
			break;
		}

		// if not on last entry we've just parsed a valid file => increase file count

		pie = (PINDEX_ENTRY)(((BYTE*)(pie)) + pie->Size);
		cFiles++;
	}

	*pcFiles = cFiles;

	return true;
}

/////////////////////////////////////////////////////////////////////

template <class T> void AutoDeleteVector(T *pvec)
{
	for(T::iterator a = pvec->begin();a != pvec->end();a++)
	{
		delete *a;
	}

	delete pvec;
	pvec = NULL;
}

/////////////////////////////////////////////////////////////////////

// TODO: This function hasnt been tested at all!!!
bool NTFSReader::GetAttribute(	LONGLONG llFile, DWORD dwAttr, LPWSTR lpszName,
								BYTE* pBuf, PULONGLONG pcbBuf,PULONGLONG pcbRealSize)
{
	ASSERT(m_fInit);
	if(NULL == pcbBuf)
	{
		m_dwLastError = ERR_INVALID_PARAM;
		return false;
	}

	
	vector<GetAttrHdrs_Ret*> *pRet = new vector<GetAttrHdrs_Ret*>;
	ASSERT(pRet);
	if(NULL == pRet)
	{
		m_dwLastError = ERR_OUT_OF_MEMORY;
		return false;
	}

	if(!GetAttributeHeaders(llFile,dwAttr,lpszName,pRet))
	{
		AutoDeleteVector(pRet);
		return false;
	}
	

	// Parse Each Attribute Header 

	BYTE* pEntry = new BYTE[HARDCODED_MFT_ENTRY_SIZE];
	ASSERT(pEntry);
	if(NULL == pEntry)
	{
		AutoDeleteVector(pRet);
		m_dwLastError = ERR_OUT_OF_MEMORY;
		return false;
	}

	
	// Extract Data...

	ULONGLONG AllocBufTotal = 0; // for returning when a count is req...
	ULONGLONG RealBufTotal = 0;

	for(vector<GetAttrHdrs_Ret*>::iterator attr = pRet->begin();
		attr != pRet->end();attr++)
	{
		if(!LoadMFTEntry((*attr)->MFTEntry,pEntry))
		{
			ASSERT(false);
			AutoDeleteVector(pRet);
			delete pEntry;
			return false;
		}

		PATTR_HEADER pah = (PATTR_HEADER)(pEntry + (*attr)->offsetAttrHdr);

		ULONGLONG length = (pah->fNonResident) ? 
				pah->extra.non_resident.RealSize : 
				pah->extra.resident.AttrLength;

		ULONGLONG AllocLength = (pah->fNonResident) ? 
				pah->extra.non_resident.AllocSize : 
				pah->extra.resident.AttrLength;


		AllocBufTotal += AllocLength;
		RealBufTotal =+ length;
	}

	// Enough Space? No Return The Required Amount

	if(*pcbBuf < AllocBufTotal || NULL == pBuf)
	{
		*pcbBuf = AllocBufTotal;
			
		if(pcbRealSize)
		{
			*pcbRealSize = RealBufTotal;
		}

		AutoDeleteVector(pRet);
		delete pEntry;
		m_dwLastError = ERR_BUFFER_TOO_SMALL;
		return true;
	}


	// Extract...

	ULONGLONG WriteBufOffset = 0;
	for(vector<GetAttrHdrs_Ret*>::iterator attr = pRet->begin();
		attr != pRet->end();attr++)
	{
		if(!LoadMFTEntry((*attr)->MFTEntry,pEntry))
		{
			ASSERT(false);
			AutoDeleteVector(pRet);
			delete pEntry;
			return false;
		}

		PATTR_HEADER pah = (PATTR_HEADER)(pEntry + (*attr)->offsetAttrHdr);

		if(	!m_drhFile.SetAttributeHeader(pah))
		{
			ASSERT(false);
			AutoDeleteVector(pRet);
			delete pEntry;
			return false;
		}

		// TODO: we are saying it is not compressed here always...
		// TODO: Truncation into len here... check
		// FIX!

		_U64 len = (*pcbBuf - WriteBufOffset);
		if(!m_drhFile.GetAllDataToMem(&pBuf[WriteBufOffset],&len,false))
		{
			ASSERT(false);
			AutoDeleteVector(pRet);
			delete pEntry;
			return false;
		}

		ULONGLONG length = (pah->fNonResident) ? 
				pah->extra.non_resident.RealSize : 
				pah->extra.resident.AttrLength;


		// Should have least have written realsize amount
		// of data...

		if(len < length)
		{
			ASSERT(false);
			AutoDeleteVector(pRet);
			delete pEntry;
			return false;
		}

		WriteBufOffset += length;
	}


	if(pcbRealSize)
	{
		*pcbRealSize = RealBufTotal;
	}

	AutoDeleteVector(pRet);
	delete pEntry;
	return true;
}

/////////////////////////////////////////////////////////////////////

bool NTFSReader::GetAttributeToFile(LONGLONG llFile, DWORD dwAttr,
									LPWSTR lpszName,HANDLE hWrite,
									bool fCompressed)
{
	ASSERT(m_fInit);

	vector<GetAttrHdrs_Ret*> *pRet = new vector<GetAttrHdrs_Ret*>;
	ASSERT(pRet);
	if(NULL == pRet)
	{
		m_dwLastError = ERR_OUT_OF_MEMORY;
		return false;
	}

	if(!GetAttributeHeaders(llFile,dwAttr,lpszName,pRet))
	{
		AutoDeleteVector(pRet);
		return false;
	}
	

	// Parse Each Attribute Header 

	BYTE* pEntry = new BYTE[HARDCODED_MFT_ENTRY_SIZE];
	ASSERT(pEntry);
	if(NULL == pEntry)
	{
		AutoDeleteVector(pRet);
		m_dwLastError = ERR_OUT_OF_MEMORY;
		return false;
	}

	
	// Extract Data...

	bool fFirst = true;
	LARGE_INTEGER liTotSize;

	for(vector<GetAttrHdrs_Ret*>::iterator attr = pRet->begin();
		attr != pRet->end();attr++)
	{
		if(!LoadMFTEntry((*attr)->MFTEntry,pEntry))
		{
			AutoDeleteVector(pRet);
			delete pEntry;
			return false;
		}

		PATTR_HEADER pah = (PATTR_HEADER)(pEntry + (*attr)->offsetAttrHdr);


		if(fFirst)
		{
			liTotSize.QuadPart = (pah->fNonResident) ? 
				pah->extra.non_resident.RealSize : 
				pah->extra.resident.AttrLength;

			fFirst = false;
		}

		// Found => Extract Data

		if(!m_drhFile.SetAttributeHeader(pah))
		{
			AutoDeleteVector(pRet);
			delete pEntry;
			return false;
		}

		if(!m_drhFile.GetAllDataToFile(hWrite,fCompressed))
		{
			AutoDeleteVector(pRet);
			delete pEntry;
			return false;
		}



		//ULONGLONG cbBuf = 0;
		//if(!m_drhFile.GetAllDataToMem(NULL,&cbBuf,fCompressed) && 
		//	ERR_BUFFER_TOO_SMALL != m_drhFile.GetLastError())
		//{
		//	ASSERT(false);
		//	AutoDeleteVector(pRet);
		//	delete pEntry;
		//	return false;
		//}



		//BYTE* pBuf = new BYTE[(SIZE_T)cbBuf];
		//ASSERT(pBuf);
		//if(NULL == pBuf)
		//{
		//	AutoDeleteVector(pRet);
		//	delete pEntry;
		//	return false;
		//}

		//if(!m_drhFile.GetAllDataToMem(pBuf,&cbBuf,fCompressed))
		//{
		//	ASSERT(false);
		//	AutoDeleteVector(pRet);
		//	delete pBuf;
		//	delete pEntry;
		//	return false;
		//}


		//DWORD dwWritten;
		//if(	!WriteFile(hWrite,pBuf,cbBuf,&dwWritten,NULL) ||
		//	dwWritten != cbBuf)
		//{
		//	ASSERT(false);
		//	AutoDeleteVector(pRet);
		//	delete pBuf;
		//	delete pEntry;
		//	return false;
		//}

		//delete pBuf;
	}


	// Finally set the end of the file... This is determined from the
	// realsize of the first AttributeHeader...


	if(	!SetFilePointerEx(hWrite,liTotSize,NULL,FILE_BEGIN) ||
		!SetEndOfFile(hWrite))
	{
		AutoDeleteVector(pRet);
		delete pEntry;
		return false;
	}

	AutoDeleteVector(pRet);
	delete pEntry;
	return true;
}

/////////////////////////////////////////////////////////////////////

// IMPORTANT FUNCTION
// Gets Attribute Data Matching Params
//
// Parameters:
// llFile:		The File Number
// dwAttr:		The Attribute Type
// lpszName:	if you want a named attribute, can be NULL
// dwOccurence: if you want a specific occurence, 0=1st,1=2nd etc..
// pBuf:		Buffer to store this attributes data, can be NULL
// cbLength:	Length in bytes of this Attribute, if pBuf == NULL or
//				not long enough *pcbBuf will contain needed size and
//				this function will return true with last error = 
//				ERR_BUFFER_TOO_SMALL. Otherwise returns number
//				of bytes copied. CANNOT be NULL.
//
// This Function hides presence on $Attribute_LIST => if you want this
// Attributes data parse it yourself!!

// TODO:	fix hardcoded size of MFT Entry
// TODO:	Is it possible to miss an attribute which 
//			has one instance resident and one non-resident?
//

// TODO: Replace thjis system of returning a vector with a fn callback system...

bool NTFSReader::GetAttributeHeaders(	LONGLONG llFile, DWORD dwAttr, 
										LPWSTR lpszName,vector<GetAttrHdrs_Ret*> *pvecRet)
{
	ASSERT(m_fInit);
	ASSERT(pvecRet);

	BYTE* pEntry = new BYTE[HARDCODED_MFT_ENTRY_SIZE];
	ASSERT(pEntry);
	if(NULL == pEntry)
	{
		m_dwLastError = ERR_OUT_OF_MEMORY;
		return false;
	}
	
	if(!LoadMFTEntry(llFile,pEntry))
	{
		delete pEntry;
		return false;
	}

	PMFT_FILE_ENTRY pmfe = (PMFT_FILE_ENTRY)pEntry;

	
	// Have a look for the Attribute

	bool fMatch = false;
	BYTE* pEndMFTEntry = pEntry + pmfe->used_size - SIZEOF_ATTR_HEADER_RESIDENT;
	PATTR_HEADER pah = (PATTR_HEADER)(pEntry + pmfe->offsetFirstAttr);


	// While we haven't reached the last Attribute AND we are still
	// within the bounds of this File MFT Entry...

	while(	0xFFFFFFFF != pah->Type && ((BYTE*)pah < pEndMFTEntry))
	{
		// Attribute List? Requires Special Processing...
		
		if(ATTR_TYPE_ATTRIBUTE_LIST == pah->Type) 
		{
			if(pah->fNonResident)
			{
				// Will never occur...
				ASSERT(false);
				m_dwLastError = ERR_CANNOT_PARSE;
				delete pEntry;
				return false;
			}


			// Move Past Header & Name to get our first AttributeList Entry

			PATTRIBUTE_LIST_ENTRY pale = (PATTRIBUTE_LIST_ENTRY)
				(((BYTE*)pah) +	SIZEOF_ATTR_HEADER_RESIDENT + 
				(pah->NameLength * sizeof(WCHAR)));


			// Entries must stay within this bound...
			
			BYTE* pEndAttrList =	((BYTE*)pale) + pah->Length - 
									SIZEOF_ATTRIBUTE_LIST_ENTRY;


			// While we still have Attributes... Check for matches...

			while((BYTE*)pale < pEndAttrList)
			{			
				if(AttrListEntryMatch(pale,dwAttr,lpszName))
				{
					if(	FILE_ENTRY_FROM_FILEREF(llFile) != 
						FILE_ENTRY_FROM_FILEREF(pale->BaseFileRef))
					{
						if(!GetAttributeHeaders(FILE_ENTRY_FROM_FILEREF(pale->BaseFileRef),
												dwAttr,lpszName,pvecRet))
						{
							delete pEntry;
							return false;
						}

						fMatch = true;
					}

					// No need to deal with 'attr resident in this MFT entry' case
					// we will fall through and find it...
				}

				pale = (PATTRIBUTE_LIST_ENTRY)(((BYTE*)pale) + pale->RecordLength);
			}	
		}
		else // Not an attribute list... Standard processing
		{
			if(AttrHdrMatch(pah,dwAttr,lpszName))
			{
				// Found? -> Add To List...

				int offset2AttrHdr = (int)(((BYTE*)pah) - pEntry);
				pvecRet->push_back(new GetAttrHdrs_Ret(llFile,offset2AttrHdr));
				fMatch = true;
			}
		}

		// Next Attribute...

		pah = (PATTR_HEADER)(((BYTE*)pah) + pah->Length);
	}


	// Did we find our attribute?

	if(!fMatch)
	{
		m_dwLastError = ERR_ATTR_NOT_AVAILABLE;
		delete pEntry;
		return false;
	}

	delete pEntry;
	return true;
}

/////////////////////////////////////////////////////////////////////

bool NTFSReader::AttrListEntryMatch( PATTRIBUTE_LIST_ENTRY pale,
									 DWORD dwAttr,LPWSTR lpszName)
{
	if(pale->Type == dwAttr)
	{
		if(lpszName) // need a name match?
		{
			LPWSTR pName = (LPWSTR)(((BYTE*)pale) + pale->OffsetName);

			if(!wcsncmp(lpszName,pName,pale->NameLength))
			{
				return true;
			}
		}
		else
		{
			return true;
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////

bool NTFSReader::AttrHdrMatch(PATTR_HEADER pah, DWORD dwAttr, LPWSTR lpszName)
{
	if(pah->Type == dwAttr)
	{
		if(lpszName) // need a name match?
		{
			LPWSTR pName = (LPWSTR)(((BYTE*)pah) + pah->OffsetName);

			if(!wcsncmp(lpszName,pName,pah->NameLength))
			{
				return true;
			}
		}
		else if(pah->NameLength == 0) // only match unnamed to unnamed*/
		{
			return true;
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////

bool NTFSReader::LoadMFTEntry(LONGLONG llMFTEntry, BYTE* pBuf)
{
	ASSERT(m_fInit);
	ASSERT(pBuf);

	// Caching 4 Performance...

	if(llMFTEntry != m_llGetAttributeLastFile)
	{
		if(!GetMFTFileEntry(llMFTEntry,m_pGetAttributeMemCache,
							HARDCODED_MFT_ENTRY_SIZE))
		{
			m_llGetAttributeLastFile = -1;
			return false;
		}
		
		// Sanity Check

		PMFT_FILE_ENTRY pmfe = (PMFT_FILE_ENTRY)m_pGetAttributeMemCache;
		if(0 == pmfe->offsetFirstAttr)
		{
			m_llGetAttributeLastFile = -1;
			m_dwLastError = ERR_ATTR_NOT_AVAILABLE;
			return false;
		}

		m_llGetAttributeLastFile = llMFTEntry;
	}

	CopyMemory(pBuf,m_pGetAttributeMemCache,HARDCODED_MFT_ENTRY_SIZE);

	return true;
}

/////////////////////////////////////////////////////////////////////
// TODO: Error if outside file range
// TODO: Check Bounds

bool NTFSReader::GetMFTFileEntry(LONGLONG llFile, BYTE *pBuf, 
								 DWORD dwBufLen)
{
	if(!m_fInit)
	{
		//TODO: SetLastError()
		return false;
	}

	if(NULL == pBuf)
	{
		m_dwLastError = ERR_INVALID_PARAM;
		return false;
	}

	if(llFile == m_llGetAttributeLastFile)
	{
		CopyMemory(pBuf,m_pGetMFTEntryCache,
			((dwBufLen < HARDCODED_MFT_ENTRY_SIZE) ? 
				dwBufLen : 
				HARDCODED_MFT_ENTRY_SIZE));

		return true;
	}

	LARGE_INTEGER lrgOffset;
	lrgOffset.QuadPart = llFile * HARDCODED_MFT_ENTRY_SIZE;

	LONGLONG llBufLen = dwBufLen;
    if(!m_drhMFT.GetData(lrgOffset.QuadPart,HARDCODED_MFT_ENTRY_SIZE,pBuf,&llBufLen))
	{
		m_dwLastError = ERR_INVALID_MFT_ENTRY;
		return false;
	}

	PMFT_FILE_ENTRY pmfe = (PMFT_FILE_ENTRY)pBuf;
	if(pmfe->Magic != MFT_ENTRY_MAGIC)
	{
		m_dwLastError = ERR_INVALID_MFT_ENTRY;
		return false;
	}

	// Deal with Update Seq Numbers => These are designed to 
	// detect corruption

	if(pmfe->SizeUpdateSeq)
	{
		WORD *pUpdate = (WORD*)(pBuf + pmfe->OffsetUpdateSeq 
			+ sizeof(WORD));
		for(int i=1;i<pmfe->SizeUpdateSeq;i++)
		{
			// TODO: Hardcoded Sector Size!!! 0x200 fix
			*(WORD*)(pBuf + (i * 0x200) - sizeof(WORD)) = *pUpdate;
			pUpdate++;
		}
	}

	return true;
}

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////

//DataHelper::DataHelper()
//{
//	m_pah = NULL;
//	m_hFile = NULL;
//	m_llBeginFileOffset = 0;
//	m_cbCluster = 0;
//	m_fInit = false;
//	m_dwLastError = 0;
//}
//
///////////////////////////////////////////////////////////////////////
//
//bool DataHelper::Init(	HANDLE hFile,LONGLONG llBeginFileOffset,
//						DWORD cbCluster,PATTR_HEADER pah)
//{
//	m_hFile = hFile;
//	m_llBeginFileOffset = llBeginFileOffset;
//	m_cbCluster = cbCluster;
//	m_pah = pah;
//	m_fInit = true;
//	return true;
//}
//
///////////////////////////////////////////////////////////////////////
//
//bool DataHelper::GetAllData(BYTE* pBuf, PULONGLONG pcbBuf)
//{
//	if(!m_fInit)
//	{
//		m_dwLastError = ERR_NOT_INIT;
//		return false;
//	}
//
//	DataRunHelper drh;
//	if(!drh.Init(m_hFile, m_llBeginFileOffset, m_cbCluster,m_pah))
//	{
//		return false;
//	}
//
//	return drh.GetAllData(pBuf,pcbBuf);
//
//}
//
///////////////////////////////////////////////////////////////////////
//
////
////
////	if(m_pah->fNonResident)
////	{
////		BYTE* pRuns = ((BYTE*)m_pah) + m_pah->extra.non_resident.OffsetDataRuns;
////		DataRunHelper drh;
////		
////		if(	!drh.Init(m_hFile,m_llBeginFileOffset,m_cbCluster) || 
////			!drh.AddRawRun(pRuns,m_pah->Length - m_pah->extra.non_resident.OffsetDataRuns))
////		{
////			m_dwLastError = ERR_CANNOT_PARSE;
////			return false;
////		}
////
////		// TODO: Size, what if too big???
////
////		ULONGLONG length;
////		drh.GetAllData(NULL,&length);
////
////		if(*pcbBuf < length || NULL == pBuf)
////		{
////			*pcbBuf = length;
////			m_dwLastError = ERR_BUFFER_TOO_SMALL;
////			return false;
////		}
////
////		if(!drh.GetAllData(pBuf,pcbBuf))
////		{
////			m_dwLastError = ERR_CANNOT_PARSE;
////			return false;
////		}
////	}
////	else
////	{
////		BYTE* pTmp = ((BYTE*)m_pah);
////
////		// Move Past Header & name
////
////		pTmp += SIZEOF_ATTR_HEADER_RESIDENT + (m_pah->NameLength * sizeof(WCHAR));
////
////		if(*pcbBuf < m_pah->extra.resident.AttrLength || NULL == pBuf)
////		{
////			*pcbBuf = m_pah->extra.resident.AttrLength;
////			m_dwLastError = ERR_BUFFER_TOO_SMALL;
////			return false;
////		}
////
////		*pcbBuf = m_pah->extra.resident.AttrLength;
////		CopyMemory(pBuf,pTmp,m_pah->extra.resident.AttrLength);
////	}
////
////	return true;
////}
//
///////////////////////////////////////////////////////////////////////
//
//// TODO: Check for insufficient disk space with each write
//
//bool DataHelper::GetAllDataToFile(HANDLE hWrite,bool fCompressed)
//{
//	if(!m_fInit)
//	{
//		m_dwLastError = ERR_NOT_INIT;
//		return false;
//	}
//
//	DataRunHelper drh;
//	if(!drh.Init(m_hFile, m_llBeginFileOffset, m_cbCluster, m_pah))
//	{
//		return false;
//	}
//
//	return drh.GetAllDataToFile(hWrite, fCompressed);
//}
//
///////////////////////////////////////////////////////////////////////
//	//if(m_pah->fNonResident)
//	//{
//	//	BYTE* pRuns = ((BYTE*)m_pah) + m_pah->extra.non_resident.OffsetDataRuns;
//	//	DataRunHelper drh;
//
//	//	if(	!drh.Init(m_hFile,m_llBeginFileOffset,m_cbCluster) ||
//	//		!drh.AddRawRun(pRuns,m_pah->Length - m_pah->extra.non_resident.OffsetDataRuns))
//	//	{
//	//		m_dwLastError = ERR_CANNOT_PARSE;
//	//		return false;
//	//	}
//
//	//	if(!drh.GetAllDataToFile(hWrite,fCompressed))
//	//	{
//	//		m_dwLastError = ERR_CANNOT_PARSE;
//	//		return false;
//	//	}
//	//}
//	//else
//	//{
//	//	BYTE* pTmp = ((BYTE*)m_pah);
//
//	//	// Move Past Header & name
//
//	//	pTmp += SIZEOF_ATTR_HEADER_RESIDENT + (m_pah->NameLength * sizeof(WCHAR));
//
//	//	DWORD dwWritten;
//
//	//	if(	!WriteFile(hWrite,pTmp,m_pah->extra.resident.AttrLength,&dwWritten,NULL) ||
//	//		dwWritten != m_pah->extra.resident.AttrLength)
//	//	{
//	//		m_dwLastError = ERR_WRITING_FILE;
//	//		return false;
//	//	}
//	//}
//
//	//return true;
//
////}

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////

/* TEST CODE 

		BYTE err1[] = {0x21,0x18,0x34,0x56}; // no zero terminator
		BYTE err2[] = {0x21,0x18,0x34,0x56,0x21}; // no following legth or offset
		BYTE err3[] = {0x00};

		BYTE test1[] = {0x21,0x18,0x34,0x56,0x00};
		BYTE test2[] = {0x21,0x18,0x34,0x56,0x00}; // same again but tests concatenation
		BYTE test3[] = {0x31,0x38,0x73,0x25,0x34,0x32,0x14,0x01,0xE5,0x11,0x02,0x31,0x42,0xAA,0x00,0x03,0x00}; // normal fragmented
		BYTE test4[] = {0x11,0x30,0x60,0x21,0x10,0x00,0x01,0x11,0x20,0xE0,0x00}; // normal, scrambled
		BYTE test5[] = {0x11,0x30,0x20,0x01,0x60,0x11,0x10,0x30,0x00}; // Sparse, Unfragmented File

		DataRunHelper *pdrh = new DataRunHelper();

		pdrh->	(test5,sizeof(test5));
		//pdrh->AddRawRun(test2,sizeof(test2)); // concat test
		
		delete pdrh;

*/

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////

DataRunHelper::DataRunHelper()
{
	m_pRuns = NULL;
	m_cRuns = 0;
	m_hFile = NULL;
	m_llBeginOffset = 0;
	m_cbCluster = 0;
	m_fInit = false;
	m_pDecompBuf = NULL;
	m_pCompBuf = NULL;
	m_dwLastError = 0;
	m_pah = NULL;
}

/////////////////////////////////////////////////////////////////////

DataRunHelper::~DataRunHelper()
{
	Clear();
}

/////////////////////////////////////////////////////////////////////

bool DataRunHelper::Init(HANDLE hFile, LONGLONG llBeginOffset, 
						 DWORD cbCluster)
{
	ASSERT(!m_fInit);
	if(m_fInit)
	{
		return false;
	}

	m_hFile = hFile;
	m_llBeginOffset = llBeginOffset;
	m_cbCluster = cbCluster;
	m_fInit = true;
	m_dwLastError = 0;
	m_pah = NULL;
	return true;
}
/////////////////////////////////////////////////////////////////////

void DataRunHelper::Clear()
{
	if(m_pRuns)
	{
		delete []m_pRuns;
	}

	m_pRuns = NULL;
	m_cRuns = 0;
	m_hFile = NULL;
	m_llBeginOffset = 0;
	m_cbCluster = 0;
	m_fInit = false;
	m_dwLastError = 0;

	if(m_pDecompBuf)
	{
		delete m_pDecompBuf;
	}
	m_pDecompBuf = NULL;


	if(m_pCompBuf)
	{
		delete m_pCompBuf;
	}

	m_pCompBuf = NULL;
	m_pah = NULL;
}

/////////////////////////////////////////////////////////////////////

bool DataRunHelper::SetAttributeHeader(PATTR_HEADER pah)
{
	ASSERT(m_fInit);
	if(false == m_fInit)
	{
		m_dwLastError = ERR_NOT_INIT;
		return false;
	}

	ASSERT(pah);
	if(NULL == pah)
	{
		m_dwLastError = ERR_INVALID_PARAM;
		return false;
	}

	// Clear the previous runs...

	if(m_pRuns)
	{
		delete []m_pRuns;
	}

	m_pRuns = NULL;
	m_cRuns = 0;

	if(pah->fNonResident)
	{
		BYTE* pRuns = ((BYTE*)pah) + pah->extra.non_resident.OffsetDataRuns;

		if(	!AddRawRun(pRuns,pah->Length - pah->extra.non_resident.OffsetDataRuns))
		{
			m_dwLastError = ERR_CANNOT_PARSE;
			return false;
		}
	}

	m_pah = pah;

	return true;
}

/////////////////////////////////////////////////////////////////////

//
//bool DataRunHelper::GetDataFile(HANDLE hFile, LONGLONG llBeginOffset,
//								DWORD cbCluster,HANDLE hOut, 
//								LONGLONG llcbTruncOutFile)
//{
//	if(NULL == m_pRuns)
//	{
//		return true;
//	}
//
//	// Copy Data in...
//
//	BYTE *pBuf = new BYTE[cbCluster];
//	if(NULL == pBuf)
//	{
//		return false;
//	}
//
//	DWORD cbRead;
//	LARGE_INTEGER liTmp;
//
//	for(int i=0;i<m_cRuns;i++)
//	{
//		LONGLONG cbWant = m_pRuns[i].length * cbCluster;
//		LONGLONG offset = m_pRuns[i].offset * cbCluster + llBeginOffset;
//
//		// Sparse => zero fill
//
//		if(m_pRuns[i].fSparse)
//		{
//			ZeroMemory(pBuf,cbCluster);
//
//			for(int j=0;j<m_pRuns[i].length;j++)
//			{
//				if (!WriteFile(hOut,pBuf,cbCluster,&cbRead,NULL) || 
//					cbCluster != cbRead)
//				{
//					delete pBuf;
//					return false;
//				}
//			}
//		}
//		else
//		{	// Copy across in clusters
//
//			liTmp.QuadPart = offset;
//
//			if (!SetFilePointerEx(hFile,liTmp,NULL,FILE_BEGIN))
//			{
//				delete pBuf;
//				return false;
//			}
//
//			BYTE *pBuf = new BYTE[cbCluster];
//
//			for(int j=0;j<m_pRuns[i].length;j++)
//			{
//				if (!ReadFile(hFile,pBuf,cbCluster,&cbRead,NULL) ||	// Read
//					cbRead != cbCluster)							// Verify Read
//				{
//					delete pBuf;
//					return false;
//				}
//
//				if (!WriteFile(hOut,pBuf,cbCluster,&cbRead,NULL) || 
//					cbCluster != cbRead)
//				{
//					delete pBuf;
//					return false;
//				}
//			}
//		}
//
//	}
//
//	delete pBuf;
//
//	// Truncate hOut to actual size specified
//
//	liTmp.QuadPart = llcbTruncOutFile;
//	if (!SetFilePointerEx(hOut,liTmp,NULL,FILE_BEGIN) || 
//		!SetEndOfFile(hOut))
//	{
//		return false;
//	}
//
//	return true;
//}

/////////////////////////////////////////////////////////////////////

bool DataRunHelper::VCNToLCN(_U64 VCN, _U64 *pLCN, bool *pfIsSparse)
{
	ASSERT(m_fInit);
	if(false == m_fInit)
	{
		m_dwLastError = ERR_NOT_INIT;
		return false;
	}

	ASSERT(m_pRuns);

	// Find the LCN for this VCN

	bool fFound = false;
	_U64 lenAccum = 0;

	int j=0;
	for(;j<m_cRuns;j++)
	{
		// TODO: Unsure about limits in this if statement... verify
		if(VCN >= lenAccum && VCN < lenAccum + m_pRuns[j].length)
		{
			fFound = true;
			break;
		}

		lenAccum += m_pRuns[j].length;
	}

	if(false == fFound)
	{
		return false;
	}

	(*pLCN) = m_pRuns[j].offset + (VCN - lenAccum);
	
	(*pfIsSparse) = m_pRuns[j].fSparse;
	return true;
}

///////////////////////////////////////////////////////////////////////

bool DataRunHelper::GetData(LONGLONG llOffsetFrom,LONGLONG cbReq,
							BYTE *pBuf,PLONGLONG pBufLen)
{
	ASSERT(m_fInit);
	if(false == m_fInit)
	{
		m_dwLastError = ERR_NOT_INIT;
		return false;
	}

	if(NULL == pBufLen)
	{
		return false;
	}

	if(NULL == m_pRuns)
	{
		(*pBufLen) = 0;
		return true;
	}

	// which run???

	LONGLONG VCNStart = llOffsetFrom / m_cbCluster;
	int StartOffset = (int)(llOffsetFrom % m_cbCluster);

	LONGLONG VCNEnd = (llOffsetFrom + cbReq) / m_cbCluster;
	int EndOffset = (int)((llOffsetFrom + cbReq) % m_cbCluster);

	int CurrentOffset = 0;

	BYTE *pTmp = new BYTE[m_cbCluster];
	if(NULL == pTmp)
	{
		return false;
	}

	for(_U64 i = VCNStart;i<VCNEnd+1;i++)
	{
		bool fIsSparse;
		DWORD cbRead;
		LARGE_INTEGER liTmp;
		_U64 LCN;

		if(!VCNToLCN(i,&LCN,&fIsSparse))
		{
			delete pTmp;
			return false;
		}

		if(fIsSparse)
		{	
			ZeroMemory(pTmp,m_cbCluster);
		}
		else
		{
			liTmp.QuadPart = (LCN * m_cbCluster) + m_llBeginOffset;

			if (!SetFilePointerEx(m_hFile,liTmp,NULL,FILE_BEGIN) ||	// Move To Loc
				!ReadFile(m_hFile,pTmp,m_cbCluster,&cbRead,NULL)||		// Read
				cbRead != m_cbCluster)								// Verify Read
			{
				delete pTmp;
				return false;
			}
		}

		// Copy

		if(i == VCNStart) // Start
		{
			DWORD dwLength = m_cbCluster-StartOffset;
			if(dwLength > *pBufLen)
			{
				dwLength = (DWORD)*pBufLen;
			}

			CopyMemory(pBuf,&pTmp[StartOffset],dwLength);
			CurrentOffset += dwLength;
		}
		else if(i == VCNEnd) // End
		{
			if(CurrentOffset + EndOffset > (*pBufLen))
			{
				delete pTmp;
				return false;
			}

			CopyMemory(pBuf,&pTmp[CurrentOffset],EndOffset);
			CurrentOffset += EndOffset;
		}
		else // middle
		{
			if(CurrentOffset + m_cbCluster > (*pBufLen))
			{
				delete pTmp;
				return false;
			}

			CopyMemory(pBuf,&pTmp[CurrentOffset],m_cbCluster);
			CurrentOffset += m_cbCluster;
		}
	}
	
	(*pBufLen) = CurrentOffset;
	delete pTmp;
	return true;
}

/////////////////////////////////////////////////////////////////////

bool DataRunHelper::GetAllDataToMem(BYTE *pBuf, _U64 *pcbBuf,
									bool fCompressed)
{
	return GetAllData(NULL,pBuf,pcbBuf,fCompressed,false);
}

/////////////////////////////////////////////////////////////////////

bool DataRunHelper::GetAllDataToFile(HANDLE hWrite, bool fCompressed)
{
	return GetAllData(hWrite,NULL,0,fCompressed,true);
}

/////////////////////////////////////////////////////////////////////

bool DataRunHelper::GetAllData(	HANDLE hWrite, BYTE* pBuf, 
								_U64 *pcbBuf, bool fCompressed,
								bool fFile)
{
	ASSERT(m_fInit);
	if(false == m_fInit)
	{
		m_dwLastError = ERR_NOT_INIT;
		return false;
	}

	if(NULL == m_pah)
	{
		m_dwLastError = ERR_INVALID_PARAM;
		return false;
	}

	if(!fFile)
	{
		ASSERT(pcbBuf);
		if(NULL == pcbBuf)
		{
			m_dwLastError = ERR_INVALID_PARAM;
			return false;
		}
	}

	// Resident? -> Easy...

	if(!m_pah->fNonResident)
	{
		BYTE* pTmp = ((BYTE*)m_pah);

		// Move Past Header & name

		pTmp += SIZEOF_ATTR_HEADER_RESIDENT + (m_pah->NameLength * sizeof(WCHAR));


		if(fFile)
		{
			DWORD dwWritten;
			if(	!WriteFile(hWrite,pTmp,m_pah->extra.resident.AttrLength,&dwWritten,NULL) ||
				dwWritten != m_pah->extra.resident.AttrLength)
			{
				m_dwLastError = ERR_WRITING_FILE;
				return false;
			}
		}
		else
		{
			SIZE_T size = m_pah->extra.resident.AttrLength;
			if(0 == size)
			{
				*pcbBuf = size;
				return true;
			}

			if(NULL == pBuf || (*pcbBuf) < size)
			{
				*pcbBuf = size;
				m_dwLastError = ERR_BUFFER_TOO_SMALL;
				return false;
			}
			
			CopyMemory(pBuf,pTmp,size);
			*pcbBuf = size;
		}

		return true;
	}

	// Non Resident...
	// Uncompressed? 


	_U64 buf_offset = 0;

	if(!fCompressed)
	{
		// Just add each run to the file in standard fashion...

		for(int i=0;i<m_cRuns;i++)
		{
			_U64 buf_delta = 0;
			if(!fFile)
			{
				if(NULL != pBuf || (*pcbBuf) > buf_offset)
				{
					buf_delta = (*pcbBuf) - buf_offset;
				}
			}

			if(!GetSingleUncompressedRun(hWrite, &pBuf[buf_offset], &buf_delta, i, fFile) &&
				ERR_BUFFER_TOO_SMALL != GetLastError())
			{
				ASSERT(false);
				return false;
			}

			if(!fFile)
			{
				buf_offset += buf_delta;
			}
		}

		if(!fFile)
		{
			if(NULL == pBuf || ((*pcbBuf) < buf_offset))
			{
				*pcbBuf = buf_offset;
				m_dwLastError = ERR_BUFFER_TOO_SMALL;
				return false;
			}

			*pcbBuf = buf_offset;
		}

		return true;
	}


	// Compressed... Compression is done in 16 Cluster Chunks.
	// This will be made up of a real run <= 16 & and a 
	// sparse filler run, which will just round the block upto 16.
	
	// if a block is made up of 16 real blocks, then the chunk is completely
	// uncompressed... easy

	// A sparse run can exceed the stated 16 max chunk, in this case it will  
	// be a multiple of 16 and the last 16 clusters of it are not actually
	// sparse data but an indicator of the end of this chunk. 
	// Most times though the sparse run is just a filler to round the chunk upto
	// exactly 16 chunks. see below...

	// the real block run, has to be processed in max 16 cluster blocks. 
	// clusters are independantly compressed, all exactly one cluster in size 
	// decompressed except the final cluster which maybe any size <= 1 cluster.

	// these independent compressed clusters are all concatenated together in the 
	// real block run. if you want to find a particular cluster in the block, you 
	// must parse each block in turn. 
	// You do not have to decompress each one because, there is a 2 byte header before
	// each cluster, indicating the compressed length of this cluster. 
	// using this you can skip to the next cluster and so on...


	// just a quick sanity check. Make sure length is divisible by 16...

	LONGLONG totLen = 0;
	for(int i=0;i<m_cRuns;i++)
	{
		totLen += m_pRuns[i].length;
	}

	if((totLen & 0xF) != 0)
	{
		ASSERT(false);
		return false;
	}


	// Compression Buffer Allocated?
	
	if(NULL == m_pCompBuf)
	{
		m_pCompBuf = new _U8[m_cbCluster << 4]; // Buffer for compressed Cluster				
		ASSERT(m_pCompBuf);
		if(NULL == m_pCompBuf)
		{
			return false;
		}
	}


	// Keep track of how many clusters we have processed. 
	// rolls over @ 16 == 0

	DWORD cClustersPx = 0;

	for(int runIndex = 0;runIndex< m_cRuns;runIndex++)
	{
		// Sparse? easily dealt with...

		if(	m_pRuns[runIndex].fSparse)
		{
			// This sparse run, if it occurs in the middle of a compression
			// unit (cClussters != 0) and if greater than 16, will have its 
			// last 16 clusters used as a termination marker... the rest will 
			// be actual sparse, i.e. zero filled data...

			// else if @ start of compression unit it is actually a full sparse run
            
			
			// So how many clusters would this sparse run fill our current
			// clusters out to?

			LONGLONG totClstrs = cClustersPx +  m_pRuns[runIndex].length;

			// Sanity check this is divisible by 16, should be, right?

			ASSERT(!(totClstrs & 0xF));
			if(0xF & totClstrs)
			{
				ASSERT(false);
				return false;
			}
			
			
			// Subtract 16 to find out how many sparse clusters we have to write...
			// Only if in middle of a compression unit...

			if(0 != cClustersPx)
			{
				totClstrs -= 16; 
			}

			
			// Fill out with zeros...

			ZeroMemory(m_pCompBuf,m_cbCluster);
			for(int i=0;i<totClstrs;i++)
			{
				if(fFile)
				{
					DWORD dwWritten = 0;
					DWORD dwWrite = m_cbCluster;
					if(	!WriteFile(hWrite,m_pCompBuf,dwWrite,&dwWritten,NULL) ||
						dwWrite != dwWritten)
					{
						return false;
					}
				}
				else
				{
					if(pBuf && (*pcbBuf) > (buf_offset + m_cbCluster))
					{
						CopyMemory(&pBuf[buf_offset],m_pCompBuf,m_cbCluster);
					}
					
					buf_offset += m_cbCluster;
				}
			}

			cClustersPx = 0; // Reset
			continue;
		}

		
		// Real Data -> Process 16 clusters @ a time

		LONGLONG lengthRunLeft = m_pRuns[runIndex].length;
		LONGLONG offsetInRun = m_pRuns[runIndex].offset;
		
		while(lengthRunLeft > 0)
		{
			// Lets get this compressed data...

			LARGE_INTEGER offset;
			offset.QuadPart = (offsetInRun * m_cbCluster) + m_llBeginOffset;
			DWORD current_run_length = (DWORD)min(16,lengthRunLeft);
			DWORD cbRead = (DWORD)(current_run_length * m_cbCluster);

			if(!SetFilePointerEx(m_hFile,offset,NULL,FILE_BEGIN))
			{
				return false;
			}


			// Read this compression chunk... <= 16 Clusters

			DWORD dwReaden = 0;
			if(	!::ReadFile(m_hFile,m_pCompBuf,cbRead,&dwReaden,NULL) ||
				cbRead != dwReaden)				
			{
				return false;
			}


			// Do We have 16 non sparse clusters... 
			// that means => Uncompressed... Just copy

			if(16 == current_run_length)
			{
				if(fFile)
				{
					DWORD dwWritten;
					if(	!WriteFile(	hWrite,m_pCompBuf,cbRead,&dwWritten,NULL) || 
						dwWritten != cbRead)
					{
						return false;
					}
				}
				else
				{
					if(pBuf && (*pcbBuf) > (buf_offset + cbRead))
					{
						CopyMemory(&pBuf[buf_offset],m_pCompBuf,cbRead);
					}
					
					buf_offset += cbRead;
				}

				lengthRunLeft -= current_run_length;
				offsetInRun += current_run_length;
				cClustersPx = 0;
				continue;
			}


			// Actual Compressed Data. Process...
			
			_U64 buf_delta = 0;
			if(!fFile)
			{
				if(NULL != pBuf || (*pcbBuf) > buf_offset)
				{
					buf_delta = (*pcbBuf) - buf_offset;
				}
			}

			
			if(!DecompressLZChunk(m_pCompBuf, cbRead, hWrite, &pBuf[buf_offset], &buf_delta, fFile))
			{
				if(ERR_BUFFER_TOO_SMALL != GetLastError())
				{
					return false;
				}
			}

			
			if(!fFile)
			{
				buf_offset += buf_delta;
			}
			

			// Processed x clusters...
			// mod 16 -> gives us the required rollever effect if we just completed a chunk

			cClustersPx += current_run_length & 0xF; 
			lengthRunLeft -= current_run_length;
            offsetInRun += current_run_length;
		}
	}

	if(!fFile)
	{
		if(NULL == pBuf || ((*pcbBuf) < buf_offset))
		{
			*pcbBuf = buf_offset;
			m_dwLastError = ERR_BUFFER_TOO_SMALL;
			return false;
		}

		*pcbBuf = buf_offset;
	}

	return true;
}
	
/////////////////////////////////////////////////////////////////////

bool DataRunHelper::DecompressLZChunk(	_U8* pCompBuf, DWORD cbCompBuf, 
										HANDLE hWrite, BYTE *pBuf, 
										_U64 *pcbBuf, bool fFile)
{
	ASSERT(m_fInit);
	ASSERT(pCompBuf);
	ASSERT(cbCompBuf <= 16 * m_cbCluster);

	if(NULL == pCompBuf)
	{
		return false;
	}

	if(!fFile)
	{
		ASSERT(pcbBuf);
		if(NULL == pcbBuf)
		{
			m_dwLastError = ERR_INVALID_PARAM;
			return false;
		}
	}
	
	// Compression Buffer must be less than equal max compression chunk

	if(cbCompBuf > 16 * m_cbCluster)
	{
		return false;
	}


	// Decompression Buffer Allocated?

	if(NULL == m_pDecompBuf)
	{
		m_pDecompBuf = new _U8[m_cbCluster]; // Buffer for entire decompression unit
		ASSERT(m_pDecompBuf);
		if(NULL == m_pDecompBuf)
		{
			return false;
		}
	}


	// Do it...

	_U64 buf_offset = 0;
	int cbLeft = (int)cbCompBuf;
	_U8* current = pCompBuf;

	while(cbLeft > 0)
	{
		// SubChunk Compressed Length...

		_U32 clen = *((_U16*)current) & 0xFFF; // 12 bit length
		current+= 2;		// Skip length


		// End of this Subchunk?

		if(clen == 0 || clen > (_U32)cbLeft)
		{
			break;
		}

		// SubChunk Uncompressed?

		if(clen == 0xFFF)
		{
			if(fFile)
			{
				DWORD dwWritten;
				if(	!WriteFile(hWrite,current,m_cbCluster,&dwWritten,NULL) ||
					dwWritten != m_cbCluster)
				{
					return false;
				}
			}
			else
			{
				// ! a Size Request && enough space...

				if(NULL != pBuf && ((buf_offset + m_cbCluster) < (*pcbBuf)))
				{
					CopyMemory(&pBuf[buf_offset],current,m_cbCluster);	
				}
				
				buf_offset += m_cbCluster;
			}

			cbLeft -= m_cbCluster; 
			current+=m_cbCluster;

			
			// next Subchunk...

			continue;
		}

		
		// SubChunk Compressed...

		_U32 cbComp = 0;	
		_U32 cbDecomp = 0;

		while(cbComp <= clen)
		{
			// Tag Byte

			_U8 tag = *current;
			current++;
			cbComp++;

			
			// Tackle in blocks of 8 bytes

			for(int j = 0;j<8 && (cbDecomp < m_cbCluster) && (cbComp <= clen);
				j++,tag>>=1)
			{
                if(tag & 1) // We are at a {off,len} pair.
				{
					// What kind of split are we using? This is like ridiculously
					// complicated... well it was to figure out. basically the 
					// amount of bits used for the offset changes with how far 
					// into the decompressed file we are... So initially we use 4bits
					// then when we get 16 (2^4) bytes into the decomp buffer
					// we switch to 5bits. then when we get 32 (2^5) bytes into the
					// file we switch to 6bits and so on up to 12bits...

					_U32 off_shift = 12;
					_U32 len_mask = 0xFFF;

					while(cbDecomp > (_U32)(1<< (16-off_shift)))
					{
						off_shift--;
						len_mask>>=1;
					}

					_U16* pTag = (_U16*)current;
					int offset = (*pTag) >> off_shift;
					int length = (*pTag) & len_mask;

					length += 3; // length is (-3)-based
					offset = ~offset;	// offset stored as positive value - 1
										// Add one and Negate = bitwise not

					
					// Will this run exceed our Decompression Buffer bounds?
					// Error somewhere with this subchunk, as Decompressed
					// data will always be 1 cluster or less...

					if((cbDecomp + length) > m_cbCluster)
					{
						ASSERT(false);
						return false;
					}
					
					while(length > 0)
					{
						m_pDecompBuf[cbDecomp] = m_pDecompBuf[cbDecomp + offset];
						cbDecomp++;
						length--;
					}
					
					current+=2;
					cbComp+=2;
				}
				else
				{
					m_pDecompBuf[cbDecomp++] = *current++;
					cbComp++;
				}
			}
		}

		// SubChunk Decompressed... Dump...
		
		if(fFile)
		{
			DWORD dwWritten;
			if(	!WriteFile(hWrite,m_pDecompBuf,cbDecomp,&dwWritten,NULL) ||
				dwWritten != cbDecomp)
			{
				return false;
			}
		}
		else
		{
			if(NULL != pBuf && ((buf_offset + cbDecomp) <= (*pcbBuf)))
			{
				CopyMemory(&pBuf[buf_offset],m_pDecompBuf,cbDecomp);
			}

			buf_offset += cbDecomp;
		}

		cbLeft -= clen;
	}


	if(!fFile)
	{
		if(NULL == pBuf || ((*pcbBuf) < buf_offset))
		{
			*pcbBuf = buf_offset;
			m_dwLastError = ERR_BUFFER_TOO_SMALL;
			return false;
		}

		*pcbBuf = buf_offset;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////

bool DataRunHelper::GetSingleUncompressedRun(	HANDLE hWrite, BYTE* pBuf,
												_U64 *pcbBuf, int cRun, 
												bool fFile)
{
	ASSERT(m_fInit);

	if(!fFile)
	{
		ASSERT(pcbBuf);
		if(NULL == pcbBuf)
		{
			m_dwLastError = ERR_INVALID_PARAM;
			return false;
		}
	}

	
	_U8 *pWriteBuf = new _U8[m_cbCluster];
	if(NULL == pWriteBuf)
	{
		return false;
	}

	_U64 cbWant = m_pRuns[cRun].length * m_cbCluster;
	_I64 offset = m_pRuns[cRun].offset * m_cbCluster + m_llBeginOffset;

	// Enough room?

	if(	!fFile && 
		(NULL == pBuf || ((*pcbBuf) < cbWant)))
	{
		*pcbBuf = cbWant;
		m_dwLastError = ERR_BUFFER_TOO_SMALL;
		return false;
	}


	_U64 buf_offset = 0;

	// Sparse => zero fill

	if(m_pRuns[cRun].fSparse)
	{
		ZeroMemory(pWriteBuf,m_cbCluster);
		LONGLONG cbLeftToWrite = cbWant;

		_U64 buf_offset = 0;
		while(0 != cbLeftToWrite)
		{
			DWORD dwWrite = (DWORD)min(m_cbCluster,cbLeftToWrite);

			if(fFile)
			{
				DWORD dwWritten = 0;
				if(	!WriteFile(hWrite,pWriteBuf,dwWrite,&dwWritten,NULL) ||
					dwWrite != dwWritten)
				{
					delete pWriteBuf;
					return false;
				}
			}
			else
			{
				CopyMemory(&pBuf[buf_offset],pWriteBuf,dwWrite);
				buf_offset += dwWrite;
			}

			cbLeftToWrite -= dwWrite;
		}
	}
	else
	{
		LONGLONG cbLeftToWrite = cbWant;
		LARGE_INTEGER foo;
		foo.QuadPart = offset;

		if(!SetFilePointerEx(m_hFile,foo,NULL,FILE_BEGIN))
		{
			delete pWriteBuf;
			return false;
		}

		while(0 != cbLeftToWrite)
		{
			DWORD dwWritten = 0;
			DWORD dwWrite = (DWORD)min(m_cbCluster,cbLeftToWrite);

			// Read

			if(	!::ReadFile(m_hFile,pWriteBuf,dwWrite,&dwWritten,NULL) ||
				dwWrite != dwWritten)				
			{
				delete pWriteBuf;
				return false;
			}


			// MMcG - 12/04/2005
			// Absolute SetFilePointerEx seems to be required here
			// I do not know why in god's green earth this is so
			// but it caused some pain and so it stays. File Pointer 
			// on above read is not updated, possibly because its
			// on \\PhysicalDrive or somethin?

			foo.QuadPart += dwWritten;
			if(!::SetFilePointerEx(m_hFile,foo,NULL,FILE_BEGIN))
			{
				delete pWriteBuf;
				return false;
			}


			if(fFile)
			{
				// Write

				if(	!::WriteFile(hWrite,pWriteBuf,dwWrite,&dwWritten,NULL) ||
					dwWrite != dwWritten)
				{
					delete pWriteBuf;
					return false;
				}
			}
			else
			{
				CopyMemory(&pBuf[buf_offset],pWriteBuf,dwWrite);
				buf_offset += dwWrite;
			}

			cbLeftToWrite -= dwWritten;
		}
	}

	if(!fFile)
	{
		if(NULL == pBuf || ((*pcbBuf) < buf_offset))
		{
			*pcbBuf = buf_offset;
			m_dwLastError = ERR_BUFFER_TOO_SMALL;
			return false;
		}

		*pcbBuf = buf_offset;
	}

	delete pWriteBuf;
	return true;
}

/////////////////////////////////////////////////////////////////////

bool DataRunHelper::AddRawRun(BYTE* pRun, DWORD dwLen)
{
	ASSERT(m_fInit);

	if(NULL == pRun || dwLen == 0)
	{
		return false;
	}

	// Calculate necessary storage

	int i=0;
	int cRuns = 0;
	while(i < (int)dwLen  && pRun[i] != 0)
	{
		int cbOffset = (pRun[i] & 0xF0) >> 4;
		int cbLength = (pRun[i] & 0x0F);		

		i += cbLength + cbOffset + 1;
		if(i > (int)dwLen)
		{
			return false; // Badly formed Run
		}

		cRuns++;
	}

	// Allocate storage & fill

	DataRun *pRuns = new DataRun[m_cRuns + cRuns]; // allow space if we already have a set of runs
	if(NULL == pRuns)
	{
		return false; // OOM
	}

	// Copy any old runs into new mem

	if(m_pRuns)
	{
		for(int k=0;k<(int)m_cRuns;k++)
		{
			pRuns[k].length = m_pRuns[k].length;
			pRuns[k].offset = m_pRuns[k].offset;
		}

		delete m_pRuns; // Delete Old
		m_pRuns = 0;
	}

	i=0;
	int c=m_cRuns; // Start after old runs
	

	// Need to find last offset used in a real (non-sparse run) because
	// any following runs will have offset relative to this...

	LONGLONG lastRealRunOffset = 0;
	for(int y=c-1;y>=0;y--)
	{
		if(!m_pRuns[y].fSparse)
		{
			lastRealRunOffset = m_pRuns[y].offset;
			break;
		}
	}

	while(i < (int)dwLen && pRun[i] != 0)
	{
		int cbOffset = (pRun[i] & 0xF0) >> 4;
		int cbLength = (pRun[i] & 0x0F);	

		
		if((i + cbLength + cbOffset + 1) > (int)dwLen)
		{
			return false; // Badly formed Run
		}

		
		// length

		pRuns[c].length = 0;
		for(int j=0;j<cbLength;j++)
		{
			i++;
			pRuns[c].length += (pRun[i] << (8*j));
		}

		// offset => This is a signed value which requires the following
		// negatizing code for correct interpretation!

		i++;
		BYTE l[8] = {0x00};
		for(int j=0;j<cbOffset;j++)
		{
			l[j] = pRun[i+j];
		}

		if(0 != cbOffset)
		{
			if(0x80 == (0x80 & l[cbOffset-1]))
			{
				// negative value! => FILL 0xFF above
				
				for(int k=cbOffset;k<8;k++)
				{
					l[k] = 0xFF;
				}	
			}
		}

		pRuns[c].offset = *(PLONGLONG)&l;

		// Sparse?

		pRuns[c].fSparse = (cbOffset == 0);
		
        // This offset is relative to last non sparse run 
		// if there is one & its not sparse

		if(!pRuns[c].fSparse)
		{
			pRuns[c].offset += lastRealRunOffset;
			lastRealRunOffset = pRuns[c].offset;
		}

		i+=cbOffset; // Next Run
		c++;
	}

	m_pRuns = pRuns;
	m_cRuns = cRuns;

	return true;
}

/////////////////////////////////////////////////////////////////////