/////////////////////////////////////////////////////////////////////
// 27/JUN/2006
//
// Project: viewfs
// File: linux.h
// Author: Mark McGuill
//
// Copyright (C) 2006  Mark McGuill
//
/////////////////////////////////////////////////////////////////////

#include <tchar.h>

#define S_IFMT  	0xF000	// format mask
#define S_IFSOCK 	0xC000 	// socket
#define S_IFLNK 	0xA000 	// symbolic link
#define S_IFREG 	0x8000 	// regular file
#define S_IFBLK 	0x6000 	// block device
#define S_IFDIR 	0x4000 	// directory
#define S_IFCHR 	0x2000 	// character device
#define S_IFIFO 	0x1000 	// fifo
  	  	 
#define S_ISUID 	0x0800 	// SUID
#define S_ISGID 	0x0400 	// SGID
#define S_ISVTX 	0x0200 	// sticky bit
	  	 
#define S_IRWXU 	0x01C0 	// user mask
#define S_IRUSR 	0x0100 	// read
#define S_IWUSR 	0x0080 	// write
#define S_IXUSR 	0x0040 	// execute
  	  	 
#define S_IRWXG 	0x0038 	// group mask
#define S_IRGRP 	0x0020 	// read
#define S_IWGRP 	0x0010 	// write
#define S_IXGRP 	0x0008 	// execute
  	  	 
#define S_IRWXO 	0x0007 	// other mask
#define S_IROTH 	0x0004 	// read
#define S_IWOTH 	0x0002 	// write
#define S_IXOTH 	0x0001 	// execute

///////////////////////////////////////////////////////////////////////

#define IS_MODE_TYPE(mode,flag) \
	((((mode & S_IFMT) & flag) == flag) && \
	(((mode & S_IFMT) & ~ flag) == 0)) \

#define IS_SOCKET(mode)				IS_MODE_TYPE(mode, S_IFSOCK)
#define IS_DIRECTORY(mode)			IS_MODE_TYPE(mode, S_IFDIR)
#define IS_LINK(mode)				IS_MODE_TYPE(mode, S_IFLNK)
#define IS_REGULAR_FILE(mode)		IS_MODE_TYPE(mode, S_IFREG)
#define IS_BLOCK_DEV(mode)			IS_MODE_TYPE(mode, S_IFBLK)
#define IS_CHARACTER_DEV(mode)		IS_MODE_TYPE(mode, S_IFCHR)
#define IS_FIFO(mode)				IS_MODE_TYPE(mode, S_IFIFO)

///////////////////////////////////////////////////////////////////////

void LinuxFormatTime(TCHAR *pszTime,unsigned long time);
void LinuxFormatPermissions(TCHAR *lpszPer, unsigned long dwMode);
void LinuxFormatAttributes(TCHAR *pszFmt, unsigned long dwMode);

/////////////////////////////////////////////////////////////////////