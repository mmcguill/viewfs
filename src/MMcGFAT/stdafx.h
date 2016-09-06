// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once


#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

// Windows Header Files:
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

// TODO: reference additional headers your program requires here
