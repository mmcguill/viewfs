/////////////////////////////////////////////////////////////////////
// 25/MAR/2005
//
//
//
//
/////////////////////////////////////////////////////////////////////

#include "stdafx.h"
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
	if(NULL == ppHandler || (0x06 != dwFSType && 0x0B != dwFSType && 0x0C != dwFSType))
	{
		return FALSE;
	}	
	
	GUIFATFSHandler *pTmp = new GUIFATFSHandler;
	if(NULL == pTmp)
	{
		return FALSE;
	}

	*ppHandler = static_cast<GUIFileSystemBaseHandler*>(pTmp);
	return TRUE;
}

/////////////////////////////////////////////////////////////////////