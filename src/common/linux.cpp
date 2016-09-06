/////////////////////////////////////////////////////////////////////
// 27/JUN/2006
//
// Project: viewfs
// File: linux.cpp
// Author: Mark McGuill
//
// Copyright (C) 2006  Mark McGuill
//
/////////////////////////////////////////////////////////////////////

#include "..\..\inc\linux.h"
#include <time.h>
#include <string.h>

/////////////////////////////////////////////////////////////////////

void LinuxFormatTime(TCHAR *lpszTime,unsigned long time)
{
	if(0 == lpszTime) 
	{
		return;
	}

	time_t t = time;
	struct tm *tmp = gmtime(&t);
	if(tmp)
	{
		_tcsftime(lpszTime,32,_T("%d/%b/%y %H:%M:%S"),tmp);
	}
	else
	{
		lpszTime[0] = 0;
	}
}

/////////////////////////////////////////////////////////////////////

void LinuxFormatPermissions(TCHAR *lpszPer, unsigned long dwMode)
{
	if(0 == lpszPer) 
	{
		return;
	}

	for(int i=0;i<12;i++)
	{
		lpszPer[i] = _T('-');
	}
	lpszPer[12] = 0;


	// Special bits
	if(S_ISUID == (S_ISUID & dwMode))
	{
		lpszPer[0] = _T('s');
	}

	if(S_ISGID == (S_ISGID & dwMode))
	{
		lpszPer[1] = _T('g');
	}

	if(S_ISVTX == (S_ISVTX  & dwMode))
	{
		lpszPer[2] = _T('s');
	}
	  	 
	
	// Owner

	if(S_IRUSR == (S_IRUSR & (S_IRWXU & dwMode)))
	{
		lpszPer[3] = _T('r');
	}
	if(S_IWUSR == (S_IWUSR & (S_IRWXU & dwMode)))
	{
		lpszPer[4] = _T('w');
	}
	if(S_IXUSR == (S_IXUSR & (S_IRWXU & dwMode)))
	{
		lpszPer[5] = _T('x');
	}  	  	 

	
	// Group

	if(S_IWGRP == (S_IWGRP & (S_IRWXG & dwMode)))
	{
		lpszPer[6] = _T('r');
	}
	if(S_IWGRP == (S_IWGRP & (S_IRWXG & dwMode)))
	{
		lpszPer[7] = _T('w');
	}
	if(S_IXGRP == (S_IXGRP & (S_IRWXG & dwMode)))
	{
		lpszPer[8] = _T('x');
	}

	
	// Other

	if(S_IROTH == (S_IROTH & (S_IRWXO & dwMode)))
	{
		lpszPer[9] = _T('r');
	}
	if(S_IWOTH == (S_IWOTH & (S_IRWXO & dwMode)))
	{
		lpszPer[10] = _T('w');
	}
	if(S_IXOTH == (S_IXOTH & (S_IRWXO & dwMode)))
	{
		lpszPer[11] = _T('x');
	}
}

/////////////////////////////////////////////////////////////////////

void LinuxFormatAttributes(TCHAR *lpszFmt, unsigned long dwMode)
{
	if(0 == lpszFmt) 
	{
		return;
	}

	if(IS_REGULAR_FILE(dwMode))
	{
		_tcscpy(lpszFmt,_T("File"));
	}
	else if(IS_DIRECTORY(dwMode))
	{
		_tcscpy(lpszFmt,_T("Directory"));
	}
	else if(IS_LINK(dwMode))
	{
		_tcscpy(lpszFmt,_T("Symbolic Link"));
	}
	else if(IS_SOCKET(dwMode))
	{
		_tcscpy(lpszFmt,_T("Socket"));
	}
	else if(IS_BLOCK_DEV(dwMode))
	{
		_tcscpy(lpszFmt,_T("Block device"));
	}
	else if(IS_CHARACTER_DEV(dwMode))
	{
		_tcscpy(lpszFmt,_T("Character Device"));
	}
	else if(IS_FIFO(dwMode))
	{
		_tcscpy(lpszFmt,_T("FIFO"));
	}
	else
	{
		lpszFmt[0] = 0;
	}
}

