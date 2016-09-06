/////////////////////////////////////////////////////////////////////
//
//
//
//
//
/////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <tchar.h>
#include <stdio.h>
#include "MMcGFAT.h"

/////////////////////////////////////////////////////////////////////

// TODO: Turn this into a class

// Call this function with pItems == NULL to get count of FAT Items
// (Files & Directories). Then Call with pItems == Buffer with
// enough space. ie pItems = new FATItem[cItems];
// This will fill all you FATItem structs

// This function expects the entire directory to be passed in pDir
// and will only parse what it is given no further and return true

BOOL ParseFATDirectory(FATInfo *pfi, BYTE *pDir, 
					   DWORD dwDirBufLen, FATItem *pItems, 
					   DWORD *pcItems)
{
	// TODO: Assert Params
	// pcItems canNOT be NULL!

	Dir_Entry* pDirEntry = (Dir_Entry*)(pDir);
	DWORD cFiles = 0;	// Count of actual valid files we've processed
						// mostly not == i, which is number of entries
						// processed

	// While not reached end of entries (ie DIRName != 0) and
	// still got stuff to parse i*sizeof(Dir_Entry) < dwBufLen

	for(int i=0;pDirEntry->DIR_Name[0] != 0 && 
		i*sizeof(Dir_Entry) < dwDirBufLen;
		pDirEntry++,i++)
	{
		TCHAR szName[MAX_PATH];
		LFN_Entry* pLFN = (LFN_Entry*)pDirEntry;

		// TODO: Warning, Japanese character == 0xE5 is encoded 
		// as 0x05 here, which i don;t check for 

		if(pLFN->LDIR_Ord == 0xE5) // Deleted?
		{
			continue;
		}

		if((pLFN->LDIR_Attr & ATTR_LONG_NAME_MASK) == ATTR_LONG_NAME)
		{
			int nEntries = FormatLongFilename(szName,pDirEntry);

			// Process Last Record as standard Short Entry
			
			if(0 == nEntries) // TODO: Real entry? Bail here??
			{
				continue;
			}
			
			pDirEntry+=nEntries;
			i+=nEntries;
		}
		else
		{
			// Volume Label?

			if((pDirEntry->DIR_Attr & ATTR_VOLUME_ID) == ATTR_VOLUME_ID)
			{
				continue;
			}

			FormatOldFilename(szName,pDirEntry->DIR_Name);
		}
		
		//OutputDebugString(szName);
		//OutputDebugString(_T("\n"));

		if(pItems)
		{
			if(	cFiles < *pcItems &&
				!FillFATItemStruct(szName,pDirEntry,&pItems[cFiles]))
			{
				return FALSE;
			}
		}
		cFiles++;
	}

	if(NULL == pItems)
	{
		*pcItems = cFiles;
	}

	return TRUE;
}

/////////////////////////////////////////////////////////////////////

DWORD FirstSectorOfCluster(DWORD n, FATInfo *pfi)
{
	return (((n - 2 ) *	pfi->bs.bios_param.BPB_SecPerClus)
		+ pfi->FirstDataSector);
}

/////////////////////////////////////////////////////////////////////

BOOL InitFATInfo(BootSector *pbs, FATInfo *pfi)
{
	CopyMemory(&pfi->bs,pbs,sizeof(BootSector)); 

	// Root Sector Count

	DWORD RootDirSectors = 
		((pbs->bios_param.BPB_RootEntCnt * 32) + 
		(pbs->bios_param.BPB_BytsPerSec-1))	/ pbs->bios_param.BPB_BytsPerSec;

	
	// FAT Size

	if(pbs->bios_param.BPB_FATSz16 != 0)
	{
		pfi->FATSz = pbs->bios_param.BPB_FATSz16;
	}
	else
	{
		pfi->FATSz = pbs->FAT_Dep.fat32.BPB_FATSz32;
	}

	// First Data Sector 

	pfi->FirstDataSector = pbs->bios_param.BPB_RsvdSecCnt + 
		(pbs->bios_param.BPB_NumFATs * pfi->FATSz) + RootDirSectors;


	// Determine the count of sectors in Data region of volume

	if(pbs->bios_param.BPB_TotSec16 != 0)
	{
		pfi->TotSec = pbs->bios_param.BPB_TotSec16;
	}
	else
	{
		pfi->TotSec = pbs->bios_param.BPB_TotSec32;
	}
	
	pfi->DataSec = pfi->TotSec - 
		(pbs->bios_param.BPB_RsvdSecCnt + 
		(pbs->bios_param.BPB_NumFATs * pfi->FATSz) + RootDirSectors);


	pfi->CountofClusters = pfi->DataSec / pbs->bios_param.BPB_SecPerClus;

	if(pfi->CountofClusters < 4085) 
	{
		pfi->FATType = FAT12;/* Volume is FAT12 */

		pfi->FAT16RootDirSector = pbs->bios_param.BPB_RsvdSecCnt + 
		(pbs->bios_param.BPB_NumFATs * pfi->FATSz);
	} 
	else if(pfi->CountofClusters < 65525) 
	{
		pfi->FATType = FAT16;/* Volume is FAT16 */

		pfi->FAT16RootDirSector = pbs->bios_param.BPB_RsvdSecCnt + 
		(pbs->bios_param.BPB_NumFATs * pfi->FATSz);
	}
	else 
	{
		pfi->FATType = FAT32;/* Volume is FAT32 */

		pfi->FAT32RootDirCluster = 
			pfi->bs.FAT_Dep.fat32.BPB_RootClus;
	}

	// Size of a cluster

	pfi->cbCluster = pbs->bios_param.BPB_BytsPerSec * 
		pbs->bios_param.BPB_SecPerClus;

	return TRUE;
}

/////////////////////////////////////////////////////////////////////

BOOL GetFATEntryForCluster(HANDLE hFile,LARGE_INTEGER *plrgFileOffset, DWORD Cluster,FATInfo* pfi,
						   DWORD* pdwEntry)
{
	if(Cluster > pfi->CountofClusters + 1) // Valid => see specs
	{
		return FALSE;
	}

	if(pfi->FATType == FAT12)
	{
		DWORD FATOffset = Cluster + (Cluster / 2);

		DWORD SecNum = pfi->bs.bios_param.BPB_RsvdSecCnt + 
			(FATOffset / pfi->bs.bios_param.BPB_BytsPerSec);

		DWORD FATEntOffset = FATOffset % pfi->bs.bios_param.BPB_BytsPerSec;

		// Sector Boundary Case: Load SecNum & SecNum+1 because entries can
		// span sectors only on FAT12

		// Read That Sector

		BYTE *SecBuff = new BYTE[pfi->bs.bios_param.BPB_BytsPerSec * 2];
		if(NULL == SecBuff)
		{
			return FALSE;
		}

		ULARGE_INTEGER lrgOffset;
		lrgOffset.QuadPart = plrgFileOffset->QuadPart + 
			(((LONGLONG)SecNum) * pfi->bs.bios_param.BPB_BytsPerSec);

		DWORD cbRead;
		DWORD dwOut = SetFilePointer(hFile,lrgOffset.LowPart,(PLONG)&lrgOffset.HighPart,FILE_BEGIN);

		if(!dwOut &&
			SecNum != 0)
		{
			return FALSE;
		}


		if(!ReadFile(hFile,SecBuff,pfi->bs.bios_param.BPB_BytsPerSec * 2,
			&cbRead,NULL))
		{
			delete SecBuff;
			return FALSE;
		}

		WORD wEntry = *((WORD*)&SecBuff[FATEntOffset]);

		if(Cluster & 0x00000001) // ODD
		{
			wEntry = wEntry >> 4;
		}
		else // EVEN
		{
			wEntry = wEntry & 0x0FFF;
		}

		*pdwEntry = (DWORD)wEntry;
	}
	else 
	{
		DWORD FATOffset = 0;
		if(pfi->FATType == FAT16)
		{
			FATOffset = Cluster * 2;
		}
		else
		{
			FATOffset = Cluster * 4;
		}

		DWORD SecNum = pfi->bs.bios_param.BPB_RsvdSecCnt + 
			(FATOffset / pfi->bs.bios_param.BPB_BytsPerSec);

		DWORD FATEntOffset = FATOffset % pfi->bs.bios_param.BPB_BytsPerSec;


		// Read That Sector


		ULARGE_INTEGER lrgOffset;
		lrgOffset.QuadPart = plrgFileOffset->QuadPart + 
			(((LONGLONG)SecNum) * pfi->bs.bios_param.BPB_BytsPerSec);

		DWORD dwOut = SetFilePointer(hFile,lrgOffset.LowPart,(PLONG)&lrgOffset.HighPart,FILE_BEGIN);

		if(!dwOut &&
			SecNum != 0)
		{
			return FALSE;
		}

		BYTE *SecBuff = new BYTE[pfi->bs.bios_param.BPB_BytsPerSec];
		if(NULL == SecBuff)
		{
			return FALSE;
		}

		DWORD cbRead;
		if(!ReadFile(hFile,SecBuff,pfi->bs.bios_param.BPB_BytsPerSec,
			&cbRead,NULL))
		{
			delete SecBuff;
			return FALSE;
		}

		// Fetch Contents & return

		if(pfi->FATType == FAT16)
		{
			*pdwEntry = (DWORD)*((WORD *)&SecBuff[FATEntOffset]);
		}
		else
		{
			*pdwEntry = (*((DWORD*) &SecBuff[FATEntOffset])) & 0x0FFFFFFF;
		}
		
		delete SecBuff;
	}

	return TRUE;
}

/////////////////////////////////////////////////////////////////////

DWORD TotalLinkedClustersForCluster(HANDLE hFile, LARGE_INTEGER *plrgFileOffset, 
									FATInfo *pfi, DWORD dwCluster)
{
	DWORD cClusters = 0;

	do
	{
		if(!GetFATEntryForCluster(hFile,plrgFileOffset,dwCluster,pfi,&dwCluster))
		{
			// error reading FAT?
			break;
		}

		cClusters++;
	}while(!IsEndOfFileCluster(dwCluster,pfi) &&
		   !IsFreeCluster(dwCluster,pfi) &&
		   !IsBadCluster(dwCluster,pfi));

	return cClusters;
}

/////////////////////////////////////////////////////////////////////

BOOL ReadEntireFileFromCluster(HANDLE hFile,LARGE_INTEGER *plrgFileOffset, FATInfo *pfi,
					DWORD dwCluster,BYTE* pBuf,DWORD dwBufLen)
{
	BOOL IsEOF = FALSE;
	DWORD cClusters = 0;
	
	BYTE *pTmp = new BYTE[pfi->cbCluster];
	if(NULL == pTmp)
	{
		return FALSE;
	}

	DWORD cbRead = 0;
	do
	{
		if(!ReadCluster(hFile,plrgFileOffset,dwCluster,pfi,pTmp,pfi->cbCluster))
		{
			break;
		}
		cbRead+=pfi->cbCluster;
		
		// Next Cluster? <= FAT

		if(!GetFATEntryForCluster(hFile,plrgFileOffset,dwCluster,pfi,&dwCluster) || 
			IsFreeCluster(dwCluster,pfi))
		{
			// error reading FAT?
			break;
		}


		IsEOF = IsEndOfFileCluster(dwCluster,pfi);

		if(cbRead > dwBufLen)
		{
			if(IsEOF)
			{
				CopyMemory(&pBuf[cClusters * pfi->cbCluster],
					pTmp,dwBufLen -  (cClusters * pfi->cbCluster));
			}
			else
			{
				// we don't have enough buffer => Call ClusterCount
				// TODO: SetLastError?
				break;
			}
		}
		else
		{
			CopyMemory(&pBuf[cClusters * pfi->cbCluster],pTmp,
				pfi->cbCluster);
		}

		cClusters++;
	}while(!IsEOF);

	delete pTmp;

	return IsEOF;
}

/////////////////////////////////////////////////////////////////////

BOOL IsEndOfFileCluster(DWORD dwCluster, FATInfo *pfi)
{
	BOOL IsEOF = FALSE;
	if(pfi->FATType == FAT12)
	{
		if(dwCluster >= 0x0FF8)
		{
			IsEOF = TRUE;
		}
	}
	else if(pfi->FATType == FAT16)
	{
		if(dwCluster >= 0xFFF8)
		{
			IsEOF = TRUE;
		}
	}
	else if(pfi->FATType == FAT32)
	{
		if(dwCluster >= 0x0FFFFFF8)
		{
			IsEOF = TRUE;
		}
	}

	return IsEOF;
}

/////////////////////////////////////////////////////////////////////

BOOL IsFreeCluster(DWORD dwCluster, FATInfo *pfi)
{
	BOOL IsFREE = FALSE;
	if(pfi->FATType == FAT12 || pfi->FATType == FAT16)
	{
		if(dwCluster == 0x0000)
		{
			IsFREE = TRUE;
		}
	}
	else if(pfi->FATType == FAT32)
	{
		if((dwCluster & 0x0FFFFFFF) == 0)
		{
			IsFREE = TRUE;
		}
	}

	return IsFREE;
}

/////////////////////////////////////////////////////////////////////

BOOL IsBadCluster(DWORD dwCluster, FATInfo *pfi)
{
	BOOL IsBAD = FALSE;
	if(pfi->FATType == FAT12)
	{
		if(dwCluster == 0x0FF7)
		{
			IsBAD = TRUE;
		}
	}
	else if(pfi->FATType == FAT16)
	{
		if(dwCluster == 0xFFF7)
		{
			IsBAD = TRUE;
		}
	}
	else if(pfi->FATType == FAT32)
	{
		if(dwCluster == 0x0FFFFFF7)
		{
			IsBAD = TRUE;
		}
	}

	return IsBAD;
}

/////////////////////////////////////////////////////////////////////

BOOL ReadCluster(HANDLE hFile,LARGE_INTEGER *plrgFileOffset, DWORD dwCluster,
				 FATInfo *pfi,BYTE* pBuf,DWORD dwBufLen)
{
	// TODO: Check Params

	if(dwBufLen < pfi->cbCluster)
	{
		return FALSE;
	}

	// Check if this is a bad cluster?

	if(IsBadCluster(dwCluster,pfi))
	{
		return FALSE;
	}

	ULARGE_INTEGER lrgOffset;
	lrgOffset.QuadPart = plrgFileOffset->QuadPart + 
		(((LONGLONG)FirstSectorOfCluster(dwCluster,pfi)) *
		pfi->bs.bios_param.BPB_BytsPerSec);

	DWORD cbRead;

	if (!SetFilePointer(hFile,lrgOffset.LowPart,(PLONG)&lrgOffset.HighPart,FILE_BEGIN) ||
		!ReadFile(hFile,pBuf,pfi->cbCluster,&cbRead,NULL) || 
		cbRead != pfi->cbCluster)
	{
		return FALSE;
	}

	return TRUE;
}

/////////////////////////////////////////////////////////////////////

int FormatLongFilename(LPTSTR pszName, Dir_Entry *pDir) // Assumption: pszName at least MAX_PATH
{
	WORD FileName[MAX_PATH]={0};
	int nameLen = 0;

	//int nEntries = (~LAST_LONG_ENTRY & pLFN->LDIR_Ord);
	// How many entries in this long name, could use the doc'd way
	// => ((~LAST_LONG_ENTRY & pLFN->LDIR_Ord), but won't work too well
	// for deleted files as pLFN->LDIR_Ord == 0xE5
	// We fail back to this if our walking algorithm nEntries is insane

	int nEntries=0;
	LFN_Entry* pWalkLFN = (LFN_Entry*)pDir;
	while(nEntries < 0x40 && 
		(pWalkLFN->LDIR_Attr & ATTR_LONG_NAME_MASK) == ATTR_LONG_NAME)
	{
		nEntries++;
		pWalkLFN++;
	}

	// Sanity Check and fail safe

    if(nEntries >= 0x40)
	{
        LFN_Entry* pLFN = (LFN_Entry*)pDir;
		nEntries = ((~LAST_LONG_ENTRY & pLFN->LDIR_Ord) >= 0x40) ? 0 : 
			(~LAST_LONG_ENTRY & pLFN->LDIR_Ord);
	}


	// Parse

	for(int j=0;j<nEntries;j++)
	{
		LFN_Entry* pLFN = (LFN_Entry*)pDir + ((nEntries - 1) - j);
		int k=0;

		BOOL nameDone = FALSE;

		while(k < 5 && !nameDone)
		{
			FileName[nameLen++] = pLFN->LDIR_Name1[k++];
			nameDone = (k<5 && pLFN->LDIR_Name1[k] == 0);
		}
		k=0;
		while(k < 6 && !nameDone)
		{
			FileName[nameLen++] = pLFN->LDIR_Name2[k++];
			nameDone = (k<6 && pLFN->LDIR_Name2[k] == 0);
		}
		k=0;
		while(k < 2 && !nameDone)
		{
			FileName[nameLen++] = pLFN->LDIR_Name3[k++];
			nameDone = (k<2 && pLFN->LDIR_Name3[k] == 0);
		}
	}

	FileName[nameLen] = 0; // Null Term

#ifdef _UNICODE
	_tcscpy(pszName,(LPTSTR)FileName);
#else
	WideCharToMultiByte(CP_ACP,0,(LPCWSTR)FileName,-1,pszName,MAX_PATH,NULL,NULL);
#endif

	// Trim Trailing Spaces

	int x=nameLen-1;
	while(x > 0 && pszName[x] == 0x20)
	{
		pszName[x--] = 0;
	}

	return nEntries;
}

/////////////////////////////////////////////////////////////////////

void FormatOldFilename(LPTSTR pszName, BYTE *pName)
{
	int j=0;
	while(pName[j] != 0x20 && j<8)
	{
		if(0 == j)
		{
			if(pName[j] == 0xE5) // Deleted!
			{
				pszName[j] = _T('?');
				j++;
				continue;
			}
		}
		pszName[j] = (TCHAR)pName[j];
		j++;
	}

	if(pName[8] != 0x20)
	{
		pszName[j] = _T('.');
		pszName[j+1] = pName[8];
		pszName[j+2] = pName[9];
		pszName[j+3] = pName[10];
		pszName[j+4] = _T('\0');
	}
	else
	{
		pszName[j] = _T('\0');
	}
}

/////////////////////////////////////////////////////////////////////

BOOL FillFATItemStruct(LPTSTR pszName,Dir_Entry* pDir,FATItem *pItem)
{
	// TODO: Assert Params

	_tcsncpy(pItem->szName,pszName,MAX_PATH);
	
	pItem->dwAttr = pDir->DIR_Attr;
	pItem->dwFirstCluster = MAKELONG(pDir->DIR_FstClusLO,pDir->DIR_FstClusHI);
	pItem->dwSize = pDir->DIR_FileSize;
	
	pItem->fDeleted = (pDir->DIR_Name[0] == 0xE5);

	pItem->dateAccess = pDir->DIR_LstAccDate;
	pItem->dateCreate = pDir->DIR_CrtDate;
	pItem->timeCreate = pDir->DIR_CrtTime;
	pItem->dateModify = pDir->DIR_WrtDate;
	pItem->timeModify = pDir->DIR_WrtTime;

	return TRUE;
}

/////////////////////////////////////////////////////////////////////

void FormatAttributes(LPTSTR pszAttr, BYTE Attributes)
{
	pszAttr[0] = _T('\0');

	if(ATTR_DIRECTORY == (ATTR_DIRECTORY & Attributes))
	{
		_tcscat(pszAttr,_T("D"));
	}
	if(ATTR_READ_ONLY == (ATTR_READ_ONLY & Attributes))
	{
		_tcscat(pszAttr,_T("R"));
	}
	if(ATTR_HIDDEN == (ATTR_HIDDEN & Attributes))
	{
		_tcscat(pszAttr,_T("H"));
	}
	if(ATTR_SYSTEM == (ATTR_SYSTEM & Attributes))
	{
		_tcscat(pszAttr,_T("S"));
	}
	if(ATTR_ARCHIVE == (ATTR_ARCHIVE & Attributes))
	{
		_tcscat(pszAttr,_T("A"));
	}
	if(ATTR_VOLUME_ID == (ATTR_VOLUME_ID & Attributes))
	{
		_tcscat(pszAttr,_T("<Volume ID>"));
	}
}

/////////////////////////////////////////////////////////////////////