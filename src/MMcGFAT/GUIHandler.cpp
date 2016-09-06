/////////////////////////////////////////////////////////////////////
// 25/MAR/2005
//
//
//
//
//
/////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <commctrl.h>
#include "GUIHandler.h"

/////////////////////////////////////////////////////////////////////

int compare_pitems( const void *arg1, const void *arg2 );

/////////////////////////////////////////////////////////////////////

struct COL
{
	LPCTSTR lpszCol;
	int nWidth;
};

struct COL cols[] = 
{
	{_T("Name"),100},
	{_T("Created"),110},
	{_T("Modified"),110},
	{_T("Access"),110},
	{_T("Size"),75},
	{_T("Attributes"),50},
	{_T("First Cluster"),100},
	{_T("Deleted"),25},
};

/////////////////////////////////////////////////////////////////////

#define NO_OF_COLUMNS (sizeof(cols) / sizeof(COL))

/////////////////////////////////////////////////////////////////////

#define HARDCODED_SECTOR_SIZE 512

/////////////////////////////////////////////////////////////////////

// TODO:	Extend Error Handling From MMcGNTFS to This Class so user
//			get error cause!!

bool GUIFATFSHandler::Init(HANDLE hFile, LONGLONG llBeginFileOffset, 
						   HWND hWndListCtrl)
{
	if(NULL == hFile || NULL == hWndListCtrl)
	{
		return FALSE;
	}

	m_hFile = hFile;
	m_llBeginFileOffset = llBeginFileOffset;
	m_hWndListCtrl = hWndListCtrl;

	BYTE *pBuffer = new BYTE[HARDCODED_SECTOR_SIZE];
	if(NULL == pBuffer)
	{
		return FALSE;
	}

	LARGE_INTEGER liTmp;
	liTmp.QuadPart = m_llBeginFileOffset;
	DWORD cbRead;

	if (!SetFilePointerEx(m_hFile,liTmp,NULL,FILE_BEGIN) ||
		!ReadFile(m_hFile,pBuffer,HARDCODED_SECTOR_SIZE,&cbRead,NULL) || 
		cbRead != HARDCODED_SECTOR_SIZE)
	{
		delete pBuffer;
		return FALSE;
	}

	BootSector* pbs = (BootSector*)pBuffer;

	// Init FatInfo Structure

	m_pfi = new FATInfo;
	if(NULL == m_pfi)
	{
		delete pBuffer;
		return FALSE;
	}

	if(!InitFATInfo(pbs, m_pfi))
	{
		delete pBuffer;
		delete m_pfi;
		m_pfi = NULL;

		return FALSE;
	}

	delete pBuffer;

	// Setup ListCtrl

	ListView_DeleteAllItems(m_hWndListCtrl);
	DWORD old = ListView_GetExtendedListViewStyle(m_hWndListCtrl);
	ListView_SetExtendedListViewStyle(m_hWndListCtrl,old | LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);

	// Delete Columns

	HWND hWndHeader = ListView_GetHeader(m_hWndListCtrl);
	int cCols = Header_GetItemCount(hWndHeader);
	for(int i=cCols-1;i>=0;i--)
	{
		ListView_DeleteColumn(m_hWndListCtrl,i);
	}

	// Set Image List

	SHFILEINFO FileInfo;
	HIMAGELIST hImageList = 
		(HIMAGELIST)SHGetFileInfo(_T(".dll"),NULL,&FileInfo,sizeof(FileInfo),
		SHGFI_SMALLICON | SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES);

	ListView_SetImageList(m_hWndListCtrl, hImageList, LVSIL_SMALL);

	// Columns

	for(int i=0;i<NO_OF_COLUMNS;i++)
	{
		LVCOLUMN lvc = {0};
		lvc.iSubItem = i;
		lvc.cx = cols[i].nWidth;
		lvc.pszText = (LPTSTR)cols[i].lpszCol;
		lvc.mask = LVCF_TEXT | LVCF_WIDTH;
		ListView_InsertColumn(m_hWndListCtrl,i,&lvc);
	}
	
	// Done

	m_fInit = TRUE;
	return TRUE;
}

/////////////////////////////////////////////////////////////////////

bool GUIFATFSHandler::DisplayRootDirectory()
{
	if(!m_fInit)
	{
		return FALSE;
	}

	DWORD cbRootDir = 0;

	if(FAT32 == m_pfi->FATType)
	{
		LARGE_INTEGER liTmp;
		liTmp.QuadPart = m_llBeginFileOffset;
		cbRootDir = TotalLinkedClustersForCluster(m_hFile, 
			&liTmp,m_pfi,m_pfi->FAT32RootDirCluster)
			* m_pfi->cbCluster;
	}
	else
	{
		cbRootDir = 
			m_pfi->bs.bios_param.BPB_RootEntCnt*sizeof(Dir_Entry);
	}

	BYTE *pRootDir = new BYTE[cbRootDir];
	if(NULL == pRootDir)
	{
		return FALSE;
	}

	if(FAT32 == m_pfi->FATType)
	{
		LARGE_INTEGER liTmp;
		liTmp.QuadPart = m_llBeginFileOffset;
		if(!ReadEntireFileFromCluster(m_hFile,&liTmp,
			m_pfi,m_pfi->FAT32RootDirCluster,
			pRootDir,cbRootDir))
		{
			delete pRootDir;
			return FALSE;
		}
	}
	else
	{
		ULARGE_INTEGER lrgOffset;
		lrgOffset.QuadPart = 
			m_llBeginFileOffset + 
			(m_pfi->FAT16RootDirSector * m_pfi->bs.bios_param.BPB_BytsPerSec);

		DWORD cbRead;
        if (!SetFilePointer(m_hFile,lrgOffset.LowPart,
			(PLONG)&lrgOffset.HighPart,FILE_BEGIN) ||
			!ReadFile(m_hFile,pRootDir,cbRootDir,&cbRead,NULL))
		{
			delete pRootDir;
			return FALSE;
		}
	}

	if(!DisplayDirectory(pRootDir, cbRootDir))
	{
		delete pRootDir;
		return FALSE;
	}

	delete pRootDir;

	EmptyPath();
	return AddDirectoryToPath(0,_T(""));
}

/////////////////////////////////////////////////////////////////////

bool GUIFATFSHandler::GetCurrentPath(LPTSTR lpszPath, DWORD cchMaxPath)
{
	stack<PATH_COMPONENT*> stkTmp;
	
	while(!m_stkPath.empty())
	{
		stkTmp.push(m_stkPath.top());
		m_stkPath.pop();
	}

	lpszPath[0] = 0;
	while(!stkTmp.empty())
	{
		size_t len = _tcslen(lpszPath);
		PATH_COMPONENT* pComp = stkTmp.top();
		ASSERT(pComp);

		if(cchMaxPath > len)
		{
			_tcsncat(lpszPath,pComp->szName,cchMaxPath - len - 2);
			_tcscat(lpszPath,_T("/"));
			lpszPath[cchMaxPath-1] = 0;
		}

		m_stkPath.push(pComp); // Restore Path Stack
		stkTmp.pop();
	}

	return TRUE;
}

/////////////////////////////////////////////////////////////////////

bool GUIFATFSHandler::PerformDefaultItemAction()
{
	if(!m_fInit)
	{
		return FALSE;
	}

	int cItems = ListView_GetItemCount(m_hWndListCtrl);
	int nSel = -1;
	LPARAM lParam;

	for(int i=0;i<cItems;i++)
	{
		LVITEM lvi;
		lvi.mask = LVIF_STATE | LVIF_PARAM;
		lvi.stateMask = LVIS_SELECTED;
		lvi.iItem = i;
		lvi.iSubItem = 0;
		
		ListView_GetItem(m_hWndListCtrl, &lvi);
		if(lvi.state == LVIS_SELECTED)
		{
			nSel = i;
			lParam = lvi.lParam;
			break;
		}
	}

	if(-1 == nSel)
	{
		return TRUE;
	}

	FATItem* pItem = (FATItem*)lParam;
	ASSERT(pItem);

	if(NULL == pItem)
	{
		return FALSE;
	}

	if((pItem->dwAttr & ATTR_DIRECTORY) == ATTR_DIRECTORY)
	{
		TCHAR szTmp[FAT_NAME_LEN];
		_tcsncpy(szTmp,pItem->szName,FAT_NAME_LEN-1);
		szTmp[FAT_NAME_LEN-1] = 0;

		LONGLONG llInTmp = pItem->dwFirstCluster;

		ChangeDirectory(pItem->dwFirstCluster);

		return AddDirectoryToPath(llInTmp, szTmp);
	}
	else
	{
        CreateTmpViewOfItem(pItem);
	}
	
	return TRUE;
}

/////////////////////////////////////////////////////////////////////

bool GUIFATFSHandler::ShowContextMenu()
{
	return FALSE;
}

/////////////////////////////////////////////////////////////////////

bool GUIFATFSHandler::CDUp()
{
	if(1 == m_stkPath.size()) // Already at root
	{
		return TRUE;
	}

	ASSERT(!m_stkPath.empty());

	m_stkPath.pop(); // Pop Current Dir

	PATH_COMPONENT* pComp = (PATH_COMPONENT*)m_stkPath.top();
	ASSERT(pComp);
	
    return ChangeDirectory((DWORD)pComp->llDir);
}

/////////////////////////////////////////////////////////////////////

void GUIFATFSHandler::Close()
{
	m_fInit = FALSE;

	if(m_pfi)
	{
		delete m_pfi;
	}
	m_pfi = NULL;

	if(m_pItems)
	{
		delete []m_pItems;
	}
	m_pItems = NULL;
	m_cItems = 0;

	EmptyPath();

	return;
}

/////////////////////////////////////////////////////////////////////

void GUIFATFSHandler::CloseAndDestroy()
{
	Close();
	delete this;
}

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////

bool GUIFATFSHandler::ChangeDirectory(DWORD dwCluster)
{
	ASSERT(m_fInit);

	if(0 == dwCluster)
	{
		return DisplayRootDirectory();
	}

	LARGE_INTEGER liTmp;
	liTmp.QuadPart = m_llBeginFileOffset;

	DWORD cbDir = m_pfi->cbCluster * 
		TotalLinkedClustersForCluster(m_hFile, &liTmp,m_pfi,dwCluster);

	BYTE *pDir = new BYTE[cbDir];
	ASSERT(pDir);

	if (NULL == pDir ||
		!ReadEntireFileFromCluster(m_hFile,&liTmp,m_pfi,
									dwCluster,pDir,cbDir) ||
		!DisplayDirectory(pDir, cbDir))
	{
		delete pDir;
		return FALSE;
	}

	delete pDir;	
	return TRUE;
}

/////////////////////////////////////////////////////////////////////

bool GUIFATFSHandler::CreateTmpViewOfItem(FATItem* pItem)
{
	ASSERT(pItem);

	BYTE *pBuf = new BYTE[pItem->dwSize];
	ASSERT(pBuf);
	if(NULL == pBuf)
	{
		return FALSE;
	}

	// TODO: This isn't gonna be good for large files? 
	// read & write cluster by cluster is possible solution

	LARGE_INTEGER liTmp;
	liTmp.QuadPart = m_llBeginFileOffset;
    if(!ReadEntireFileFromCluster(m_hFile,&liTmp,m_pfi, 
			pItem->dwFirstCluster,pBuf,pItem->dwSize))
	{
		delete pBuf;
		return FALSE;
	}


	TCHAR szPath[MAX_PATH];
	GetTempPath(MAX_PATH,szPath);
	GetTempFileName(szPath,NULL,0,szPath);

	// Try To Keep Extension the same as on orig file so
	// ShellExecute Works

	LPTSTR pExt = _tcsrchr(pItem->szName,_T('.'));
	if(NULL != pExt)
	{
		// TODO: Check This Doesnt Overflow!!
		_tcsncat(szPath,pExt,MAX_PATH);
	}

    HANDLE hNew = CreateFile(szPath,GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);

	if(INVALID_HANDLE_VALUE == hNew)
	{
		delete pBuf;
		return FALSE;
	}

	DWORD cbRead;
	if(!WriteFile(hNew,pBuf,pItem->dwSize,&cbRead,NULL))
	{
		CloseHandle(hNew);
		delete pBuf;
		return FALSE;
	}

	CloseHandle(hNew);

	if(32 > (DWORD)(DWORD_PTR)ShellExecute(GetParent(m_hWndListCtrl),_T("open"),
				szPath,szPath,NULL,SW_SHOWNORMAL)) 
	{
		// Error Opening, open in notepad

		ShellExecute(GetParent(m_hWndListCtrl),_T("open"),_T("notepad.exe %s"),
			szPath,NULL,SW_SHOWNORMAL);
	}

	// TODO: Find Some Way To Delete this tmp file? necessary?

	delete pBuf;
	return TRUE;
}

/////////////////////////////////////////////////////////////////////

bool GUIFATFSHandler::DisplayDirectory(BYTE *pDir, DWORD cbDir)
{
	// Remove anything in the list and its associated data

	ListView_DeleteAllItems(m_hWndListCtrl);

	if(m_pItems)
	{
		delete m_pItems;
		m_pItems = NULL;
		m_cItems = 0;
	}

	// Get Item Count

	if(!ParseFATDirectory(m_pfi, pDir, cbDir, NULL, &m_cItems))
	{
		return FALSE;
	}

	// Get Items

	m_pItems = new FATItem[m_cItems];
	if(NULL == m_pItems)
	{
		return FALSE;
	}

	if(!ParseFATDirectory(m_pfi, pDir, cbDir, m_pItems, &m_cItems))
	{
		return FALSE;
	}

	// Sort 

	qsort(m_pItems,m_cItems,sizeof(FATItem),compare_pitems);

	// Show Items

	if(!DisplayItems())
	{
		delete m_pItems;
		m_pItems = NULL;
		m_cItems = 0;
		return FALSE;
	}

	return TRUE;
}

/////////////////////////////////////////////////////////////////////

bool GUIFATFSHandler::DisplayItems()
{
	ListView_DeleteAllItems(m_hWndListCtrl);

	for(DWORD i=0;i<m_cItems;i++)
	{
		InsertEntry(&m_pItems[i]);
	}

	return TRUE;
}

/////////////////////////////////////////////////////////////////////

bool GUIFATFSHandler::InsertEntry(FATItem *pItem)
{
	SHFILEINFO FileInfo;

	if (ATTR_DIRECTORY == (ATTR_DIRECTORY & pItem->dwAttr))
	{
		SHGetFileInfo(pItem->szName,FILE_ATTRIBUTE_DIRECTORY,
			&FileInfo,sizeof(SHFILEINFO), SHGFI_SMALLICON | 
			SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES );	
	}
	else
	{
		SHGetFileInfo(pItem->szName,NULL,&FileInfo,sizeof(SHFILEINFO), 
		SHGFI_SMALLICON | SHGFI_SYSICONINDEX| SHGFI_USEFILEATTRIBUTES );	
	}

	LV_ITEM lvi = {0};
	lvi.iItem = 0;
	lvi.iSubItem = 0;
	lvi.pszText = pItem->szName;
	lvi.iImage = FileInfo.iIcon;
	lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
	lvi.lParam = (DWORD_PTR)pItem;
	
	int nItem = ListView_InsertItem(m_hWndListCtrl,&lvi);
	if(-1 == nItem)
	{
		return FALSE;
	}

	TCHAR szFmt[64];
	FormatDosTime(szFmt,pItem->dateCreate,pItem->timeCreate);
	lvi.iItem = nItem;
	lvi.iSubItem = 1;
	lvi.pszText = szFmt;
	lvi.mask = LVIF_TEXT;
	ListView_SetItem(m_hWndListCtrl,&lvi);
	
	FormatDosTime(szFmt,pItem->dateModify,pItem->timeModify);
	lvi.iItem = nItem;
	lvi.iSubItem = 2;
	lvi.pszText = szFmt;
	lvi.mask = LVIF_TEXT;
	ListView_SetItem(m_hWndListCtrl,&lvi);
	
	FormatDosTime(szFmt,pItem->dateAccess,0);
	lvi.iItem = nItem;
	lvi.iSubItem = 3;
	lvi.pszText = szFmt;
	lvi.mask = LVIF_TEXT;
	ListView_SetItem(m_hWndListCtrl,&lvi);

	if (ATTR_DIRECTORY != (ATTR_DIRECTORY & pItem->dwAttr))
	{
		FormatSize(szFmt,pItem->dwSize);
		lvi.iItem = nItem;
		lvi.iSubItem = 4;
		lvi.pszText = szFmt;
		lvi.mask = LVIF_TEXT;
		ListView_SetItem(m_hWndListCtrl,&lvi);
	}

	FormatAttributes(szFmt,(BYTE)pItem->dwAttr);
	lvi.iItem = nItem;
	lvi.iSubItem = 5;
	lvi.pszText = szFmt;
	lvi.mask = LVIF_TEXT;
	ListView_SetItem(m_hWndListCtrl,&lvi);

	_sntprintf(szFmt,64,_T("0x%0.8X"),pItem->dwFirstCluster);
	lvi.iItem = nItem;
	lvi.iSubItem = 6;
	lvi.pszText = szFmt;
	lvi.mask = LVIF_TEXT;
	ListView_SetItem(m_hWndListCtrl,&lvi);

	lvi.iItem = nItem;
	lvi.iSubItem = 7;
	lvi.pszText = pItem->fDeleted ? _T("Yes") : _T("No");
	lvi.mask = LVIF_TEXT;
	ListView_SetItem(m_hWndListCtrl,&lvi);

	return TRUE;
}

/////////////////////////////////////////////////////////////////////

void GUIFATFSHandler::FormatSize(LPTSTR pszSize,DWORD size)
{
	if(size >= (1024*1024*1024))
	{
		_sntprintf(pszSize,32,_T("%.2f Gb"),(float)size / (1024*1024*1024));
	}
	else if(size >= (1024*1024))
	{
		_sntprintf(pszSize,32,_T("%.2f Mb"),(float)size / (1024*1024));
	}
	else if(size >= (1024))
	{
		_sntprintf(pszSize,32,_T("%.1f Kb"),(float)size / 1024);
	}
	else
	{
		_sntprintf(pszSize,32,_T("%d Bytes"),size);
	}
}

/////////////////////////////////////////////////////////////////////

void GUIFATFSHandler::FormatDosTime(LPTSTR szTime,WORD date, WORD time)
{
	if(0 == date && 0==time)
	{
		_tcscpy(szTime,_T("?"));
		return;
	}

	FILETIME ft;
	DosDateTimeToFileTime(date,time,&ft);

	// first convert file time (UTC time) to local time
	// then convert that time to system time
	// then convert the system time to a tm struct

	SYSTEMTIME sysTime;
	if (!FileTimeToSystemTime(&ft, &sysTime))
	{
		return;
	}

	struct tm tmp;
	tmp.tm_sec =	sysTime.wSecond;
	tmp.tm_min =	sysTime.wMinute;
	tmp.tm_hour =	sysTime.wHour;
	tmp.tm_mday =	sysTime.wDay;
	tmp.tm_mon =	sysTime.wMonth - 1;       // tm_mon is 0 based
	tmp.tm_year =	sysTime.wYear - 1900;     // tm_year is 1900 based
	tmp.tm_isdst =	0;

	_tcsftime(szTime,32,_T("%d/%b/%y %H:%M:%S"),&tmp);
}

/////////////////////////////////////////////////////////////////////

bool GUIFATFSHandler::AddDirectoryToPath(LONGLONG llDir, LPTSTR lpszName)
{
	// Does This Inode Exist Within our Path??
	// If so just Pop Path To That Point then
	// continue, this handles smybolic links
	// junction points etc, gracefully, well...

	PATH_COMPONENT* pComp = NULL;
	stack<PATH_COMPONENT*> stkTmp;

	bool fFound = FALSE;
	while(!m_stkPath.empty())
	{
		pComp = m_stkPath.top();
		ASSERT(pComp);
		stkTmp.push(pComp); // Need to restore later

		if(llDir == pComp->llDir)
		{
			fFound = TRUE;
			break;
		}
	
		m_stkPath.pop();
	}

	if(!fFound)
	{
		// restore path
		while(!stkTmp.empty())
		{
			pComp = stkTmp.top();
			m_stkPath.push(pComp);
			stkTmp.pop();
		}
	
		// Add New Component

		pComp = new PATH_COMPONENT;
		ASSERT(pComp);
		if(NULL == pComp)
		{
			//m_dwLastError = ERR_OUT_OF_MEMORY;
			return FALSE;
		}

		pComp->llDir = llDir;
		_tcscpy(pComp->szName,lpszName);

		m_stkPath.push(pComp);
	}

	return TRUE;
}

/////////////////////////////////////////////////////////////////////

void GUIFATFSHandler::EmptyPath()
{
	while(!m_stkPath.empty())
	{
		PATH_COMPONENT* pComp = (PATH_COMPONENT*)m_stkPath.top();
		ASSERT(pComp);
		delete pComp;

		m_stkPath.pop();
	}
}

/////////////////////////////////////////////////////////////////////

int compare_pitems( const void *arg1, const void *arg2 )
{
	FATItem *p1 = (FATItem *)arg1;
	FATItem *p2 = (FATItem *)arg2;

	if(  (ATTR_DIRECTORY == (ATTR_DIRECTORY & p1->dwAttr)) && 
		!(ATTR_DIRECTORY == (ATTR_DIRECTORY & p2->dwAttr)))
	{	
		return 1;
	}
	if(  (ATTR_DIRECTORY == (ATTR_DIRECTORY & p2->dwAttr)) && 
		!(ATTR_DIRECTORY == (ATTR_DIRECTORY & p1->dwAttr)))
	{	
		return -1;
	}


	TCHAR *pa, *pb;
	pa = p1->szName;
	pb = p2->szName;

	// skip dot and compare on next letter

	if(_tcslen(pa) > 1 && pa[0] == '.' && pa[1] != '.')
	{
		pa++;
	}

	if(_tcslen(pb) > 1 && pb[0] == '.' && pb[1] != '.')
	{
		pb++;
	}

	int nRet = _tcscmp(pb,pa);

    return nRet;
}

/////////////////////////////////////////////////////////////////////