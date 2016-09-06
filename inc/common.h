/////////////////////////////////////////////////////////////////////
// 25/MAR/2005
//
//
//
//
//
/////////////////////////////////////////////////////////////////////

// This class must be dynamically allocated with new
// It will delete itself on passing out of scope...
// It will Call Close() before so doing...

class GUIFileSystemBaseHandler
{
public:
	GUIFileSystemBaseHandler()
	{
		m_hWndListCtrl = NULL; // Handle to List Control
		m_fInit = FALSE;
	}

public: 
    virtual bool Init(HANDLE hFile, LONGLONG llBeginFileOffset, HWND hWndListCtrl) = 0;
	virtual bool DisplayRootDirectory() = 0;
	virtual bool GetCurrentPath(LPTSTR lpszPath, DWORD cchMaxPath) = 0;
	virtual bool PerformDefaultItemAction() = 0;
	virtual bool ShowContextMenu() = 0;
	virtual bool CDUp() = 0;
	virtual void Close() = 0;
	virtual void CloseAndDestroy() = 0;

	virtual void ColumnSort(int column)
	{
		return;
	}

	void FormatSizeEx(LPTSTR pszSize,ULONGLONG *psize)
	{
		ASSERT(psize);
		ULARGE_INTEGER liTmp;
		liTmp.QuadPart = *psize;
		FormatSizeEx(pszSize,&liTmp);
	}

	void FormatSizeEx(LPTSTR pszSize,ULARGE_INTEGER *psize)
	{
		ASSERT(psize);

		DWORD dwGig = (1024*1024*1024);
		ULARGE_INTEGER Tera;
		Tera.QuadPart = dwGig;
		Tera.QuadPart *= 1024;

		if(psize->QuadPart >= Tera.QuadPart)
		{
			_sntprintf(pszSize,32,_T("%.2f Tb"),(float)psize->QuadPart / Tera.QuadPart);
		}
		else if(psize->QuadPart >= dwGig)
		{
			_sntprintf(pszSize,32,_T("%.2f Gb"),(float)psize->QuadPart / (1024*1024*1024));
		}
		else if(psize->QuadPart >= (1024*1024))
		{
			_sntprintf(pszSize,32,_T("%.2f Mb"),(float)psize->QuadPart / (1024*1024));
		}
		else if(psize->QuadPart >= (1024))
		{
			_sntprintf(pszSize,32,_T("%.1f Kb"),(float)psize->QuadPart / 1024);
		}
		else
		{
			_sntprintf(pszSize,32,_T("%d Bytes"),psize->QuadPart);
		}
	}

/////////////////////////////////////////////////////////////////////

protected:
	HWND		m_hWndListCtrl; // Handle to List Control
	bool		m_fInit;
};

/////////////////////////////////////////////////////////////////////

// Handler DLL Must Export This Function

const LPSTR kszHandlerFuncName = "GetGUIFSHandler";

typedef bool (*PFN_GET_GUI_FS_HANDLER)(DWORD dwFSType,
									 GUIFileSystemBaseHandler** ppHandler);

/////////////////////////////////////////////////////////////////////