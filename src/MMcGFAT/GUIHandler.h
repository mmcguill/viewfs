/////////////////////////////////////////////////////////////////////
// 25/MAR/2005
//
//
//
//
//
/////////////////////////////////////////////////////////////////////

#pragma once

#include "MMcGFAT.h"

/////////////////////////////////////////////////////////////////////

typedef struct _PATH_COMPONENT
{
	LONGLONG llDir;
	TCHAR szName[FAT_NAME_LEN];
}PATH_COMPONENT;

/////////////////////////////////////////////////////////////////////

class GUIFATFSHandler : public GUIFileSystemBaseHandler
{
public:
	GUIFATFSHandler()
	{
		m_pItems = NULL;
		m_cItems = 0;
	}

	virtual ~GUIFATFSHandler()
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
	virtual void CloseAndDestroy();

protected:
	bool ChangeDirectory(DWORD dwCluster);
	bool CreateTmpViewOfItem(FATItem* pItem);
	bool DisplayDirectory(BYTE *pDir, DWORD cbDir);
	bool DisplayItems();
	bool InsertEntry(FATItem *pItem);
	void FormatDosTime(LPTSTR szTime,WORD date, WORD time);
	void FormatSize(LPTSTR pszSize,DWORD size);
	bool AddDirectoryToPath(LONGLONG llDir, LPTSTR lpszName);
	void EmptyPath();

protected:
	FATInfo *m_pfi;
	FATItem *m_pItems;
	DWORD m_cItems;
	HANDLE		m_hFile;
	LONGLONG	m_llBeginFileOffset;
	stack<PATH_COMPONENT*>	m_stkPath;

private:
private:
};

/////////////////////////////////////////////////////////////////////