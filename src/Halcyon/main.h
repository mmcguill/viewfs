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

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <tchar.h>
#include <commctrl.h>

#ifdef _DEBUG
	#define _CRTDBG_MAP_ALLOC
	#include <crtdbg.h>

	#define ASSERT(x) _ASSERTE(x)
#else
	#define ASSERT(x)
#endif

extern HINSTANCE ghInst;
extern HWND ghWndMain;

int AppMessageBox(LPTSTR lpszMsg, UINT nStyle = MB_OK);
