/////////////////////////////////////////////////////////////////////
// 25/MAR/2005
//
//
//
//
/////////////////////////////////////////////////////////////////////

#include "dllmain.h"
#include <commctrl.h>
#include <commdlg.h>
#include "..\..\inc\viewfs_types.h"
#include "GUIHandler.h"

/////////////////////////////////////////////////////////////////////

int CALLBACK compare_pitems( LPARAM p1, LPARAM p2, LPARAM param );
int compare_items_u64(_U64 a, _U64 b);
int compare_items_text(LPWSTR pa, LPWSTR pb);
int compare_items_filetime(PFILETIME pt1, PFILETIME pt2);

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
	//{_T("MFT Entry"),125},
	//{_T("Deleted"),25},
};

/////////////////////////////////////////////////////////////////////

#define NO_OF_COLUMNS (sizeof(cols) / sizeof(COL))

/////////////////////////////////////////////////////////////////////

// TODO:	Extend Error Handling From MMcGNTFS to This Class so user
//			get error cause!!

bool GUINTFSFSHandler::Init(HANDLE hFile, LONGLONG llBeginFileOffset, 
						   HWND hWndListCtrl)
{
	if(NULL == hFile || NULL == hWndListCtrl)
	{
		return FALSE;
	}

	ASSERT(!m_fInit);
	if(m_fInit)
	{
		return FALSE;
	}

	m_hWndListCtrl = hWndListCtrl;
    
	m_prdr = new NTFSReader();
	if(NULL == m_prdr)
	{
		return FALSE;
	}

	if(!m_prdr->Init(hFile,llBeginFileOffset))
	{
		delete m_prdr;
		m_prdr = NULL;
		return FALSE;
	}

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
	m_sortOrder = -1;
	
	return TRUE;
}

/////////////////////////////////////////////////////////////////////

bool GUINTFSFSHandler::DisplayRootDirectory()
{
	if(!m_fInit)
	{
		return FALSE;
	}

	if(!DisplayDirectory(CORE_FILE_ROOTDIR))
	{
		return FALSE;
	}

	EmptyPath();
	return AddMFTEntryToPath(CORE_FILE_ROOTDIR,_T(""));
}

/////////////////////////////////////////////////////////////////////

bool GUINTFSFSHandler::DisplayDirectory(LONGLONG llDir)
{
	if(!m_fInit)
	{
		return FALSE;
	}

	//
	
	//LONGLONG llParentDir;
	//if (!m_prdr->GetParentMFTEntry(llDir, &llParentDir))
	//{
	//	return FALSE;
	//}
	
	// Get Count

	LONGLONG cFiles = 0;
	if (!m_prdr->ReadDirectory(llDir, NULL, &cFiles))
	{
		return FALSE;
	}

	// Clear Old

	ListView_DeleteAllItems(m_hWndListCtrl);

	if(m_pItems)
	{
		delete m_pItems;
		m_pItems = NULL;
		m_cItems = 0;
	}

	
	// Alloc New

	m_cItems = (int)cFiles;
	m_pItems = new NTFSItem[m_cItems];
	if(NULL == m_pItems)
	{
		return FALSE;
	}
	
	// Set Parent Now So if an error occurs, we can use CD up to change
	// back to originating dir

	// Load

	cFiles = m_cItems;
	if (!m_prdr->ReadDirectory(llDir,m_pItems,&cFiles))
	{
		return FALSE;		
	}

	// Display em

	if(!DisplayItems())
	{
		return FALSE;
	}

	//ColumnSort(0);
	//int c = ListView_GetItemCount(m_hWndListCtrl);
	
	m_sortOrder = 1;
    ListView_SortItems(	m_hWndListCtrl,compare_pitems,
						MAKELPARAM(0,m_sortOrder));


	return TRUE;
}	

/////////////////////////////////////////////////////////////////////

bool GUINTFSFSHandler::DisplayItems()
{
	if(!m_fInit)
	{
		return FALSE;
	}

	ListView_DeleteAllItems(m_hWndListCtrl);

	for(int i=0;i<m_cItems;i++)
	{
		if(!InsertEntry(i,&m_pItems[i]))
		{
			return FALSE;
		}
	}
	return TRUE;
}

/////////////////////////////////////////////////////////////////////

bool GUINTFSFSHandler::InsertEntry(int index, NTFSItem* pItem)
{
	if(!m_fInit)
	{
		return FALSE;
	}

	// Only put Win32 Names in there!

	if(	FILE_NAMESPACE_WIN32 != 
		(pItem->FilenameNamespace& FILE_NAMESPACE_WIN32))
	{
		return TRUE;
	}

	// TODO: alter this for UNICODE!

	
	LPTSTR lpszName;
#ifdef _UNICODE
	lpszName = pItem->szName;
#else
	TCHAR szTmp2[256];
	WideCharToMultiByte(CP_ACP,0,pItem->szName,-1,szTmp2,256,_T("?"),NULL);
	lpszName = szTmp2;
#endif 
	

	// Icon

	SHFILEINFO FileInfo;

	if (IS_DIRECTORY(pItem->dwAttr))
	{
		SHGetFileInfo(	lpszName,FILE_ATTRIBUTE_DIRECTORY,&FileInfo,sizeof(SHFILEINFO),
						SHGFI_SMALLICON | SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES );	
	}
	else
	{
		SHGetFileInfo(lpszName,NULL,&FileInfo,sizeof(SHFILEINFO), 
		SHGFI_SMALLICON | SHGFI_SYSICONINDEX| SHGFI_USEFILEATTRIBUTES );	
	}

	// Insert

	LV_ITEM lvi = {0};
	lvi.iItem = index;
	lvi.iSubItem = 0;
	lvi.iImage = FileInfo.iIcon;
	lvi.lParam = (DWORD_PTR)pItem;
	lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
	lvi.pszText = lpszName;

	int nItem = ListView_InsertItem(m_hWndListCtrl,&lvi);

	TCHAR szFmt[64];

	//{_T("Created"),100},
	FormatTime(szFmt,&pItem->ftCreate);
	lvi.iItem = nItem;
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 1;
	lvi.pszText  = szFmt;
	
	ListView_SetItem(m_hWndListCtrl,&lvi);

	//{_T("Modified"),100},
	FormatTime(szFmt,&pItem->ftMod);
	lvi.iItem = nItem;
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 2;
	lvi.pszText  = szFmt;
	ListView_SetItem(m_hWndListCtrl,&lvi);

    //{_T("Access"),100},
	FormatTime(szFmt,&pItem->ftAccess);
	lvi.iItem = nItem;
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 3;
	lvi.pszText  = szFmt;
	ListView_SetItem(m_hWndListCtrl,&lvi);

	//{_T("Size"),100},
	if(!IS_DIRECTORY(pItem->dwAttr))
	{
		ULARGE_INTEGER uli;
		uli.QuadPart = pItem->size;
		FormatSizeEx(szFmt,&uli);
		lvi.iItem = nItem;
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 4;
		lvi.pszText  = szFmt;
		ListView_SetItem(m_hWndListCtrl,&lvi);
	}

	//{_T("Attributes"),100},
	FormatAttributes(szFmt,pItem->dwAttr);
	lvi.iItem = nItem;
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 5;
	lvi.pszText  = szFmt;
	ListView_SetItem(m_hWndListCtrl,&lvi);

	////{_T("MFT Entry"),100},
	//_sntprintf(szFmt,64,_T("0x%0.16I64X"),pItem->mftIndex);
	//lvi.iItem = nItem;
	//lvi.mask = LVIF_TEXT;
	//lvi.iSubItem = 6;
	//lvi.pszText  = szFmt;
	//ListView_SetItem(m_hWndListCtrl,&lvi);
	//
	////{_T("Deleted"),100},
	//lvi.iItem = nItem;
	//lvi.mask = LVIF_TEXT;
	//lvi.iSubItem = 7;
	//lvi.pszText  = pItem->fDeleted ? _T("Yes") : _T("No");
	//ListView_SetItem(m_hWndListCtrl,&lvi);

	return TRUE;
}

/////////////////////////////////////////////////////////////////////

void GUINTFSFSHandler::FormatAttributes(LPTSTR lpszFmt, DWORD dwAttr)
{
	lpszFmt[0] = 0;

	if(FILE_ATTR_DIRECTORY == (FILE_ATTR_DIRECTORY & dwAttr))
	{
		_tcscat(lpszFmt,_T("D|"));
	}

	if(FILE_ATTR_READ_ONLY == (FILE_ATTR_READ_ONLY & dwAttr))
	{
		_tcscat(lpszFmt,_T("R|"));
	}

	if(FILE_ATTR_HIDDEN == (FILE_ATTR_HIDDEN & dwAttr))
	{
		_tcscat(lpszFmt,_T("H|"));
	}

	if(FILE_ATTR_SYSTEM == (FILE_ATTR_SYSTEM & dwAttr))
	{
		_tcscat(lpszFmt,_T("S|"));
	}

	if(FILE_ATTR_ARCHIVE == (FILE_ATTR_ARCHIVE & dwAttr))
	{
		_tcscat(lpszFmt,_T("A|"));
	}

	if(FILE_ATTR_COMPRESSED == (FILE_ATTR_COMPRESSED & dwAttr))
	{
		_tcscat(lpszFmt,_T("C|"));
	}

	if(FILE_ATTR_ENCRYPTED == (FILE_ATTR_ENCRYPTED & dwAttr))
	{
		_tcscat(lpszFmt,_T("E|"));
	}

	if(FILE_ATTR_DEVICE == (FILE_ATTR_DEVICE & dwAttr))
	{
		_tcscat(lpszFmt,_T("DEV|"));
	}

	if(FILE_ATTR_NORMAL == (FILE_ATTR_NORMAL & dwAttr))
	{
		_tcscat(lpszFmt,_T("N|"));
	}

	if(FILE_ATTR_TEMPORARY == (FILE_ATTR_TEMPORARY & dwAttr))
	{
		_tcscat(lpszFmt,_T("TMP|"));
	}

	if(FILE_ATTR_SPARSE_FILE == (FILE_ATTR_SPARSE_FILE & dwAttr))
	{
		_tcscat(lpszFmt,_T("SPAR|"));
	}

	if(FILE_ATTR_REPARSE_POINT == (FILE_ATTR_REPARSE_POINT & dwAttr))
	{
		_tcscat(lpszFmt,_T("REP|"));
	}

	if(FILE_ATTR_OFFLINE == (FILE_ATTR_OFFLINE & dwAttr))
	{
		_tcscat(lpszFmt,_T("OFF|"));
	}

	if(FILE_ATTR_NOT_CONTENT_INDEXED == (FILE_ATTR_NOT_CONTENT_INDEXED & dwAttr))
	{
		_tcscat(lpszFmt,_T("NCI|"));
	}

	if(FILE_ATTR_INDEX_VIEW == (FILE_ATTR_INDEX_VIEW & dwAttr))
	{
		_tcscat(lpszFmt,_T("IDX|"));
	}

	// Remove last pipe 

	size_t len = _tcslen(lpszFmt);

	if(0 < len)
	{
		lpszFmt[len - 1] = 0;
	}
}

/////////////////////////////////////////////////////////////////////

void GUINTFSFSHandler::FormatTime(LPTSTR szTime,FILETIME* pft)
{
	szTime[0] = 0;
	if(pft->dwHighDateTime == 0 && pft->dwLowDateTime ==0)
	{
		return;
	}
	// first convert file time (UTC time) to local time
	// then convert that time to system time
	// then convert the system time to a tm struct

	SYSTEMTIME sysTime;
	if (!FileTimeToSystemTime(pft, &sysTime))
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

bool GUINTFSFSHandler::GetCurrentPath(LPTSTR lpszPath, DWORD cchMaxPath)
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

bool GUINTFSFSHandler::PerformDefaultItemAction()
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
			break; // Single Sel only at the moment
		}
	}

	if(-1 == nSel)
	{
		return TRUE;
	}

	NTFSItem* pItem = (NTFSItem*)lParam;
	ASSERT(pItem);

	if(NULL == pItem)
	{
		return FALSE;
	}

	return PerformDefaultActionOnItem(pItem);
}

/////////////////////////////////////////////////////////////////////

bool GUINTFSFSHandler::PerformDefaultActionOnItem(NTFSItem *pItem)
{
	ASSERT(pItem);

	if (IS_DIRECTORY(pItem->dwAttr))
	{
		// Make Temp Copies as DisplayDirectory will delete
		// pItem;

		LPTSTR lpszTmpName;
				
#ifdef _UNICODE
		lpszTmpName = pItem->szName;
#else
		TCHAR szTmp2[FILE_NAME_LEN];
		WideCharToMultiByte(CP_ACP,0,pItem->szName,-1,szTmp2,FILE_NAME_LEN,_T("?"),NULL);
		lpszTmpName = szTmp2;
#endif
		
		TCHAR szTmp[FILE_NAME_LEN];
		_tcsncpy(szTmp,lpszTmpName,FILE_NAME_LEN-1);
		szTmp[FILE_NAME_LEN-1] = 0;

		LONGLONG llInTmp = pItem->mftIndex;

		if(!DisplayDirectory(pItem->mftIndex))
		{
			return FALSE;
		}

		return AddMFTEntryToPath(llInTmp, szTmp);
	}
	else if(IS_REGULAR_FILE(pItem->dwAttr))
	{	
        CopyFileLocal(pItem);
	}
	else
	{
		MessageBox(	NULL,
					_T("Cannot open this type of file..."),
					_T("Error: Not a Regular File."),
					MB_ICONINFORMATION);

		return FALSE;
	}

	return TRUE;
}

/////////////////////////////////////////////////////////////////////

bool GUINTFSFSHandler::CopyFileLocal(NTFSItem* pItem)
{
	ASSERT(pItem);

	if(pItem->size > 20 * 1024 * 1024)
	{
		// TODO: Get working for big files
		if(IDNO == MessageBox(NULL,
			_T(	"This is a very large file (20MB+).\n")
			_T("Are you sure you want to copy it?"),
			_T("Large File"),MB_YESNO | MB_ICONQUESTION))
		{
			return true;
		}
	}

	// Where should we copy to?

	TCHAR szPath[1024];
#ifdef _UNICODE
	wcsncpy(szPath,pItem->szName,1023);
#else
    WideCharToMultiByte(CP_ACP,0,pItem->szName,-1,szPath,1024,NULL,NULL);
#endif
	
	OPENFILENAME ofn = {0};

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = GetParent(m_hWndListCtrl);
	ofn.lpstrFile = szPath;
	ofn.nMaxFile = 1024;
	ofn.Flags = OFN_ENABLESIZING | OFN_OVERWRITEPROMPT;

	if(!GetSaveFileName(&ofn))
	{
		return true;
	}
	
	TCHAR szMsg[256];

	if(!m_prdr->ReadItemDataToFile(pItem->mftIndex,szPath,IS_COMPRESSED_FILE(pItem->dwAttr)))
	{
		_sntprintf(szMsg,256,_T("Could not copy file to %s"),szPath);
		MessageBox(NULL,szMsg,_T("Error Copying"),MB_ICONEXCLAMATION | MB_OK);

		return false;
	}

	_sntprintf(szMsg,256,_T("File successfully copied to %s"),szPath);
	MessageBox(NULL,szMsg,_T("Copy Successful"),MB_ICONINFORMATION | MB_OK);

	return true;
}

/////////////////////////////////////////////////////////////////////

bool GUINTFSFSHandler::ShowContextMenu()
{
	return FALSE;
}

/////////////////////////////////////////////////////////////////////

bool GUINTFSFSHandler::CDUp()
{
	if(1 == m_stkPath.size()) // Already at root
	{
		return TRUE;
	}

	ASSERT(!m_stkPath.empty());

	m_stkPath.pop(); // Pop Current Dir

	PATH_COMPONENT* pComp = (PATH_COMPONENT*)m_stkPath.top();
	ASSERT(pComp);
	
    return DisplayDirectory(pComp->llDir);
}

/////////////////////////////////////////////////////////////////////

void GUINTFSFSHandler::Close()
{
	if(m_prdr)
	{
		delete m_prdr;
	}
	m_prdr = NULL;

	if(m_pItems)
	{
		delete []m_pItems;
	}
	m_pItems = NULL;
	m_cItems = 0;

	EmptyPath();

    m_fInit = FALSE;
}

/////////////////////////////////////////////////////////////////////

void GUINTFSFSHandler::CloseAndDestroy()
{
	Close();
	delete this;
}

/////////////////////////////////////////////////////////////////////

bool GUINTFSFSHandler::AddMFTEntryToPath(LONGLONG llDir, 
										 LPTSTR lpszName)
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

void GUINTFSFSHandler::EmptyPath()
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

void GUINTFSFSHandler::ColumnSort(int column)
{
	m_sortOrder = -m_sortOrder;

    ListView_SortItems(	m_hWndListCtrl,compare_pitems,
						MAKELPARAM(column,m_sortOrder));
}

/////////////////////////////////////////////////////////////////////

int CALLBACK compare_pitems(	LPARAM param1, 
								LPARAM param2, 
								LPARAM param )
{
	_I16 sortOrder = (_I16)HIWORD(param);
	int column = LOWORD(param);

	NTFSItem *p1 = (NTFSItem*)param1; //plvi1->lParam;
	NTFSItem *p2 = (NTFSItem*)param2; //plvi2->lParam;

	if(IS_DIRECTORY(p1->dwAttr) && !IS_DIRECTORY(p2->dwAttr))
	{	
		return -1;
	}

	if(IS_DIRECTORY(p2->dwAttr) && !IS_DIRECTORY(p1->dwAttr))
	{	
		return 1;
	}

	/*
	{_T("Name"),100},
	{_T("Created"),110},
	{_T("Modified"),110},
	{_T("Access"),110},
	{_T("Size"),75},
	{_T("Attributes"),50},
	*/

	switch(column)
	{
	case 0: // Name
		return compare_items_text(p1->szName,p2->szName) * sortOrder;
		break;
	case 1: // Created
		return compare_items_filetime(&p1->ftCreate,&p2->ftCreate) * sortOrder;
	case 2: // Modified
		return compare_items_filetime(&p1->ftMod,&p2->ftMod) * sortOrder;
	case 3: // Accessed
		return compare_items_filetime(&p1->ftAccess,&p2->ftAccess) * sortOrder;
		break;
	case 4: //Size
		if(IS_DIRECTORY(p1->dwAttr) && IS_DIRECTORY(p2->dwAttr))
		{
			return 0;
		}
		else
		{
			return compare_items_u64(p1->size,p2->size) * sortOrder;
		}
		break;
	case 5: // Attributes
		//return compare_items_text(p1->szName,p2->szName);
		break;
	}

	return 0;
}

/////////////////////////////////////////////////////////////////////

int compare_items_text(LPWSTR pa, LPWSTR pb)
{
	// skip dot and compare on next letter

	if(wcslen(pa) > 1 && pa[0] == '.' && pa[1] != '.')
	{
		pa++;
	}

	if(wcslen(pb) > 1 && pb[0] == '.' && pb[1] != '.')
	{
		pb++;
	}

	return wcsicmp(pa,pb);
}

/////////////////////////////////////////////////////////////////////

int compare_items_u64(_U64 a, _U64 b)
{
	if(a == b)
	{
		return 0;
	}

	return (a > b) ? 1 : -1;
}

/////////////////////////////////////////////////////////////////////

int compare_items_filetime(PFILETIME pt1, PFILETIME pt2)
{
	return CompareFileTime(pt1,pt2);
}

/////////////////////////////////////////////////////////////////////