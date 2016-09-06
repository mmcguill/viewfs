/////////////////////////////////////////////////////////////////////
// 07/APR/2005
//
// Project: MMcGExt2FS
// File: dllmain.cpp
// Author: Mark McGuill
//
// Copyright (C) 2005  Mark McGuill
//
/////////////////////////////////////////////////////////////////////

#include "dllmain.h"
#include "..\..\inc\viewfs_types.h"
#include "GUIHandler.h"

/////////////////////////////////////////////////////////////////////

HINSTANCE ghInst = NULL;

/////////////////////////////////////////////////////////////////////

BOOL APIENTRY DllMain( HANDLE hModule,DWORD  ul_reason_for_call,
					  LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		ghInst = (HINSTANCE)hModule;
		DisableThreadLibraryCalls((HMODULE)hModule);
		break;
	case DLL_PROCESS_DETACH:
		break;
	}
    return TRUE;
}

/////////////////////////////////////////////////////////////////////

BOOL GetGUIFSHandler(DWORD dwFSType, GUIFileSystemBaseHandler** ppHandler)
{
	if(NULL == ppHandler || 0x83 != dwFSType)
	{
		return FALSE;
	}	
	
	GUIExt2FSHandler  *pTmp = new GUIExt2FSHandler;
	if(NULL == pTmp)
	{
		return FALSE;
	}

	*ppHandler = static_cast<GUIFileSystemBaseHandler*>(pTmp);
	return TRUE;
}

/////////////////////////////////////////////////////////////////////