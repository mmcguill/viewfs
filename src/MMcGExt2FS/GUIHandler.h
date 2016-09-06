/////////////////////////////////////////////////////////////////////
// 07/APR/2005
//
// Project: MMcGExt2FS
// File: GUIHandler.h
// Author: Mark McGuill
//
// Copyright (C) 2005  Mark McGuill
//
/////////////////////////////////////////////////////////////////////

#pragma once

/////////////////////////////////////////////////////////////////////

#include "MMcGExt2FS.h"
using namespace MMcGExt2FS;

/////////////////////////////////////////////////////////////////////

typedef struct _PATH_COMPONENT
{
	LONGLONG llInode;
	TCHAR szName[EXT2_NAME_LEN];
}PATH_COMPONENT;

/////////////////////////////////////////////////////////////////////

class GUIExt2FSHandler : public GUIFileSystemBaseHandler
{
public:
	GUIExt2FSHandler()
	{
		m_prdr = NULL;
		m_pItems = NULL;
		m_cItems = 0;
	}

	virtual ~GUIExt2FSHandler()
	{
		Close();
	}

public: 
	virtual bool Init(HANDLE hFile, LONGLONG llBeginFileOffset, HWND hWndListCtrl);
	virtual bool DisplayRootDirectory();
	virtual bool GetCurrentPath(LPTSTR lpszPath, DWORD cchMaxPath);
	virtual bool PerformDefaultItemAction(); // On Double Click or Enter
	virtual bool ShowContextMenu();
	virtual bool CDUp();
	virtual void Close();
	virtual void CloseAndDestroy(); // Necessary because heaps can differ in client
	virtual void ColumnSort(int column);

protected:
	bool PerformDefaultActionOnItem(Ext2FSItem *pItem);
	bool DisplayDirectory(LONGLONG llInode);
	bool CopyFileLocal(Ext2FSItem* pItem);
	bool DisplayItems();
	bool InsertEntry(int index, Ext2FSItem* pItem);
	
	bool AddInodeToPath(LONGLONG llInode, LPTSTR lpszName);
	void EmptyPath();

protected:
	Ext2FSReader			*m_prdr;
	Ext2FSItem				*m_pItems;
	_U32					m_cItems;
	stack<PATH_COMPONENT*>	m_stkPath;
	int						m_sortOrder; 
private:
private:
};

/////////////////////////////////////////////////////////////////////