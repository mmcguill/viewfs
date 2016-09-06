/////////////////////////////////////////////////////////////////////
// 24/JUN/2006
//
// Project: MMcGReiser
// File: GUIHandler.h
// Author: Mark McGuill
//
// Copyright (C) 2006  Mark McGuill
//
/////////////////////////////////////////////////////////////////////

#pragma once

/////////////////////////////////////////////////////////////////////

#include "MMcGReiser.h"
using namespace MMcGReiser;

/////////////////////////////////////////////////////////////////////

typedef struct _PATH_COMPONENT
{
	DWORD dwDirectoryID;
	DWORD dwObjectID;
	TCHAR szName[REISER_CCH_MAX_NAME];
}PATH_COMPONENT;

/////////////////////////////////////////////////////////////////////

class GUIReiserHandler : public GUIFileSystemBaseHandler
{
public:
	GUIReiserHandler()
	{
		m_prdr = NULL;
		m_pItems = NULL;
		m_cItems = 0;
	}

	virtual ~GUIReiserHandler()
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
	bool PerformDefaultActionOnItem(ReiserItem* pItem);
	bool DisplayDirectory(DWORD dwDirectoryID, DWORD dwObjectID);
	bool CopyFileLocal(ReiserItem* pItem);
	bool DisplayItems();
	bool InsertEntry(int index, ReiserItem* pItem);
	
	bool AddDirToPath(DWORD dwDirectoryID, DWORD dwObjectID, LPTSTR lpszName);
	void EmptyPath();

protected:
	ReiserReader			*m_prdr;
	ReiserItem				*m_pItems;
	_U32					m_cItems;
	stack<PATH_COMPONENT*>	m_stkPath;
	int						m_sortOrder;

private:
private:
};

/////////////////////////////////////////////////////////////////////