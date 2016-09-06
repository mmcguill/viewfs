/////////////////////////////////////////////////////////////////////
// 07/APR/2005
//
// Project: MMcGExt2FS
// File: MMcGExt2FS.cpp
// Author: Mark McGuill
//
// Copyright (C) 2005  Mark McGuill
//
/////////////////////////////////////////////////////////////////////

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <malloc.h>
#include <tchar.h>
#include "..\..\inc\linux.h"
#include "..\..\inc\viewfs_types.h"
#include "MMcGExt2FS.h"

/////////////////////////////////////////////////////////////////////

#define INIT_HARDCODED_SUPERBLOCK_START	0x400
#define INIT_HARDCODED_SUPERBLOCK_SIZE	0x1000
#define INIT_HARDCODED_SECTOR_SIZE		0x200

/////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
	#define _CRTDBG_MAP_ALLOC
	#include <crtdbg.h>

	#define ASSERT(x) _ASSERTE(x)
#else
	#define ASSERT(x)
#endif

/////////////////////////////////////////////////////////////////////

using namespace MMcGExt2FS;

/////////////////////////////////////////////////////////////////////

Ext2FSReader::Ext2FSReader()
{
	m_fInit = false;
	m_dwLastError = 0;
	m_hFile = NULL;
	m_llBeginFileOffset = 0;
	m_pSB = NULL;
	m_pBGs = NULL;


	/*_CrtSetDbgFlag(_CRTDBG_DELAY_FREE_MEM_DF | 
		_CRTDBG_CHECK_CRT_DF | _CRTDBG_ALLOC_MEM_DF |
		_CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_LEAK_CHECK_DF);*/
}

/////////////////////////////////////////////////////////////////////

Ext2FSReader::~Ext2FSReader()
{
	Close();
}

/////////////////////////////////////////////////////////////////////

bool Ext2FSReader::Init(HANDLE hFile, LONGLONG llBeginFileOffset)
{
	if (m_fInit)
	{
		ASSERT(!m_fInit);
		m_dwLastError = ERR_CALL_CLOSE_FIRST;
		return false;
	}

	// Duplicate File Handle So Caller Can forget it

	HANDLE hProc = GetCurrentProcess();
	if (!DuplicateHandle(hProc,hFile,hProc,&m_hFile,0,false,
		DUPLICATE_SAME_ACCESS))
	{
		m_dwLastError = ERR_OPENING_FILE;
		return false;
	}

	m_llBeginFileOffset = llBeginFileOffset;

	// Load Super Block

	LARGE_INTEGER liTmp;
	liTmp.QuadPart = m_llBeginFileOffset + INIT_HARDCODED_SUPERBLOCK_START;
	if(!SetFilePointerEx(m_hFile,liTmp,NULL,FILE_BEGIN))
	{
		m_dwLastError = ERR_READING_FILE;
		CloseHandle(m_hFile);
		m_hFile = NULL;
		return false;
	}


	BYTE *pBuffer = new BYTE[INIT_HARDCODED_SUPERBLOCK_SIZE];
	ASSERT(pBuffer);
	if(NULL == pBuffer)
	{
		CloseHandle(m_hFile);
		m_hFile = NULL;
		m_dwLastError = ERR_READING_FILE;
		return false;
	}

	// Read
	DWORD cbRead = 0;
	if(!ReadFile(m_hFile,pBuffer,INIT_HARDCODED_SUPERBLOCK_SIZE,&cbRead,NULL))
	{
		delete pBuffer;
		CloseHandle(m_hFile);
		m_hFile = NULL;
		m_dwLastError = ERR_READING_FILE;
		return false;
	}

	m_pSB = new ext2_super_block;
	
	ASSERT(m_pSB);
	if(NULL == m_pSB)
	{
		delete pBuffer;
		m_dwLastError = ERR_OUT_OF_MEMORY;
		CloseHandle(m_hFile);
		m_hFile = NULL;
		return false;
	}

	CopyMemory(m_pSB,pBuffer,sizeof(ext2_super_block));

	// Sanity Check

	ASSERT(EXT2FS_MAGIC == (WORD)m_pSB->s_magic);
	if(EXT2FS_MAGIC != (WORD)m_pSB->s_magic)
	{
		delete pBuffer;
		CloseHandle(m_hFile);
		m_hFile = NULL;
		m_dwLastError = ERR_FS_CORRUPT;
		return false;
	}

	delete pBuffer;

	
	// Load Group Descriptors, Read on a Block Boundary, calc 
	// with round up

	int cbBlock = CB_BLOCK(m_pSB);

	int cGroups = 
		(m_pSB->s_blocks_count + m_pSB->s_blocks_per_group - 1) /
		m_pSB->s_blocks_per_group; // Rounded Up

	ASSERT(cGroups != 0);
	if(0 == cGroups)
	{
		// WTF?
		CloseHandle(m_hFile);
		m_hFile = NULL;
		m_dwLastError = ERR_FS_CORRUPT;
		return false;
	}

	int cBlocks =	((cGroups * sizeof(ext2_group_desc)) + 
					(cbBlock - 1)) / cbBlock;

	
	int cbGroups = cBlocks * CB_BLOCK(m_pSB);
	pBuffer = new BYTE[cbGroups];
	ASSERT(pBuffer);
	if(NULL == pBuffer)
	{
		CloseHandle(m_hFile);
		m_hFile = NULL;
		m_dwLastError = ERR_READING_FILE;
		return false;
	}

	liTmp.QuadPart = m_llBeginFileOffset + 
		((m_pSB->s_first_data_block + 1) * cbBlock);

	if(!SetFilePointerEx(m_hFile,liTmp,NULL,FILE_BEGIN))
	{
		m_dwLastError = ERR_READING_FILE;
		CloseHandle(m_hFile);
		m_hFile = NULL;
		return false;
	}

	cbRead = 0;
	if(!ReadFile(m_hFile,pBuffer,cbGroups,&cbRead,NULL))
	{
		delete pBuffer;
		CloseHandle(m_hFile);
		m_hFile = NULL;
		m_dwLastError = ERR_READING_FILE;
		return false;
	}

	m_pBGs = new ext2_group_desc[cGroups];
	ASSERT(m_pBGs);
	if(NULL == m_pBGs)
	{
		delete pBuffer;
		CloseHandle(m_hFile);
		m_hFile = NULL;
		return false;
	}

	CopyMemory(m_pBGs,pBuffer,cGroups * sizeof(ext2_group_desc));
	delete pBuffer;

	m_fInit = true;
	return true;
}

/////////////////////////////////////////////////////////////////////

void Ext2FSReader::Close()
{
	m_fInit = false;

	if(m_hFile)
	{
		CloseHandle(m_hFile);
	}
	m_hFile = NULL;

	if(m_pSB)
	{
		delete m_pSB;
	}
	m_pSB = NULL;

	if(m_pBGs)
	{
		delete m_pSB;
	}
	m_pBGs = NULL;

	m_dwLastError = 0;
	m_llBeginFileOffset = 0;
}

/////////////////////////////////////////////////////////////////////

bool Ext2FSReader::GetItemFromPath(TCHAR* pszPath,Ext2FSItem *pItem)
{
	ASSERT(m_fInit);
	if(!m_fInit)
	{		
		m_dwLastError = ERR_NOT_INIT;
		return false;
	}

	ASSERT(pszPath);
	ASSERT(pItem);

	if(	NULL == pszPath || NULL == pItem)
	{
		m_dwLastError = ERR_INVALID_PARAM;
		return false;
	}


	// sanity check path should start with /

	if(_T('/') != pszPath[0])
	{
		m_dwLastError = ERR_INVALID_PARAM;
		return false;
	}

    pItem->llInode = EXT2_ROOT_INO;

	TCHAR *pCopy = _tcsdup(pszPath);
	TCHAR *pTok = pCopy;

	while(NULL != (pTok = _tcstok(pTok,_T("/"))))
	{
		_U32 cItems = 0;
	
		if(!ReadDirectory(pItem->llInode,NULL,&cItems))
		{
			free(pCopy);
			return false;
		}

		Ext2FSItem* pItems = new Ext2FSItem[cItems];
		if(NULL == pItems)
		{
			m_dwLastError = ERR_OUT_OF_MEMORY;
			free(pCopy);
			return false;
		}

		if(!ReadDirectory(pItem->llInode,pItems,&cItems))
		{
			m_dwLastError = ERR_PATH_NOT_FOUND;
			delete pItems;
			free(pCopy);
			return false;
		}

		_U32 i = 0;
		for(;i<cItems;i++)
		{
			if(_tcscmp(pItems[i].szName,pTok))
			{
				continue;
			}

			CopyMemory(pItem,&pItems[i],sizeof(Ext2FSItem));

			break;
		}

		delete pItems;

		if(i == cItems)
		{
			m_dwLastError = ERR_PATH_NOT_FOUND;
			free(pCopy);
			return false;
		}

		pTok = NULL; // Next Directory
	}

	free(pCopy);
	return true;
}

/////////////////////////////////////////////////////////////////////

bool Ext2FSReader::ReadDirectory(LONGLONG llInode, Ext2FSItem *pItems, 
								 _U32* pcItems)
{
	ASSERT(m_fInit);
	if(!m_fInit)
	{
		m_dwLastError = ERR_NOT_INIT;
		return false;
	}
	
	ASSERT(pcItems);
	if(NULL == pcItems)
	{
		m_dwLastError = ERR_INVALID_PARAM;
		return false;		 
	}


	ext2_inode inode = {0};
	if(!ReadInode(llInode,&inode))
	{
		return false;
	}

	BYTE *pBuffer = new BYTE[inode.i_size]; 
	ASSERT(pBuffer);
	if(NULL == pBuffer)
	{
		m_dwLastError = ERR_OUT_OF_MEMORY;
		return false;
	}

	if(!ReadItem(llInode,pBuffer))
	{
		delete pBuffer;
		return false;
	}

	// Parse Data

	_U32 cItems = 0;
	ext2_dir_entry* pDirEntry = (ext2_dir_entry*)pBuffer;
	ext2_inode in;

	while(	((BYTE*)pDirEntry < (pBuffer + inode.i_size)) && 
			pDirEntry->inode != 0) // Ignore Inodes with 0's which pad out small directories.
	{
        if(pItems && cItems < (*pcItems))
		{
			if(!ReadInode(pDirEntry->inode,&in))
			{
				return false;
			}

			pItems[cItems].dwMode = ((unsigned short)in.i_mode);

			pItems[cItems].fDeleted = false;
			pItems[cItems].llInode = pDirEntry->inode;
			pItems[cItems].size = in.i_size;
			pItems[cItems].dwAccess = in.i_atime;
			pItems[cItems].dwCreate = in.i_ctime;
			pItems[cItems].dwModify = in.i_mtime; 
			
			// TODO: Unicode friendly this 

			LPTSTR lpszName;
#ifdef _UNICODE
	TCHAR szTmp[EXT2_NAME_LEN];
	MultiByteToWideChar(CP_ACP,0,pDirEntry->name,pDirEntry->name_len,szTmp,EXT2_NAME_LEN);
	lpszName = szTmp;
#else
	lpszName = pDirEntry->name;
#endif

			_tcsncpy(pItems[cItems].szName,lpszName,pDirEntry->name_len);

 			pItems[cItems].szName[pDirEntry->name_len]=0;
		}

		cItems++;
		pDirEntry = (ext2_dir_entry*)
			//((BYTE*)pDirEntry + EXT2_DIR_REC_LEN(pDirEntry->name_len));
			(((BYTE*)(pDirEntry)) + pDirEntry->rec_len);
	}

	if(NULL == pItems || cItems > (*pcItems))
	{
		*pcItems = cItems;
		delete pBuffer; 
		m_dwLastError = ERR_NOT_ENOUGH_BUFFER;
		return true;
	}

	*pcItems = cItems;
	delete pBuffer; 
	return true;
}

/////////////////////////////////////////////////////////////////////

bool Ext2FSReader::ReadItem(LONGLONG llInode,BYTE *pBuffer)
{
	ASSERT(m_fInit);
	if(!m_fInit)
	{
		m_dwLastError = ERR_NOT_INIT;
		return false;
	}

	ASSERT(pBuffer);
	if(NULL == pBuffer)
	{
		return false;
	}

	ext2_inode inode;
	if(!ReadInode(llInode,&inode))
	{
		return false;
	}

	// may not use any blocks...

	if(inode.i_blocks == 0)
	{
		// Something really wrong if it doesnt fit within these blocks...

		if(inode.i_size > EXT2_TIND_BLOCK * sizeof(inode.i_block[0]))
		{
			ASSERT(false);
			return false;
		}

		CopyMemory(pBuffer,inode.i_block,inode.i_size);
		return true;
	}


	// but mostly it will...

	BYTE *pTmp = new BYTE[CB_BLOCK(m_pSB)]; 
	ASSERT(pTmp);
	if(NULL == pTmp)
	{
		m_dwLastError = ERR_OUT_OF_MEMORY;
		return false;
	}

	int bufOffset = 0;

	for(int i=0;i<EXT2_N_BLOCKS && inode.i_block[i] != 0;i++)
	{
		if(!ReadBlock(inode.i_block[i],pTmp))
		{
			delete pTmp;
			return false;
		}

		if(i >= EXT2_NDIR_BLOCKS)
		{
			if(i == EXT2_IND_BLOCK)
			{
				if(!ReadIndirectBlock(	inode.i_block[i],NULL,
										pBuffer,&bufOffset,
										inode.i_size,false))
				{
					delete pTmp;
					return false;
				}
			}
			else if(i == EXT2_DIND_BLOCK)
			{
				if(!ReadDIndirectBlock(	inode.i_block[i],NULL,
										pBuffer,&bufOffset,
										inode.i_size,false))
				{
					delete pTmp;
					return false;
				}
			}
			else if(i == EXT2_TIND_BLOCK)
			{
				if(!ReadTIndirectBlock(	inode.i_block[i],NULL,
										pBuffer,&bufOffset,
										inode.i_size,false))
				{
					delete pTmp;
					return false;
				}
			}
		}
		else
		{
			int cbCopy = ((inode.i_size - bufOffset) < CB_BLOCK(m_pSB)) ? 
							(inode.i_size - bufOffset) : 
							CB_BLOCK(m_pSB);

			CopyMemory(&pBuffer[bufOffset],pTmp,cbCopy);

			bufOffset += cbCopy;
		}
	}

	delete pTmp;
	return true;
}

/////////////////////////////////////////////////////////////////////

bool Ext2FSReader::ReadItemToFile(LONGLONG llInode, TCHAR *pszFile)
{
	ASSERT(m_fInit);
	if(!m_fInit)
	{
		m_dwLastError = ERR_NOT_INIT;
		return false;
	}

	ext2_inode inode;
	if(!ReadInode(llInode,&inode))
	{
		return false;
	}


	BYTE *pTmp = new BYTE[CB_BLOCK(m_pSB)]; 
	ASSERT(pTmp);
	if(NULL == pTmp)
	{
		m_dwLastError = ERR_OUT_OF_MEMORY;
		return false;
	}

	
	// Create The File

	HANDLE hFileOut = CreateFile(	pszFile,GENERIC_WRITE,
									FILE_SHARE_READ,NULL,
									CREATE_ALWAYS,0,NULL);

	if(INVALID_HANDLE_VALUE == hFileOut)
	{
		m_dwLastError = (ERROR_FILE_EXISTS == ::GetLastError()) ? 
						ERR_FILE_ALREADY_EXISTS	: 
						ERR_CREATE_FILE;

		delete pTmp;
		return false;
	}

	for(int i=0;i<EXT2_N_BLOCKS && inode.i_block[i] != 0;i++)
	{
		if(!ReadBlock(inode.i_block[i],pTmp))
		{
			delete pTmp;
			CloseHandle(hFileOut);
			DeleteFile(pszFile);
			return false;
		}

		if(i >= EXT2_NDIR_BLOCKS)
		{
			if(i == EXT2_IND_BLOCK)
			{
				if(!ReadIndirectBlock(	inode.i_block[i],hFileOut,
										NULL,0,inode.i_size,true))
				{
					delete pTmp;
					CloseHandle(hFileOut);
					DeleteFile(pszFile);
					return false;
				}
			}
			else if(i == EXT2_DIND_BLOCK)
			{
				if(!ReadDIndirectBlock(	inode.i_block[i],hFileOut,
										NULL,0,inode.i_size,true))
				{
					delete pTmp;
					CloseHandle(hFileOut);
					DeleteFile(pszFile);
					return false;
				}
			}
			else if(i == EXT2_TIND_BLOCK)
			{
				if(!ReadTIndirectBlock(	inode.i_block[i],hFileOut,
										NULL,0,inode.i_size,true))
				{
					delete pTmp;
					CloseHandle(hFileOut);
					DeleteFile(pszFile);
					return false;
				}
			}
		}
		else
		{
			int cbCopy = min(inode.i_size,CB_BLOCK(m_pSB)); 
			DWORD dwWritten;		
			if(!WriteFile(hFileOut,pTmp,cbCopy,&dwWritten,NULL) || 
				dwWritten != cbCopy)
			{
				delete pTmp;
				CloseHandle(hFileOut);
				DeleteFile(pszFile);
				return false;
			}
		}
	}

	delete pTmp;

	// SetEOF and close

	if( !SetFilePointer(hFileOut,inode.i_size,NULL,FILE_BEGIN) || 
		!SetEndOfFile(hFileOut))
	{
		CloseHandle(hFileOut);
		DeleteFile(pszFile);
		return false;
	}

	CloseHandle(hFileOut);
	return true;
}

/////////////////////////////////////////////////////////////////////

bool Ext2FSReader::ReadTIndirectBlock(	int nBlock, HANDLE hFileOut, 
										BYTE* pBuffer, int *pBufOffset, 
										int nSize, bool fUseFile)
{
	ASSERT(fUseFile || pBufOffset);
	ASSERT(fUseFile || pBuffer);

	BYTE *pBlock = new BYTE[CB_BLOCK(m_pSB)]; 
	ASSERT(pBlock);
	if(NULL == pBlock)
	{
		m_dwLastError = ERR_OUT_OF_MEMORY;
		return false;
	}

	if(!ReadBlock(nBlock,pBlock))
	{
		delete pBlock;
		return false;
	}

	for (int *pBlk=(int*)pBlock;
		(BYTE*)pBlk < (pBlock + CB_BLOCK(m_pSB)) && (*pBlk != 0);
		pBlk++)
	{
		ReadDIndirectBlock(*pBlk,hFileOut,pBuffer,pBufOffset,nSize,fUseFile);
	}

	delete pBlock;
	return true;
}
/////////////////////////////////////////////////////////////////////

bool Ext2FSReader::ReadDIndirectBlock(	int nBlock,HANDLE hFileOut, 
										BYTE* pBuffer, int *pBufOffset, 
										int nSize, bool fUseFile)
{
	ASSERT(fUseFile || pBufOffset);
	ASSERT(fUseFile || pBuffer);

	BYTE *pBlock = new BYTE[CB_BLOCK(m_pSB)]; 
	ASSERT(pBlock);
	if(NULL == pBlock)
	{
		m_dwLastError = ERR_OUT_OF_MEMORY;
		return false;
	}

	if(!ReadBlock(nBlock,pBlock))
	{
		delete pBlock;
		return false;
	}

	for (int *pBlk=(int*)pBlock;
		(BYTE*)pBlk < (pBlock + CB_BLOCK(m_pSB)) && (*pBlk != 0);
		pBlk++)
	{
		ReadIndirectBlock(*pBlk,hFileOut,pBuffer,pBufOffset,nSize,fUseFile);
	}

	delete pBlock;
	return true;
}

/////////////////////////////////////////////////////////////////////

bool Ext2FSReader::ReadIndirectBlock(int nBlock, HANDLE hFileOut, 
									 BYTE* pBuffer, int *pBufOffset, 
									 int nSize, bool fUseFile)
{
	ASSERT(fUseFile || pBufOffset);
	ASSERT(fUseFile || pBuffer);

	int bufOffset;
	if(!fUseFile)
	{
		bufOffset = *pBufOffset;
	}

	BYTE *pBlock = new BYTE[CB_BLOCK(m_pSB)]; 
	ASSERT(pBlock);
	if(NULL == pBlock)
	{
		m_dwLastError = ERR_OUT_OF_MEMORY;
		return false;
	}

	if(!ReadBlock(nBlock,pBlock))
	{
		delete pBlock;
		return false;
	}
	
	BYTE *pInd = new BYTE[CB_BLOCK(m_pSB)]; 
	ASSERT(pInd);
	if(NULL == pInd)
	{
		delete pBlock;
		m_dwLastError = ERR_OUT_OF_MEMORY;
		return false;
	}

	for (int *pBlk=(int*)pBlock;
		(BYTE*)pBlk < (pBlock + CB_BLOCK(m_pSB)) && (*pBlk != 0);
		pBlk++)
	{
		if(!ReadBlock(*pBlk,pInd))
		{
			delete pInd;
			delete pBlock;
			return false;
		}

		if(fUseFile)
		{
			DWORD dwWritten;		
			if(!WriteFile(hFileOut,pInd,CB_BLOCK(m_pSB),&dwWritten,NULL) || 
				dwWritten != CB_BLOCK(m_pSB))
			{
				delete pInd;
				delete pBlock;
				return false;
			}
		}
		else
		{
			int cbCopy = 
				((nSize - bufOffset) < CB_BLOCK(m_pSB)) ? 
					(nSize - bufOffset) : 
					CB_BLOCK(m_pSB);

			CopyMemory(&pBuffer[bufOffset],pInd,cbCopy);

			bufOffset += cbCopy;
		}
	}

	delete pInd;
	delete pBlock;

	if(!fUseFile)
	{
		*pBufOffset = bufOffset;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////
// nBlock is zero based

bool Ext2FSReader::ReadBlock(DWORD nBlock, BYTE *pBlock)
{
	ASSERT(m_fInit);
	ASSERT(pBlock);
	if(!m_fInit || pBlock==NULL)
	{
		m_dwLastError = ERR_INVALID_PARAM;
		return false;
	}

	// Calc offset

	ULONGLONG cbOffset = (ULONGLONG)nBlock * CB_BLOCK(m_pSB);

	// Read in this block
	
	LARGE_INTEGER liTmp;
	liTmp.QuadPart = m_llBeginFileOffset + cbOffset;

	if(!SetFilePointerEx(m_hFile,liTmp,NULL,FILE_BEGIN))
	{
		m_dwLastError = ERR_READING_FILE;
		return false;
	}

	DWORD cbRead = 0;
	if(!ReadFile(m_hFile,pBlock,CB_BLOCK(m_pSB),&cbRead,NULL)|| 
		cbRead != CB_BLOCK(m_pSB))
	{
		m_dwLastError = ERR_READING_FILE;
		return false;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////
// llInode is one based

bool Ext2FSReader::ReadInode(LONGLONG llInode, ext2_inode *pInode)
{
	ASSERT(m_fInit);
	if(!m_fInit)
	{
		return false;
	}

	// Inode is one based...

	llInode--;

	// Find Block Group

	int nBlockGroup =	(int)llInode / m_pSB->s_inodes_per_group;
	int nInodeInGroup = (int)llInode % m_pSB->s_inodes_per_group;
	int nBlockWithinGroup = 
		(nInodeInGroup  * sizeof(ext2_inode))/
		CB_BLOCK(m_pSB);

	int nInodeWithinBlock = 
		( (nInodeInGroup * sizeof(ext2_inode)) -
		  (nBlockWithinGroup * CB_BLOCK(m_pSB)) ) / 
					sizeof(ext2_inode);

	DWORD cbRead = 0;
	BYTE *pBuffer = new BYTE[CB_BLOCK(m_pSB)];
	ASSERT(pBuffer);
	if(NULL == pBuffer)
	{
		m_dwLastError = ERR_OUT_OF_MEMORY;
		return false;
	}

	if(!ReadBlock(m_pBGs[nBlockGroup].bg_inode_table+nBlockWithinGroup,
					pBuffer))
	{
		delete pBuffer;
		return false;
	}

	ext2_inode *pTmp = (ext2_inode *)pBuffer;
	pTmp += nInodeWithinBlock;

	CopyMemory(pInode,pTmp,sizeof(ext2_inode));

	// TODO: Cache this

	delete pBuffer;
	return true;
}

/////////////////////////////////////////////////////////////////////
