/////////////////////////////////////////////////////////////////////
// 25/MAR/2005
//
//
//
//
//
/////////////////////////////////////////////////////////////////////

#pragma once

/////////////////////////////////////////////////////////////////////

#include "MMcGNTFS.h"
using namespace MMcGNTFS;

/////////////////////////////////////////////////////////////////////

typedef struct _PATH_COMPONENT
{
	LONGLONG llDir;
	TCHAR szName[FILE_NAME_LEN];
}PATH_COMPONENT;

/////////////////////////////////////////////////////////////////////

class GUINTFSFSHandler : public GUIFileSystemBaseHandler
{
public:
	GUINTFSFSHandler()
	{
		m_prdr = NULL;
		m_pItems = NULL;
		m_cItems = 0;
	}

	virtual ~GUINTFSFSHandler()
	{
		Close();
	}

public: 
	virtual bool Init(HANDLE hFile, LONGLONG llBeginFileOffset, HWND hWndListCtrl);
	virtual bool DisplayRootDirectory();
	virtual bool GetCurrentPath(LPTSTR lpszPath, DWORD cchMaxPath);
	virtual bool PerformDefaultItemAction(); // On Double Click or Enter
	virtual bool ShowContextMenu();
	virtual void ColumnSort(int column);
	virtual bool CDUp();
	virtual void Close();
	virtual void CloseAndDestroy(); // Necessary because heaps can differ in client

protected:
	bool PerformDefaultActionOnItem(NTFSItem *pItem);
	bool DisplayDirectory(LONGLONG llDir);
	bool CopyFileLocal(NTFSItem* pItem);

	bool DisplayItems();
	bool InsertEntry(int index, NTFSItem* pItem);
	void FormatTime(LPTSTR szTime,FILETIME* pft);
	void FormatAttributes(LPTSTR lpszFmt, DWORD dwAttr);
	bool AddMFTEntryToPath(LONGLONG llDir, LPTSTR lpszName);
	void EmptyPath();

protected:
	NTFSReader				*m_prdr;
	NTFSItem				*m_pItems;
	int						m_cItems;
	stack<PATH_COMPONENT*>	m_stkPath;
	int						m_sortOrder;

private:
private:
};

/////////////////////////////////////////////////////////////////////