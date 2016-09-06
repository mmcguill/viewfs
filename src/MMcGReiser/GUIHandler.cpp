/////////////////////////////////////////////////////////////////////
// 24/JUN/2006
//
// Project: MMcGReiser
// File: GUIHandler.cpp
// Author: Mark McGuill
//
// Copyright (C) 2005  Mark McGuill
//
/////////////////////////////////////////////////////////////////////

#include "dllmain.h"
#include <commctrl.h>
#include <commdlg.h>
#include "..\..\inc\viewfs_types.h"
#include "..\..\inc\linux.h"
#include "GUIHandler.h"

/////////////////////////////////////////////////////////////////////

//int quick_compare_pitems( const void *arg1, const void *arg2 );
int CALLBACK compare_pitems( LPARAM p1, LPARAM p2, LPARAM param );
int compare_items_u64(_U64 a, _U64 b);
int compare_items_text(LPTSTR pa, LPTSTR pb);

/////////////////////////////////////////////////////////////////////

struct COL
{
	LPCTSTR lpszCol;
	int nWidth;
};

/////////////////////////////////////////////////////////////////////

COL cols[] = 
{
	{_T("Name"),100},
	{_T("Size"),75},
	{_T("Permissions"),75},
	{_T("Attributes"),50},
	{_T("Created"),110},
	{_T("Modified"),110},
	{_T("Access"),110},
};

/////////////////////////////////////////////////////////////////////

#define NO_OF_COLUMNS (sizeof(cols) / sizeof(COL))

/////////////////////////////////////////////////////////////////////

bool GUIReiserHandler::Init(HANDLE hFile, LONGLONG llBeginFileOffset,
							HWND hWndListCtrl)
{
	if(NULL == hFile || NULL == hWndListCtrl)
	{
		ASSERT(false);
		return false;
	}

	ASSERT(!m_fInit);
	if(m_fInit)
	{
		return false;
	}

	m_hWndListCtrl = hWndListCtrl;

    m_prdr = new ReiserReader();
	if(NULL == m_prdr)
	{
		//m_dwLastError = ERR_OUT_OF_MEMORY;
		return false;
	}

	if(!m_prdr->Init(hFile,llBeginFileOffset))
	{
		delete m_prdr;
		m_prdr = NULL;
		return false;
	}

	
	// Setup ListCtrl

	ListView_DeleteAllItems(m_hWndListCtrl);
	DWORD old = ListView_GetExtendedListViewStyle(m_hWndListCtrl);
	ListView_SetExtendedListViewStyle(m_hWndListCtrl,
				old | LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);

	
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

	m_fInit = true;
	m_sortOrder = -1;
	return true;
}

/////////////////////////////////////////////////////////////////////

bool GUIReiserHandler::DisplayRootDirectory()
{
	if(!DisplayDirectory(1,2))
	{
		return false;
	}

	EmptyPath();
	
	return AddDirToPath(1,2,_T(""));
}

/////////////////////////////////////////////////////////////////////

bool GUIReiserHandler::DisplayDirectory(DWORD dwDirectoryID, DWORD dwObjectID)
{
	ASSERT(m_fInit);
	
	_U32 cItems = 0;
	if(!m_prdr->ReadDirectory(dwDirectoryID,dwObjectID,NULL,&cItems) &&
		ERR_NOT_ENOUGH_BUFFER != m_prdr->GetLastError())
	{
		return false;
	}

	
	// Clear Old

	ListView_DeleteAllItems(m_hWndListCtrl);

	if(m_pItems)
	{
		delete m_pItems;
		m_pItems = NULL;
		m_cItems = 0;
	}
	
	
	// AllocNew

	m_cItems = cItems;
	m_pItems = new ReiserItem[m_cItems];
	if(NULL == m_pItems)
	{
		return false;
	}

	if(!m_prdr->ReadDirectory(dwDirectoryID,dwObjectID,m_pItems,&m_cItems))
	{
		return false;
	}

	// Sort
	
	/*qsort(m_pItems,m_cItems,sizeof(ReiserItem),quick_compare_pitems);*/

	// Display

	if(!DisplayItems())
	{
		return false;
	}

	//ColumnSort(0);
	m_sortOrder = 1;
    ListView_SortItems(	m_hWndListCtrl,compare_pitems,
						MAKELPARAM(0,m_sortOrder));

	return true;
}

/////////////////////////////////////////////////////////////////////

bool GUIReiserHandler::DisplayItems()
{
	ASSERT(m_fInit);

	ListView_DeleteAllItems(m_hWndListCtrl);

	for(unsigned long i=0;i<m_cItems;i++)
	{
		if(	(m_pItems[i].szName[0] == _T('.') && m_pItems[i].szName[1] == 0) || 
			(m_pItems[i].szName[0] == _T('.') && m_pItems[i].szName[1] == _T('.') && m_pItems[i].szName[2] == 0))
		{
			continue;
		}

		if(!InsertEntry(i,&m_pItems[i]))
		{
			return false;
		}
	}
	return true;
}

/////////////////////////////////////////////////////////////////////

void GUIReiserHandler::ColumnSort(int column)
{
	m_sortOrder = -m_sortOrder;

	ListView_SortItems(	m_hWndListCtrl,compare_pitems,
						MAKELPARAM(column,m_sortOrder));
}

/////////////////////////////////////////////////////////////////////

bool GUIReiserHandler::InsertEntry(int index, ReiserItem* pItem)
{
	/*
	{_T("Name"),100},
	{_T("Size"),75},
	{_T("Permissions"),75},
	{_T("Attributes"),50},
	{_T("Created"),110},
	{_T("Modified"),110},
	{_T("Access"),110},
	*/

	SHFILEINFO FileInfo;
	if (IS_DIRECTORY(pItem->dwMode))
	{
		SHGetFileInfo(pItem->szName,FILE_ATTRIBUTE_DIRECTORY,
			&FileInfo,sizeof(SHFILEINFO), SHGFI_SMALLICON | 
			SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES );	
	}
	else
	{
		// TODO: Unicode this name
		SHGetFileInfo(pItem->szName,NULL,&FileInfo,sizeof(SHFILEINFO), 
		SHGFI_SMALLICON | SHGFI_SYSICONINDEX| SHGFI_USEFILEATTRIBUTES );	
	}

	TCHAR szTmp[64];

	// Name

	LV_ITEM lvi = {0};
	lvi.iItem = index;
	lvi.iSubItem = 0;
	lvi.iImage = FileInfo.iIcon;
	lvi.lParam = (DWORD_PTR)pItem;
	lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
	lvi.pszText = pItem->szName;

	int nItem = ListView_InsertItem(m_hWndListCtrl,&lvi);		

	// Size

	ULONGLONG ul = pItem->size;
	FormatSizeEx(szTmp,&ul);
	lvi.iItem = nItem;
	lvi.iSubItem = 1;
	if (IS_DIRECTORY(pItem->dwMode))
	{
		lvi.pszText = _T("");
	}
	else
	{
		lvi.pszText = szTmp;
	}
	lvi.mask = LVIF_TEXT;
	ListView_SetItem(m_hWndListCtrl,&lvi);		

	// Permissions

	lvi.iItem = nItem;
	lvi.iSubItem = 2;
	lvi.pszText = szTmp;
	lvi.mask = LVIF_TEXT;
	LinuxFormatPermissions(szTmp,pItem->dwMode);
	ListView_SetItem(m_hWndListCtrl,&lvi);		

	// Attributes

	lvi.iItem = nItem;
	lvi.iSubItem = 3;
	lvi.pszText = szTmp;
	lvi.mask = LVIF_TEXT;
	LinuxFormatAttributes(szTmp,pItem->dwMode);
	ListView_SetItem(m_hWndListCtrl,&lvi);		

	// Created

	lvi.iItem = nItem;
	lvi.iSubItem = 4;
	lvi.pszText = szTmp;
	lvi.mask = LVIF_TEXT;
	LinuxFormatTime(szTmp,pItem->dwCreate);
	ListView_SetItem(m_hWndListCtrl,&lvi);		

	// Modified

	lvi.iItem = nItem;
	lvi.iSubItem = 5;
	lvi.pszText = szTmp;
	lvi.mask = LVIF_TEXT;
	LinuxFormatTime(szTmp,pItem->dwModify);
	ListView_SetItem(m_hWndListCtrl,&lvi);		

	// Access

	lvi.iItem = nItem;
	lvi.iSubItem = 6;
	lvi.pszText = szTmp;
	lvi.mask = LVIF_TEXT;
	LinuxFormatTime(szTmp,pItem->dwAccess);
	ListView_SetItem(m_hWndListCtrl,&lvi);		

	return true;
}

/////////////////////////////////////////////////////////////////////

bool GUIReiserHandler::GetCurrentPath(LPTSTR lpszPath, DWORD cchMaxPath)
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

	return true;
}

/////////////////////////////////////////////////////////////////////

bool GUIReiserHandler::PerformDefaultItemAction()
{
	if(!m_fInit)
	{
		return false;
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
		return true;
	}

	ReiserItem* pItem = (ReiserItem*)lParam;
	ASSERT(pItem);

	if(NULL == pItem)
	{
		return false;
	}
	
	return PerformDefaultActionOnItem(pItem);
}

/////////////////////////////////////////////////////////////////////

bool GUIReiserHandler::PerformDefaultActionOnItem(ReiserItem* pItem)
{
	ASSERT(pItem);

	if (IS_DIRECTORY(pItem->dwMode))
	{
		// Make Copies as DisplayDirectory will delete pItem;

		TCHAR szName[REISER_CCH_MAX_NAME];
		_tcscpy(szName,pItem->szName);
		DWORD dwDirectoryID = pItem->dwDirectoryID;
		DWORD dwObjectID = pItem->dwObjectID;

		if(!DisplayDirectory(pItem->dwDirectoryID,pItem->dwObjectID))
		{
			return false;
		}

		return AddDirToPath(dwDirectoryID,dwObjectID, szName);
	}
	else if(IS_REGULAR_FILE(pItem->dwMode))
	{	
        return CopyFileLocal(pItem);
	}
	else if(IS_LINK(pItem->dwMode))
	{
		TCHAR szLinkedFile[512];
		TCHAR szFullPath[512];
		_U32 cchLF = 512;

		if(!m_prdr->ReadSymbolicLink(	pItem->dwDirectoryID,
										pItem->dwObjectID,
										szLinkedFile,&cchLF))
		{
			MessageBox(NULL,_T("Error Reading Symbolic Link."),
							_T("Error: Symbolic Link"),
							MB_ICONEXCLAMATION);

			return false;
		}

		// Calculate Full Path

		if(szLinkedFile[0] != _T('/'))
		{
			// Just get Current path and append

			if(!GetCurrentPath(szFullPath,512))
			{
				return false;
			}

			_tcsncat(szFullPath,szLinkedFile,512);
		}
		else
		{
			_tcsncpy(szFullPath,szLinkedFile,512);
		}


		// What Kind of Object is this? Find out so we can ask the
		// right question...

		ReiserItem item;

		if(!m_prdr->GetItemFromPath(szFullPath,&item))
		{
			TCHAR szMsg[512];
			_sntprintf(szMsg,512,
				_T("The item that this symbolic link refers to")
				_T(" (%s) does not exist on this file system."),
				szFullPath);

			MessageBox(NULL,szMsg,_T("Broken Link"),MB_ICONINFORMATION);
			return false;
		}

		// Ask the right questions...

		TCHAR szMsg[512];
		bool ret = false;

		if(IS_DIRECTORY(item.dwMode))
		{
			_sntprintf(szMsg,512,
				_T(	"The file %s is a symbolic link to the directory:\n\n"
				_T("%s\n\nWould you like to go to this directory?")),
				pItem->szName,szFullPath);

			if(IDNO == MessageBox(NULL,szMsg,_T("Symbolic Link"),
									MB_YESNO | MB_ICONQUESTION))
			{
				return true;
			}

			if(!DisplayDirectory(item.dwDirectoryID,item.dwObjectID))
			{
				return false;
			}


			// Need to rebuild path as it could be anything now!
						
			EmptyPath();

			// Add Root

			if(!AddDirToPath(1,2, _T("")))
			{
				return false;
			}
	        
			// Make sure its not the root dir if so we can now bail...

			if(szFullPath[0] == _T('/') && szFullPath[1] == 0)
			{
				return true;
			}

			// Parse Full Path & Add

			LPTSTR pTok = szFullPath;
			LPTSTR pSubName = pTok + 1;

			pTok = _tcschr(pTok+1,_T('/'));
			
			bool fContinue = true;
			while(fContinue)
			{
				if(pTok)
				{
					*pTok = 0; // Null out this slash temporarily
				}

				ReiserItem TmpItem;

				if(	!m_prdr->GetItemFromPath(szFullPath,&TmpItem) ||
					!AddDirToPath(TmpItem.dwDirectoryID,TmpItem.dwObjectID,pSubName))
				{
					return false;
				}
				
                // Restore Slash
				if(pTok)
				{
					*pTok = _T('/');
				}

				// Next Subname...
				pSubName = pTok + 1;
				

				// Update

				fContinue = (pTok != NULL);

				if(fContinue)
				{
					pTok = _tcschr(pTok+1,_T('/'));
				}
			}	

			return true;
		}
		else if(IS_REGULAR_FILE(item.dwMode) || IS_LINK(item.dwMode))
		{
			_sntprintf(szMsg,512,
			_T(	"The file %s is a symbolic link to the file:\n\n"
				_T("%s\n\nWould you like to copy this file?")),
				pItem->szName,szFullPath);

			if(IDNO == MessageBox(NULL,szMsg,_T("Symbolic Link"),
									MB_YESNO | MB_ICONQUESTION))
			{
				return true;
			}

			ret = PerformDefaultActionOnItem(&item); 
			return ret;
		}
	}
	else
	{
		MessageBox(NULL,_T("Cannot Open This Item Type"),
			_T("Error Opening"),MB_ICONEXCLAMATION);
	}

	return true;
}

/////////////////////////////////////////////////////////////////////

bool GUIReiserHandler::ShowContextMenu()
{
	return false;
}

/////////////////////////////////////////////////////////////////////

bool GUIReiserHandler::CDUp()
{
	if(m_stkPath.size() < 2) // Already at root
	{
		return true;
	}

	m_stkPath.pop(); // Pop It Off

	PATH_COMPONENT* pComp = (PATH_COMPONENT*)m_stkPath.top();
	ASSERT(pComp);

	return DisplayDirectory(pComp->dwDirectoryID,pComp->dwObjectID);
}

/////////////////////////////////////////////////////////////////////

void GUIReiserHandler::Close()
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

	EmptyPath();
	m_cItems = 0;
	m_fInit = false;
}

/////////////////////////////////////////////////////////////////////

bool GUIReiserHandler::AddDirToPath(DWORD dwDirectoryID, DWORD dwObjectID, 
									LPTSTR lpszName)
{
	PATH_COMPONENT* pComp = NULL;
	
	stack<PATH_COMPONENT*> stkTmp;

	bool fFound = false;
	while(!m_stkPath.empty())
	{
		pComp = m_stkPath.top();
		ASSERT(pComp);
		stkTmp.push(pComp); // Need to restore later

		if(	dwDirectoryID == pComp->dwDirectoryID && 
			dwObjectID == pComp->dwObjectID)
		{
			fFound = true;
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
			return false;
		}

		pComp->dwDirectoryID = dwDirectoryID;
		pComp->dwObjectID = dwObjectID;

		_tcscpy(pComp->szName,lpszName);

		m_stkPath.push(pComp);
	}

	return true;
}

/////////////////////////////////////////////////////////////////////

void GUIReiserHandler::EmptyPath()
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

void GUIReiserHandler::CloseAndDestroy()
{
	Close();
	delete this;
}

/////////////////////////////////////////////////////////////////////

bool GUIReiserHandler::CopyFileLocal(ReiserItem* pItem)
{
	ASSERT(pItem);

	if(pItem->size > 20 * 1024 * 1024)
	{
		// TODO: Get working for big files
		if(IDNO == MessageBox(NULL,
			_T(	"This is a very large file (20MB+).\n"
				_T("Are you sure you want to copy it?")),
				_T("Large File"),MB_YESNO | MB_ICONQUESTION))
		{
			return true;
		}
	}

	// Where should we copy to?

	TCHAR szPath[1024];
	_tcsncpy(szPath,pItem->szName,1023);

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

	if(!m_prdr->ReadItemToFile(pItem->dwDirectoryID,pItem->dwObjectID,szPath))
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

int CALLBACK compare_pitems( LPARAM param1, LPARAM param2, LPARAM param )
{
	_I16 sortOrder = (_I16)HIWORD(param);
	int column = LOWORD(param);

	ReiserItem *p1 = (ReiserItem *)param1; //plvi1->lParam;
	ReiserItem *p2 = (ReiserItem *)param2; //plvi2->lParam;

	if(IS_DIRECTORY(p1->dwMode) && !IS_DIRECTORY(p2->dwMode))
	{	
		return -1;
	}

	if(IS_DIRECTORY(p2->dwMode) && !IS_DIRECTORY(p1->dwMode))
	{	
		return 1;
	}

	/*
	{_T("Name"),100},
	{_T("Size"),75},
	{_T("Permissions"),75},
	{_T("Attributes"),50},
	{_T("Created"),110},
	{_T("Modified"),110},
	{_T("Access"),110},
	*/

	switch(column)
	{
	case 0: // Name
		return compare_items_text(p1->szName,p2->szName) * sortOrder;
		break;
	case 2: // Permissions
		//return compare_items_text(p1->szName,p2->szName);
		break;
	case 3: // Attributes
		//return compare_items_text(p1->szName,p2->szName);
		break;
	case 1: //Size
		if(IS_DIRECTORY(p1->dwMode) && IS_DIRECTORY(p2->dwMode))
		{
			return 0;
		}
		else
		{
			return compare_items_u64(p1->size,p2->size) * sortOrder;
		}
		break;
	case 4: // Created
		return compare_items_u64(p1->dwCreate,p2->dwCreate) * sortOrder;
	case 5: // Modified
		return compare_items_u64(p1->dwModify,p2->dwModify) * sortOrder;
	case 6: // Accessed
		return compare_items_u64(p1->dwAccess,p2->dwAccess) * sortOrder;
		break;
	}

	return 0;
}

/////////////////////////////////////////////////////////////////////

//int quick_compare_pitems( const void *arg1, const void *arg2 )
//{
//	ReiserItem *p1 = (ReiserItem *)arg1;
//	ReiserItem *p2 = (ReiserItem *)arg2;
//
//	if(IS_DIRECTORY(p1->dwMode) && !IS_DIRECTORY(p2->dwMode))
//	{	
//		return -1;
//	}
//	if(IS_DIRECTORY(p2->dwMode) && !IS_DIRECTORY(p1->dwMode))
//	{	
//		return 1;
//	}
//
//
//	char *pa, *pb;
//	pa = p1->szName;
//	pb = p2->szName;
//
//	// skip dot and compare on next letter
//
//	if(strlen(pa) > 1 && pa[0] == _T('.') && pa[1] != _T('.'))
//	{
//		pa++;
//	}
//
//	if(strlen(pb) > 1 && pb[0] == _T('.') && pb[1] != _T('.'))
//	{
//		pb++;
//	}
//
//	int nRet = stricmp(pa,pb);
//
//    return nRet;
//}

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

int compare_items_text(LPTSTR pa, LPTSTR pb)
{
	// skip dot and compare on next letter

	if(_tcslen(pa) > 1 && pa[0] == _T('.') && pa[1] != _T('.'))
	{
		pa++;
	}

	if(_tcslen(pb) > 1 && pb[0] == _T('.') && pb[1] != _T('.'))
	{
		pb++;
	}

	int nRet = _tcsicmp(pa,pb);

    return nRet;
}

/////////////////////////////////////////////////////////////////////
