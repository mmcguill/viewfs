/////////////////////////////////////////////////////////////////////
// 10/APR/2005
//
// Project: Halcyon
// File: main.h
// Author: Mark McGuill
//
// Copyright (C) 2005  Mark McGuill
//
/////////////////////////////////////////////////////////////////////

#include "main.h"
#include "resource.h"

/////////////////////////////////////////////////////////////////////

#define MAIN_WND_CLASS_NAME		_T("MMCG_Halcyon_1.0")
#define MAIN_WND_CLASS_STYLE	(CS_DBLCLKS)
#define MAIN_WND_TITLE			_T("MMcG Codename Halcyon")
#define MAIN_WND_STYLE			WS_OVERLAPPEDWINDOW

/////////////////////////////////////////////////////////////////////

HINSTANCE ghInst = NULL;
HWND ghWndMain = NULL;

/////////////////////////////////////////////////////////////////////

typedef struct _HALCYON_WND_LAYOUT
{
	HWND hWndTree;
	RECT rcTree;
	HWND hWndList;
	RECT rcList;
}HALCYON_WND_LAYOUT;

#define ID_LISTVIEW 101
#define ID_TREEVIEW 102

#define SPLITTER_CS = 5 // 5 pixels wide
#define SPLIT_INIT_DEFAULT_PC 30 // 30% left / 70% right initially 

static HALCYON_WND_LAYOUT s_layout;

/////////////////////////////////////////////////////////////////////

LRESULT CALLBACK MainWndProc(HWND hwnd,UINT uMsg,WPARAM wParam,
							 LPARAM lParam);

/////////////////////////////////////////////////////////////////////

LRESULT OnCreate(WPARAM wParam, LPARAM lParam);
LRESULT OnClose(WPARAM wParam, LPARAM lParam);
BOOL	InitChildren();

/////////////////////////////////////////////////////////////////////

int AppMessageBox(LPTSTR lpszMsg, UINT nStyle /* = MB_OK */)
{
	return MessageBox(ghWndMain,lpszMsg,MAIN_WND_TITLE,nStyle);
}

/////////////////////////////////////////////////////////////////////

int WINAPI WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,
			LPSTR lpCmdLine,int nCmdShow)
{
	ghInst = hInstance;
	
	WNDCLASS wndClass = {0};
	wndClass.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wndClass.hCursor = LoadCursor(ghInst,MAKEINTRESOURCE(IDC_ARROW));
	wndClass.hIcon = LoadIcon(ghInst,MAKEINTRESOURCE(IDI_HALCYON));
	wndClass.lpszMenuName = MAKEINTRESOURCE(IDR_HALCYON);
	wndClass.hInstance = ghInst;
	wndClass.lpfnWndProc = MainWndProc;
	wndClass.lpszClassName = MAIN_WND_CLASS_NAME;
	wndClass.style =	MAIN_WND_CLASS_STYLE;

	if(!RegisterClass(&wndClass))
	{
		AppMessageBox(_T("Error Registering Main Window"),
			MB_ICONEXCLAMATION);
		return 0;
	}	

	if(NULL == (ghWndMain = CreateWindow(MAIN_WND_CLASS_NAME,
					MAIN_WND_TITLE, MAIN_WND_STYLE, 0, 0, 
					CW_USEDEFAULT, CW_USEDEFAULT,
					NULL, NULL, ghInst, NULL)))
	{
		AppMessageBox(_T("Error Creating Main Window"),
			MB_ICONEXCLAMATION);

		UnregisterClass(MAIN_WND_CLASS_NAME,ghInst);
		return 0;
	}

	// Create Children Now

	InitChildren();

	// Cloaks Down... Display

	ShowWindow(ghWndMain, SW_SHOWNORMAL);

	
	// Pump

	MSG msg;
	while(0 < GetMessage(&msg,NULL,0,0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	
	// Cleanup

	UnregisterClass(MAIN_WND_CLASS_NAME,ghInst);
	return 1;
}

/////////////////////////////////////////////////////////////////////

LRESULT CALLBACK MainWndProc(HWND hwnd,UINT uMsg,WPARAM wParam,
							 LPARAM lParam)
{

	switch(uMsg)
	{
	case WM_CREATE:
		return OnCreate(wParam, lParam);
	case WM_CLOSE:
		return OnClose(wParam, lParam);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	}

	return DefWindowProc(hwnd,uMsg,wParam,lParam);
}

/////////////////////////////////////////////////////////////////////

LRESULT OnClose(WPARAM wParam, LPARAM lParam)
{
	DestroyWindow(ghWndMain);
	return 0;
}

/////////////////////////////////////////////////////////////////////

LRESULT OnCreate(WPARAM wParam, LPARAM lParam)
{
	INITCOMMONCONTROLSEX ic = {0};
	ic.dwSize = sizeof(INITCOMMONCONTROLSEX);
	ic.dwICC = ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES 
		| ICC_BAR_CLASSES;

	if(!InitCommonControlsEx(&ic))
	{
		return -1;
	}

	//LPCREATESTRUCT lpcs = (LPCREATESTRUCT)lParam;
	return 0;
}

/////////////////////////////////////////////////////////////////////

BOOL InitChildren()
{
	RECT rc;
	GetClientRect(ghWndMain,&rc);

	HWND hWndStat = CreateStatusWindow(WS_CHILD | WS_VISIBLE,_T("Test"),ghWndMain,2);

	HWND hWndTree = 
		CreateWindowEx(WS_EX_CLIENTEDGE,WC_TREEVIEW,NULL,
			WS_CHILD | WS_VISIBLE,
			0,0,rc.right / 2,rc.bottom,ghWndMain,(HMENU)ID_TREEVIEW,ghInst,NULL);

	HWND hWndList = 
		CreateWindowEx(WS_EX_CLIENTEDGE,WC_LISTVIEW,NULL,
			LVS_ALIGNLEFT | WS_CHILDWINDOW | WS_TABSTOP | WS_VISIBLE,
			rc.right / 2 + 5,0,rc.right / 2 - 5,rc.bottom,ghWndMain,(HMENU)ID_LISTVIEW,ghInst,NULL);

	return TRUE; //(hWndList && hWndTree);
}
/////////////////////////////////////////////////////////////////////

//void RecalcLayout(int cx,int cy)
//{
//	cx -= CX_SPLIT_MARGIN;
//
//	s_layout.rcTree.left = 0;
//	s_layout.rcTree.right = (s_layout.pcSplit * cx) / 100;
//	s_layout.rcTree.top = 0;
//	s_layout.rcTree.bottom = cy;
//
//}

/////////////////////////////////////////////////////////////////////