/////////////////////////////////////////////////////////////////////
// 24/JUN/2006
//
// Project: MMcGReiser
// File: MMcGReiser.cpp
// Author: Mark McGuill
//
// Copyright (C) 2005  Mark McGuill
//
/////////////////////////////////////////////////////////////////////

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string.h>
#include "..\..\inc\viewfs_types.h"
#include "..\..\inc\linux.h"
#include "MMcGReiser.h"


#ifdef _DEBUG
	#include <stdio.h>
#endif

/////////////////////////////////////////////////////////////////////

#define INIT_HARDCODED_SUPERBLOCK_START	0x10000
#define INIT_HARDCODED_SUPERBLOCK_SIZE	0x1000
#define INIT_HARDCODED_SECTOR_SIZE		0x200
#define CACHE_MAX_BLOCKS				128

/////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
	#define _CRTDBG_MAP_ALLOC
	#include <crtdbg.h>

	#define ASSERT(x) _ASSERTE(x)
#else
	#define ASSERT(x)
#endif

/////////////////////////////////////////////////////////////////////

using namespace MMcGReiser;

/////////////////////////////////////////////////////////////////////

ReiserReader::ReiserReader()
{
	m_fInit = false;
	m_dwLastError = 0;
	m_hFile = NULL;
	m_llBeginFileOffset = 0;
	m_pSB = NULL;

	m_pmapMUBlockCache = NULL;
	m_lb_time = 0;
	m_pvecRSBCache = 0;
	m_RSBLevel = 0;
	m_RSBMaxLevel = 0;

	m_cDirectType = 0;
	m_cIndirectType = 0;
}

/////////////////////////////////////////////////////////////////////

ReiserReader::~ReiserReader()
{
	Close();
}

/////////////////////////////////////////////////////////////////////

void ReiserReader::Close()
{
	m_fInit = false;

	// Free any cached blocks...

    if(m_pmapMUBlockCache)
	{
		for(map<_U32,cache_item>::iterator e = m_pmapMUBlockCache->begin();
			e != m_pmapMUBlockCache->end();e++)
		{
			if(e->second.pBlk)
			{
				delete e->second.pBlk;
			}
		}

		m_pmapMUBlockCache->clear();

		delete m_pmapMUBlockCache;
		m_pmapMUBlockCache = NULL;
	}

	
	if(m_pvecRSBCache)
	{
		for(vector<_U8*>::iterator elem = m_pvecRSBCache->begin();
			elem != m_pvecRSBCache->end();elem++)
		{
			delete (*elem);		
		}

		m_pvecRSBCache->clear();
		delete m_pvecRSBCache;
		m_pvecRSBCache = NULL;
	}


	if(m_hFile)
	{
		CloseHandle(m_hFile);
	}
	m_hFile = NULL;

	if(m_pSB)
	{
		delete m_pSB;
	}
	m_pSB = NULL;

	m_dwLastError = 0;
	m_llBeginFileOffset = 0;
	m_lb_time = 0;
	m_pvecRSBCache = 0;
	m_RSBLevel = 0;
	m_RSBMaxLevel = 0;

	m_cDirectType = 0;
	m_cIndirectType = 0;
}

/////////////////////////////////////////////////////////////////////

bool ReiserReader::Init(HANDLE hFile, _I64 llBeginFileOffset)
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


	// Load Super Block

	LARGE_INTEGER liTmp;
	liTmp.QuadPart = m_llBeginFileOffset + INIT_HARDCODED_SUPERBLOCK_START;

	if(!SetFilePointerEx(m_hFile,liTmp,NULL,FILE_BEGIN))
	{
		m_dwLastError = ERR_READING_FILE;
		CloseHandle(m_hFile);
		m_hFile = NULL;
		return false;
	}


	_U8 *pBuffer = new _U8[INIT_HARDCODED_SUPERBLOCK_SIZE];
	ASSERT(pBuffer);
	if(NULL == pBuffer)
	{
		CloseHandle(m_hFile);
		m_hFile = NULL;
		m_dwLastError = ERR_READING_FILE;
		return false;
	}

	
	// Read

	DWORD cbRead = 0;
	if(!ReadFile(m_hFile,pBuffer,INIT_HARDCODED_SUPERBLOCK_SIZE,&cbRead,NULL))
	{
		delete pBuffer;
		CloseHandle(m_hFile);
		m_hFile = NULL;
		m_dwLastError = ERR_READING_FILE;
		return false;
	}

	m_pSB = new reiser_super_block;
	
	ASSERT(m_pSB);
	if(NULL == m_pSB)
	{
		delete pBuffer;
		m_dwLastError = ERR_OUT_OF_MEMORY;
		CloseHandle(m_hFile);
		m_hFile = NULL;
		return false;
	}

	CopyMemory(m_pSB,pBuffer,sizeof(reiser_super_block));

		
	// Sanity Check 

	if(!REISER_MAGIC_OK(m_pSB->MagicString))
	{
		delete pBuffer;
		CloseHandle(m_hFile);
		m_hFile = NULL;
		m_dwLastError = ERR_FS_CORRUPT;
		return false;
	}

	if(0 == _stricmp(m_pSB->MagicString,REISER_MAGIC))
	{
		m_cDirectType = 0xFFFFFFFF;
		m_cIndirectType = 0xFFFFFFFE;
	}
	else
	{
		m_cDirectType = 2;
		m_cIndirectType = 1;
	}

	delete pBuffer;

	m_fInit = true;

	return true;
}

/////////////////////////////////////////////////////////////////////

bool ReiserReader::GetItemFromPath(LPTSTR pszPath,ReiserItem* pItem)
{
	ASSERT(m_fInit);
	if(!m_fInit)
	{		
		m_dwLastError = ERR_NOT_INIT;
		return false;
	}

	ASSERT(pszPath);
	ASSERT(pItem);

	if(	NULL == pszPath || NULL == pItem)
	{
		m_dwLastError = ERR_INVALID_PARAM;
		return false;
	}

	// sanity check path should start with /

	if(_T('/') != pszPath[0])
	{
		m_dwLastError = ERR_INVALID_PARAM;
		return false;
	}

	working_reiser_key key = {1,2,1,500};
	pItem->dwDirectoryID = 1;
	pItem->dwObjectID = 2;
	pItem->dwAccess = 0;
	pItem->dwCreate = 0;
	pItem->dwModify = 0;
	pItem->dwMode = S_IFDIR;
	pItem->size = 0;
	_tcscpy(pItem->szName,_T("/"));


	TCHAR *pCopy = _tcsdup(pszPath);
	TCHAR *pTok = pCopy;

	while(NULL != (pTok = _tcstok(pTok,_T("/"))))
	{
		_U32 cItems = 0;
		READ_DIRECTORY_PARAM rdp;
		rdp.pItems = NULL;
		rdp.cItemsTotal = cItems;
		rdp.cItemsWritten = 0;
		rdp.pcItemsRequired = &cItems;

		if(!SearchFromRoot(&key,ReadDirectoryCB,&rdp))
		{
			m_dwLastError = ERR_PATH_NOT_FOUND;
			free(pCopy);
			return false;
		}

		ReiserItem* pItems = new ReiserItem[cItems];
		if(NULL == pItems)
		{
			m_dwLastError = ERR_OUT_OF_MEMORY;
			free(pCopy);
			return false;
		}

		rdp.pItems = pItems;
		rdp.cItemsTotal = cItems;
		rdp.pcItemsRequired = &cItems;

		if(!SearchFromRoot(&key,ReadDirectoryCB,&rdp))
		{
			m_dwLastError = ERR_PATH_NOT_FOUND;
			delete pItems;
			free(pCopy);
			return false;
		}

		_U32 i = 0;
		for(;i<cItems;i++)
		{
			if(_tcscmp(pItems[i].szName,pTok))
			{
				continue;
			}

			CopyMemory(pItem,&pItems[i],sizeof(ReiserItem));
			key.DirectoryID = pItems[i].dwDirectoryID;
			key.ObjectID = pItems[i].dwObjectID;
			break;
		}

		delete pItems;

		if(i == cItems)
		{
			m_dwLastError = ERR_PATH_NOT_FOUND;
			free(pCopy);
			return false;
		}

		pTok = NULL; // Next Directory
	}


	// Stat the item for the rest of the details...

	STAT_ITEM_PARAM sip;
	key.Offset = 0;
	key.Type = 0;

	if(!SearchFromRoot(&key,StatItemCB,&sip))
	{
		return false;
	}

	pItem->dwAccess = (sip.hdr.Version == 0) ? 
							sip.stat_item.v1.Atime :
							sip.stat_item.v2.Atime;

	pItem->dwCreate = (sip.hdr.Version == 0) ? 
							sip.stat_item.v1.Ctime :
							sip.stat_item.v2.Ctime;

	pItem->dwModify = (sip.hdr.Version == 0) ? 
							sip.stat_item.v1.Atime :
							sip.stat_item.v2.Atime;

	pItem->dwMode = (sip.hdr.Version == 0) ? 
							sip.stat_item.v1.Mode :
							sip.stat_item.v2.Mode;

	pItem->size = (sip.hdr.Version == 0) ? 
							sip.stat_item.v1.Size :
							sip.stat_item.v2.Size;

	free(pCopy);
	return true;
}

/////////////////////////////////////////////////////////////////////

//// NOTE: Does not fill in name
//bool ReiserReader::FillReiserItemFromKey(	_U32 DirID,_U32 ObjID,
//							ReiserItem *pNewItem)
//{
//	ASSERT(m_fInit);
//	if(!m_fInit)
//	{		
//		m_dwLastError = ERR_NOT_INIT;
//		return false;
//	}
//
//	ASSERT(pNewItem);
//	if(NULL == pNewItem)
//	{
//		m_dwLastError = ERR_INVALID_PARAM;
//		return false;
//	}
//
//	
//	pNewItem->dwDirectoryID = DirID;
//	pNewItem->dwObjectID = ObjID;
//	
//	// skip roots parent item
//
//    if(pNewItem->dwDirectoryID == 0 && pNewItem->dwObjectID == 1)
//	{
//		pNewItem->dwAccess = 0;
//		pNewItem->dwCreate = 0;
//		pNewItem->dwModify = 0;
//		pNewItem->dwMode = S_IFDIR;
//		pNewItem->size = 0;
//	}
//	else
//	{
//		working_reiser_key key = {pNewItem->dwDirectoryID,pNewItem->dwObjectID,0,0};
//
//		STAT_ITEM_PARAM sip;
//
//		if(!SearchFromRoot(&key,StatItemCB,&sip))
//		{
//			return false;
//		}
//
//		pNewItem->dwAccess = (sip.hdr.Version == 0) ? 
//								sip.stat_item.v1.Atime :
//								sip.stat_item.v2.Atime;
//
//		pNewItem->dwCreate = (sip.hdr.Version == 0) ? 
//								sip.stat_item.v1.Ctime :
//								sip.stat_item.v2.Ctime;
//
//		pNewItem->dwModify = (sip.hdr.Version == 0) ? 
//								sip.stat_item.v1.Atime :
//								sip.stat_item.v2.Atime;
//
//		pNewItem->dwMode = (sip.hdr.Version == 0) ? 
//								sip.stat_item.v1.Mode :
//								sip.stat_item.v2.Mode;
//
//		pNewItem->size = (sip.hdr.Version == 0) ? 
//								sip.stat_item.v1.Size :
//								sip.stat_item.v2.Size;
//	}
//
//	return true;
//}

/////////////////////////////////////////////////////////////////////

bool ReiserReader::ReadItem(_U32 DirectoryID, _U32 ObjectID, 
							_U8 *pData, _U32* pcData)
{
	ASSERT(m_fInit);
	if(!m_fInit)
	{		
		m_dwLastError = ERR_NOT_INIT;
		return false;
	}
	
	ASSERT(pcData);
	if(NULL == pcData)
	{
		m_dwLastError = ERR_INVALID_PARAM;
		return false;
	}

	working_reiser_key key = {DirectoryID,ObjectID,0,0};

	STAT_ITEM_PARAM sip;

	if(!SearchFromRoot(&key,StatItemCB,&sip))
	{
		return false;
	}


	LARGE_INTEGER liSize;
	liSize.QuadPart = ((sip.hdr.Version == 1) ? sip.stat_item.v2.Size : sip.stat_item.v1.Size);

	if(NULL == pData)
	{
		// Allow the cast here because we are not going to be handling
		// buffers larger than 32 bits..  are we????

		*pcData = (_U32)liSize.QuadPart;
		return true;
	}

	
	// Zero Byte File?

	if(0 == liSize.QuadPart) 
	{
		return true;
	}

	READ_ITEM_PARAM rip;
	rip.pData = pData;
	rip.cData = *pcData;
	rip.cbWritten = 0;
	rip.pSB = m_pSB;
	rip.hFileImage = m_hFile;
	rip.llBeginFileOffset = m_llBeginFileOffset;

	// There doesn't seem to be an easy way to tell if the item is direct or indirect
	// try direct first if that fails try indirect...

	key.Offset = 1;
	key.Type = m_cDirectType;

	if(SearchFromRoot(&key,ReadDirectItemCB,&rip))
	{
		return true;
	}

	// Indirect

	key.Offset = 1;
	key.Type = m_cIndirectType;

	return SearchFromRoot(&key,ReadIndirectItemCB,&rip);
}

/////////////////////////////////////////////////////////////////////

bool ReiserReader::ReadIndirectItemCB(	void* pParam,
										reiser_item_header *pItemHdr,
										_U8* pBlock)
{
	ASSERT(pParam);

	READ_ITEM_PARAM *prip = (READ_ITEM_PARAM *)pParam;

	ASSERT(prip->pData);

	
	// For each block loaded 

	_U8* pIndBlock = new _U8[prip->pSB->BlockSize];
	ASSERT(pIndBlock);
	if(NULL == pIndBlock)
	{
		return false;
	}

	
	// load & copy blocks

	_U32 DataSize = pItemHdr->Length;
	_U8* pData = pBlock + pItemHdr->Location;
	_U8 *pEnd = pData + DataSize;

	while(pData < pEnd && prip->cbWritten < prip->cData)
	{
		// LoadBlock

		LARGE_INTEGER liTmp;
		_U32 block = *((_U32*)(pData));
		liTmp.QuadPart = prip->llBeginFileOffset + ((_U64)block * (_U64)prip->pSB->BlockSize);

		if(!SetFilePointerEx(prip->hFileImage,liTmp,NULL,FILE_BEGIN))
		{
			delete pIndBlock;
			return false;
		}
	

		// Read
		
		DWORD cbRead = 0;
		if(!ReadFile(prip->hFileImage,pIndBlock,prip->pSB->BlockSize,&cbRead,NULL))
		{
			// TODO: allow some way to pass back error
			delete pIndBlock;
			return false;
		}

		
		// Copy...

		_U32 size = min(prip->cData - prip->cbWritten,prip->pSB->BlockSize);
		CopyMemory(&prip->pData[prip->cbWritten],pIndBlock,size);
		
		prip->cbWritten += size;
		pData+=4;
	}
	
	delete pIndBlock;
	return true;
}

/////////////////////////////////////////////////////////////////////

bool ReiserReader::ReadDirectItemCB(void* pParam,
									reiser_item_header *pItemHdr,
									_U8* pBlock)
{
	ASSERT(pParam);
	READ_ITEM_PARAM *prip = (READ_ITEM_PARAM *)pParam;
	ASSERT(prip->pData);

	_U32 size = min(prip->cData - prip->cbWritten,pItemHdr->Length);
	
	CopyMemory(&prip->pData[prip->cbWritten],
				pBlock + pItemHdr->Location,size);

	prip->cbWritten += size;

	return true;
}

/////////////////////////////////////////////////////////////////////

bool ReiserReader::ReadSymbolicLink(_U32 DirectoryID,_U32 ObjectID, 
									TCHAR *pszBuf,	_U32 *pcchBuf)
{
	ASSERT(m_fInit);
	if(!m_fInit)
	{		
		m_dwLastError = ERR_NOT_INIT;
		return false;
	}
	
	ASSERT(pcchBuf);
	if(NULL == pcchBuf)
	{
		m_dwLastError = ERR_INVALID_PARAM;
		return false;
	}

	working_reiser_key key = {DirectoryID,ObjectID,0,0};

	STAT_ITEM_PARAM sip;

	if(!SearchFromRoot(&key,StatItemCB,&sip))
	{
		return false;
	}

	LARGE_INTEGER liSize;
	liSize.QuadPart = ((sip.hdr.Version == 1) ? sip.stat_item.v2.Size : sip.stat_item.v1.Size);

	if(NULL == pszBuf || liSize.QuadPart > *pcchBuf)
	{
		// Allow the cast here because we are not going to be handling
		// buffers larger than 32 bits..  are we????

		*pcchBuf = (_U32)liSize.QuadPart;
		return true;
	}

	
	// Zero Byte File?

	if(0 == liSize.QuadPart) 
	{
		return true;
	}

	READ_ITEM_PARAM rip;
	rip.pData = (_U8*)pszBuf;
	rip.cData = *pcchBuf;
	rip.cbWritten = 0;
	rip.pSB = m_pSB;
	rip.hFileImage = m_hFile;
	rip.llBeginFileOffset = m_llBeginFileOffset;

	// There doesn't seem to be an easy way to tell if the item is direct or indirect
	// try direct first if that fails try indirect...

	key.Offset = 1;
	key.Type = 0xFFFFFFFF;

	if(SearchFromRoot(&key,ReadDirectItemCB,&rip))
	{
		pszBuf[liSize.QuadPart] = 0;
		return true;
	}

	// Indirect

	key.Offset = 1;
	key.Type = 0xFFFFFFFE;

	
	bool ret = SearchFromRoot(&key,ReadIndirectItemCB,&rip);
	pszBuf[liSize.QuadPart] = 0;
	return ret;
}

/////////////////////////////////////////////////////////////////////

bool ReiserReader::ReadItemToFile(	_U32 DirectoryID, _U32 ObjectID, 
									TCHAR *pszFile)
{
	ASSERT(m_fInit);
	if(!m_fInit)
	{		
		m_dwLastError = ERR_NOT_INIT;
		return false;
	}

	ASSERT(pszFile);
	if(NULL == pszFile)
	{
		m_dwLastError = ERR_INVALID_PARAM;
		return false;
	}

	working_reiser_key key = {DirectoryID,ObjectID,0,0};

	STAT_ITEM_PARAM sip;

	if(!SearchFromRoot(&key,StatItemCB,&sip))
	{
		return false;
	}

	
	// Calculate Eventual File Size...

	LARGE_INTEGER liSize;
	if(sip.hdr.Version != 1)
	{
		liSize.QuadPart = sip.stat_item.v1.Size;
	}
	else
	{
		liSize.QuadPart = sip.stat_item.v2.Size;
	}


	// Create the file...

	HANDLE hWrite = CreateFile(	pszFile,GENERIC_WRITE,FILE_SHARE_READ,
								NULL,CREATE_ALWAYS,	
								FILE_ATTRIBUTE_NORMAL,NULL);

	if(INVALID_HANDLE_VALUE == hWrite)
	{
		m_dwLastError = (ERROR_FILE_EXISTS == ::GetLastError()) ? 
				ERR_FILE_ALREADY_EXISTS	: 
				ERR_CREATE_FILE;

		return false;
	}

	// Zero Byte File?

	if(0 == liSize.QuadPart) 
	{
		CloseHandle(hWrite);
		return true;
	}
	
	
	// Setup Callback struct

	READ_ITEM_TO_FILE_PARAM rip;
	rip.hFileImage = m_hFile;
	rip.hFileWrite = hWrite;
	rip.llBeginFileOffset = m_llBeginFileOffset;
	rip.pSB = m_pSB;
	rip.FileSize = liSize.QuadPart;


	// There doesn't seem to be an easy way to tell if the item is direct or indirect
	// try direct first if that fails try indirect...

	// Could also be a symbolic link...

	key.Offset = 1;
	key.Type = 	m_cDirectType;

	if(SearchFromRoot(&key,ReadDirectItemToFileCB,&rip))
	{
		if(	!SetFilePointerEx(hWrite,liSize,NULL,FILE_BEGIN) || 
			!SetEndOfFile(hWrite) || 
			!CloseHandle(hWrite))
		{
			DeleteFile(pszFile);
			return false;
		}

		return true;
	}

	
	// Indirect

    key.Offset = 1;
	key.Type = m_cIndirectType;

	if(!SearchFromRoot(&key,ReadIndirectItemToFileCB,&rip))
	{
		CloseHandle(hWrite);
		DeleteFile(pszFile);
		return false;
	}

	if(	!SetFilePointerEx(hWrite,liSize,NULL,FILE_BEGIN) || 
		!SetEndOfFile(hWrite) || 
		!CloseHandle(hWrite))
	{
		DeleteFile(pszFile);
		return false;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////

bool ReiserReader::ReadIndirectItemToFileCB(void* pParam,
											reiser_item_header *pItemHdr,
											_U8* pBlock)
{
	ASSERT(pParam);

	READ_ITEM_TO_FILE_PARAM *prip = (READ_ITEM_TO_FILE_PARAM *)pParam;

	// For each block loaded 

	_U8* pIndBlock = new _U8[prip->pSB->BlockSize];
	ASSERT(pIndBlock);
	if(NULL == pIndBlock)
	{
		return false;
	}

	
	// load & copy blocks

	_U8* pData = pBlock + pItemHdr->Location;
	_U8 *pEnd = pData + pItemHdr->Length;

	LARGE_INTEGER cbCurrentFileSize;
	if(!GetFileSizeEx(prip->hFileWrite,&cbCurrentFileSize))
	{
		delete pIndBlock;
		return false;
	}

	while(	pData < pEnd && 
			(_U64)cbCurrentFileSize.QuadPart < prip->FileSize)
	{
		// LoadBlock

		LARGE_INTEGER liTmp;
		_U32 block = *((_U32*)(pData));
		liTmp.QuadPart = prip->llBeginFileOffset + ((_U64)block * (_U64)prip->pSB->BlockSize);

		if(!SetFilePointerEx(prip->hFileImage,liTmp,NULL,FILE_BEGIN))
		{
			delete pIndBlock;
			return false;
		}
		
		// Read
		
		DWORD cbRead = 0;
		if(!ReadFile(prip->hFileImage,pIndBlock,prip->pSB->BlockSize,&cbRead,NULL))
		{
			// TODO: allow some way to pass back error
			delete pIndBlock;
			return false;
		}
		

		// Write... write block, ReadItemToFile will truncate file where necessary
		// using stat_item->size so we don't have to bother

		DWORD dwWritten = 0;

		if(!WriteFile(prip->hFileWrite,pIndBlock,prip->pSB->BlockSize,&dwWritten,NULL) ||
			prip->pSB->BlockSize != dwWritten)
		{
			delete pIndBlock;
			return false;
		}

		cbCurrentFileSize.QuadPart += dwWritten;
		pData+=4;
	}
	
	delete pIndBlock;
	return true;
}

/////////////////////////////////////////////////////////////////////

bool ReiserReader::ReadDirectItemToFileCB(	void* pParam,
											reiser_item_header *pItemHdr,
											_U8* pBlock)
{
	ASSERT(pParam);

	READ_ITEM_TO_FILE_PARAM *prip = (READ_ITEM_TO_FILE_PARAM *)pParam;

	DWORD dwWritten = 0;

	_U8* pData = pBlock + pItemHdr->Location;

	if(!WriteFile(prip->hFileWrite,pData,pItemHdr->Length,&dwWritten,NULL) ||
		pItemHdr->Length != dwWritten)
	{
		return false;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////

bool ReiserReader::ReadDirectory(	_U32 DirectoryID, _U32 ObjectID, 
									ReiserItem *pItems, _U32* pcItems)
{
	ASSERT(m_fInit);
	if(!m_fInit)
	{		
		m_dwLastError = ERR_NOT_INIT;
		return false;
	}
	
	ASSERT(pcItems);
	if(NULL == pcItems)
	{
		m_dwLastError = ERR_INVALID_PARAM;
		return false;
	}

	// Load Stat Item & verify its a directory!

	working_reiser_key key = {DirectoryID,ObjectID,0,0};


	STAT_ITEM_PARAM sip;

	if(!SearchFromRoot(&key,StatItemCB,&sip))
	{
		return false;
	}
    
	if(!IS_DIRECTORY(sip.stat_item.v1.Mode))
	{
		m_dwLastError = ERR_NOT_DIRECTORY;
		return false;
	}
	

	// Ok its def a directory... Read it!

	key.Offset = 1;
	key.Type = 500;

	READ_DIRECTORY_PARAM rdp;
	rdp.pItems = pItems;
	rdp.cItemsTotal = *pcItems;
	rdp.cItemsWritten = 0;
	rdp.pcItemsRequired = pcItems;

	if(!SearchFromRoot(&key,ReadDirectoryCB,&rdp))
	{
		return false;
	}


	// if we were just getting a count bail now...

	if(NULL == pItems)
	{
		return true;
	}


	// Details...

	for(_U32 i=0;i<*pcItems;i++)
	{
		// skip roots parent item

		if(pItems[i].dwDirectoryID == 0 && pItems[i].dwObjectID == 1)
		{
			pItems[i].dwAccess = 0;
			pItems[i].dwCreate = 0;
			pItems[i].dwModify = 0;
			pItems[i].dwMode = S_IFDIR;
			pItems[i].size = 0;

			continue;
		}

		working_reiser_key key = {pItems[i].dwDirectoryID,pItems[i].dwObjectID,0,0};

		STAT_ITEM_PARAM sip;

		if(!SearchFromRoot(&key,StatItemCB,&sip))
		{
			return false;
		}

		pItems[i].dwAccess = (sip.hdr.Version == 0) ? 
								sip.stat_item.v1.Atime :
								sip.stat_item.v2.Atime;

		pItems[i].dwCreate = (sip.hdr.Version == 0) ? 
								sip.stat_item.v1.Ctime :
								sip.stat_item.v2.Ctime;

		pItems[i].dwModify = (sip.hdr.Version == 0) ? 
								sip.stat_item.v1.Atime :
								sip.stat_item.v2.Atime;

		pItems[i].dwMode = (sip.hdr.Version == 0) ? 
								sip.stat_item.v1.Mode :
								sip.stat_item.v2.Mode;

		pItems[i].size = (sip.hdr.Version == 0) ? 
								sip.stat_item.v1.Size :
								sip.stat_item.v2.Size;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////

bool ReiserReader::ReadDirectoryCB(void* pParam,
								   reiser_item_header *pItemHdr,
								   _U8* pBlock)
{
	ASSERT(pParam);

	READ_DIRECTORY_PARAM *prdp = (READ_DIRECTORY_PARAM *)pParam;

	if(0 == prdp->cItemsTotal) // we are just getting a count...
	{
		(*prdp->pcItemsRequired) += pItemHdr->Count;
		return true;
	}

	
	ASSERT(prdp->pItems);

	if(NULL == prdp->pItems)
	{
		return false;
	}


	// we are adding items, have we got enough room?

	if(((prdp->cItemsWritten) + pItemHdr->Count) > prdp->cItemsTotal)
	{
		return false;
	}
	

	// Parse Directory

	_U8* pData = pBlock + pItemHdr->Location;
	reiser_directory_header *prdh = (reiser_directory_header*)pData;

	for(int j=0,i=(prdp->cItemsWritten);j<pItemHdr->Count;j++,i++,prdh++)
	{
        // Always ASCII...

		// sometimes name wont be null terminated I believe this is to 
		// do with 8byte alignment this is the fix -> Use previous entry 
		// begin as this entry end, or end of block in case of first item

		int len = 0;

		if(j > 0)
		{
			reiser_directory_header *prdhPrev = prdh - 1;
			len = prdhPrev->NameLocation - prdh->NameLocation;
		}
		else
		{
			len = pItemHdr->Length - prdh->NameLocation;
		}
		len = min(len,REISER_CCH_MAX_NAME - 1);


		// Copy Name

		LPTSTR lpszName;
#ifdef _UNICODE
		TCHAR szTmp[REISER_CCH_MAX_NAME];
		MultiByteToWideChar(CP_ACP,0,
							(char*)pData + prdh->NameLocation,
							len,szTmp,REISER_CCH_MAX_NAME);

		lpszName = szTmp;
#else
		lpszName = (char*)pData + prdh->NameLocation;
#endif
		

		_tcsncpy(	prdp->pItems[i].szName,
					lpszName,
					REISER_CCH_MAX_NAME);

		prdp->pItems[i].szName[len] = 0;
		prdp->pItems[i].dwDirectoryID = prdh->DirectoryID;
		prdp->pItems[i].dwObjectID = prdh->ObjectID;

		(prdp->cItemsWritten) = (prdp->cItemsWritten) + 1;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////

bool ReiserReader::StatItemCB(void* pParam,
							  reiser_item_header *pItemHdr,
							  _U8* pBlock)
{
	ASSERT(pParam);
	
	STAT_ITEM_PARAM *psip = (STAT_ITEM_PARAM *)pParam;

	CopyMemory(&psip->hdr,pItemHdr,sizeof(reiser_item_header));
	
	CopyMemory(&psip->stat_item,
				pBlock + pItemHdr->Location,
				pItemHdr->Length);

	return true;
}

/////////////////////////////////////////////////////////////////////

bool ReiserReader::SearchFromRoot(	working_reiser_key* pKeySearch, 
									PFNSEARCH pfnCallback, 
									void *pParam)
{
	ASSERT(m_fInit);
	if(!m_fInit)
	{		
		m_dwLastError = ERR_NOT_INIT;
		return false;
	}

	ASSERT(pfnCallback);
	if(NULL == pfnCallback)
	{
		m_dwLastError = ERR_INVALID_PARAM;
		return false;		
	}

	return RecSearchBlock(m_pSB->RootBlock,pKeySearch,
							pfnCallback,pParam);
}

/////////////////////////////////////////////////////////////////////

bool ReiserReader::RecSearchBlock(	_U32 block, working_reiser_key* pKeySearch, 
									PFNSEARCH pfnCallback, void *pParam)
{
	ASSERT(m_fInit);
	ASSERT(pfnCallback);

	// Init cache if not already done

	if(NULL == m_pvecRSBCache)
	{
		m_pvecRSBCache = new vector<_U8*>;
		ASSERT(m_pvecRSBCache);
		if(NULL == m_pvecRSBCache)
		{
			m_dwLastError = ERR_OUT_OF_MEMORY;
			return false;
		}
	}


	// Get a block buffer from cache or
	// allocate a new one and add to cache

	m_RSBLevel++;
	_U8 *pBlock;

	if(m_RSBLevel > m_RSBMaxLevel)
	{
		pBlock = new _U8[m_pSB->BlockSize];
		ASSERT(pBlock);
		if(NULL == pBlock)
		{
			m_dwLastError = ERR_OUT_OF_MEMORY;
			m_RSBLevel--;
			return false;
		}

		m_pvecRSBCache->push_back(pBlock);
		m_RSBMaxLevel = m_RSBLevel;
	}
	else
	{
		pBlock = (*m_pvecRSBCache)[m_RSBLevel-1];
	}
	

	if(!LoadBlock(block,pBlock))
	{
		m_RSBLevel--;
		return false;
	}

	reiser_block_header *prbh = (reiser_block_header*)pBlock;
	reiser_key *pKey = (reiser_key*)(prbh + 1);


	// Search...

	bool fInMultiReiserItem_Item = false;

	if(prbh->Level > 1) // Internal Node
	{
		int n=0;

		while(n <= prbh->cItems)
		{
			working_reiser_key key;
			key.DirectoryID  = pKey->v2.DirectoryID;
			key.ObjectID  = pKey->v2.ObjectID;

			// Assuming v1 search for Internal nodes, dunno what to do if
			// we get v2!

			if(is_key_format_v1(pKey->v2.Type))
			{
				key.Offset = pKey->v1.Offset;
				key.Type  = pKey->v1.Type;
			}
			else
			{
				key.Offset = pKey->v2.Offset;
				key.Type  = pKey->v2.Type;
			}

            int match = CompareKey(&key,pKeySearch, CompareKeyNormal);

			if(match >= 0 || n == prbh->cItems) // Must Dive
			{
				reiser_pointer* pPointer = (reiser_pointer*)
					(pBlock + sizeof(reiser_block_header) + 
					(prbh->cItems * sizeof(reiser_key)) + 
					(n * sizeof(reiser_pointer)));

				if(	RecSearchBlock(pPointer->BlockNumber,pKeySearch,pfnCallback,pParam))
				{
					// We got our match and there are more pointers @ this level -> 
					// check if its split across multiple reiser items at this level...

					if(n != prbh->cItems)
					{
						fInMultiReiserItem_Item = true;
					}
					else
					{
						m_RSBLevel--;
						return true;
					}
				}
				else if(fInMultiReiserItem_Item)// we have completed our
												// collection of component
												// reiser items for this 
												// file/dir item
				{
					m_RSBLevel--;
					return true;
				}
				else if(match != 0)		// if this was not an exact match we 
										// will not find any more nodes to 
										// dive which will satisfy this search... 
										// bail...
				{
					m_dwLastError = ERR_NO_SUCH_KEY;
					m_RSBLevel--;
					return false;
				}
			}

			pKey++;
			n++;
		}

		m_dwLastError = ERR_NO_SUCH_KEY;
		m_RSBLevel--;
		return false; // Couldn't Find!
	}
	else // Leaf node... 
	{
		reiser_item_header* pItemHdr = (reiser_item_header*)(prbh + 1);

		int n = 0;

		while(n < prbh->cItems)
		{
			working_reiser_key key;
			key.DirectoryID  = pItemHdr->Key.v2.DirectoryID;
			key.ObjectID  = pItemHdr->Key.v2.ObjectID;
			if(pItemHdr->Version == 1)
			{
				key.Offset = pItemHdr->Key.v2.Offset;
				key.Type  = pItemHdr->Key.v2.Type;			
			}
			else
			{
				key.Offset = pItemHdr->Key.v1.Offset;
				key.Type  = pItemHdr->Key.v1.Type;
			}

			// File/Dir Items may be spread across multiple reiser Items
			// Ignore Offset...

			if(0 == CompareKey(&key,pKeySearch,CompareKeyIgnoreOffset))
			{
				break;
			}

			pItemHdr++;
			n++;
		}

		// Found?

		if(n == prbh->cItems) // No...
		{
			m_dwLastError = ERR_NO_SUCH_KEY;
			m_RSBLevel--;
			return false;
		}

		bool ret = pfnCallback(pParam,pItemHdr,pBlock);
		m_RSBLevel--;
		return ret;
	}
}

////////////////////////////////////////////////////////////////////

_I32 ReiserReader::CompareKey(working_reiser_key* p1, 
								working_reiser_key* p2,
								CompareKeyOption opt)
{
	if(p1->DirectoryID != p2->DirectoryID)
	{
		return (p1->DirectoryID < p2->DirectoryID) ? -1 : 1;
	}

	if(p1->ObjectID != p2->ObjectID)
	{
		return (p1->ObjectID < p2->ObjectID) ? -1 : 1;
	}

	if(opt != CompareKeyIgnoreOffset)
	{
		if(p1->Offset != p2->Offset)
		{
			return (p1->Offset < p2->Offset) ? -1 : 1;
		}
	}

	if(p1->Type != p2->Type)
	{
		return (p1->Type < p2->Type) ? -1 : 1;
	}

	return 0;
}

////////////////////////////////////////////////////////////////////

// map<_U32, _U8*> m_mapMUBlockCache;

bool ReiserReader::LoadBlock(_U32 block,_U8 *pBlk)
{
	ASSERT(m_fInit);
	
	// tick...

	m_lb_time++;

	
	// Initialize Cache if not already done

	if(NULL == m_pmapMUBlockCache)
	{
		m_pmapMUBlockCache = new map<_U32,cache_item>;
		ASSERT(m_pmapMUBlockCache);

		if(NULL == m_pmapMUBlockCache)
		{
			m_dwLastError = ERR_READING_FILE;
			return false;
		}
	}

	
	// Check if we have this block cached

	map<_U32,cache_item>::iterator theBlock = m_pmapMUBlockCache->find(block);
    
	if(theBlock != m_pmapMUBlockCache->end()) // HITHITHITHIT!!!!
	{
		CopyMemory(pBlk,theBlock->second.pBlk,m_pSB->BlockSize);
		return true;
	}
	

	// MISSMISSMISSMISS

	// Do we have a full cache?
	// YES ->	overwrite oldest block.
	// NO ->	Alloc a new block and add to cache

	_U8* pBlock = NULL;

	if(m_pmapMUBlockCache->size() >= CACHE_MAX_BLOCKS)
	{
		// Find Oldest block & Overwrite...
		// Lowest time is oldest

		map<_U32,cache_item>::iterator oldest_elem = m_pmapMUBlockCache->begin();

		for(map<_U32,cache_item>::iterator e = m_pmapMUBlockCache->begin();
			e != m_pmapMUBlockCache->end();e++)
		{
			if(e->second.lastAccessed < oldest_elem->second.lastAccessed)
			{
				oldest_elem = e;
			}
		}

		pBlock = oldest_elem->second.pBlk;
		m_pmapMUBlockCache->erase(oldest_elem);
	}
	else
	{
		pBlock = new _U8[m_pSB->BlockSize];
		ASSERT(pBlock);
		if(NULL == pBlock)
		{
			m_dwLastError = ERR_OUT_OF_MEMORY;
			return false;
		}
	}


	// Add to map...

	cache_item ci;
	ci.lastAccessed = m_lb_time;
	ci.pBlk = pBlock;

	m_pmapMUBlockCache->insert(map<_U32,cache_item>::value_type(block,ci));


	// Go Find...

	LARGE_INTEGER liTmp;
	liTmp.QuadPart = m_llBeginFileOffset + ((_U64)block * (_U64)m_pSB->BlockSize);

	if(!SetFilePointerEx(m_hFile,liTmp,NULL,FILE_BEGIN))
	{
		m_dwLastError = ERR_READING_FILE;
		return false;
	}
	
	
	// Read
	
	DWORD cbRead = 0;
	if(!ReadFile(m_hFile,pBlock,m_pSB->BlockSize,&cbRead,NULL))
	{
		m_dwLastError = ERR_READING_FILE;
		return false;
	}


	// Copy & return

	CopyMemory(pBlk,pBlock,m_pSB->BlockSize);
	return true;
}


/////////////////////////////////////////////////////////////////////

#ifdef _DEBUG

bool ReiserReader::DumpFSTree()
{
	ASSERT(m_fInit);
	if(!m_fInit)
	{
		m_dwLastError = ERR_NOT_INIT;
		return false;
	}

	return RecDumpFSTree(m_pSB->RootBlock);
}

/////////////////////////////////////////////////////////////////////

bool ReiserReader::RecDumpFSTree(_U32 block)
{
	ASSERT(m_fInit);

	_U8* pBuffer = new _U8[m_pSB->BlockSize];
	ASSERT(pBuffer);
	if(NULL == pBuffer)
	{
		return 0;
	}

	if(!LoadBlock(block,pBuffer))
	{
		return 0;
	}

	
	// Overlay

	reiser_block_header *prbh = (reiser_block_header*)pBuffer;
	reiser_key *pKey = (reiser_key*)(prbh + 1);

	// Internal Node or Leaf?
	
	_tprintf(_T("In Block: %0.8X Items: %0.4X Level: %0.4X\n"),block,prbh->cItems,prbh->Level);

	if(prbh->Level > 1)
	{
		int n=0;
		bool ret = false;

		while(n <= prbh->cItems) // && match <= 0)
		{
			reiser_pointer* pPointer = 
				(reiser_pointer*)(pBuffer + sizeof(reiser_block_header) + 
				(prbh->cItems * sizeof(reiser_key)) + (n * sizeof(reiser_pointer)));

            ret = RecDumpFSTree(pPointer->BlockNumber);

			pKey++;
			n++;
		}

		return ret;
	}
	else
	{
		// Leaf node... 

		reiser_item_header* pItemHdr = (reiser_item_header*)(prbh + 1);

		int n = 0;

		while(	n < prbh->cItems)
		{
			_tprintf(_T("(Item)Key: %0.8X %0.8X %0.8X %0.8X Count: %0.4X Length: %0.4X Location: %0.4X Version: %0.4X\n"),
						pItemHdr->Key.v1.DirectoryID,
						pItemHdr->Key.v1.ObjectID,
						pItemHdr->Key.v1.Offset,
						pItemHdr->Key.v1.Type,
						pItemHdr->Count,
						pItemHdr->Length,
						pItemHdr->Location,
						pItemHdr->Version);

			pItemHdr++;
			n++;
		}
	}
    
	return 1;
}

#endif // _DEBUG

////////////////////////////////////////////////////////////////////