/////////////////////////////////////////////////////////////////////
// 07/APR/2005
//
// Project: MMcGExt2FS
// File: dllmain.h
// Author: Mark McGuill
//
// Copyright (C) 2005  Mark McGuill
//
/////////////////////////////////////////////////////////////////////

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <time.h>
#include <shellapi.h>

#include <stack>
using namespace std;

#ifdef _DEBUG
	#define _CRTDBG_MAP_ALLOC
	#include <crtdbg.h>

	#define ASSERT(x) _ASSERTE(x)
#else
	#define ASSERT(x)
#endif

#include "..\..\inc\common.h"

extern HINSTANCE ghInst;