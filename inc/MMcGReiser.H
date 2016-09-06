/////////////////////////////////////////////////////////////////////
// 24/JUN/2006
//
// Project: MMcGReiser
// File: MMcGReiser.h
// Author: Mark McGuill
//
// Copyright (C) 2005  Mark McGuill
//
// With great help from...
// http://homes.cerias.purdue.edu/~florian/reiser/reiserfs.php
//
/////////////////////////////////////////////////////////////////////

#include <map>
#include <vector>
using namespace std;

#ifdef MMCGREISER_EXPORTS
	#define MMCGREISER_API __declspec(dllexport)
#else
	#define MMCGREISER_API __declspec(dllimport)
#endif

/////////////////////////////////////////////////////////////////////

namespace MMcGReiser
{

/////////////////////////////////////////////////////////////////////

#pragma pack(push, 1)

/////////////////////////////////////////////////////////////////////

#define REISER_MAGIC	"ReIsErFs"
#define REISER2_MAGIC	"ReIsEr2Fs"
#define REISER3_MAGIC	"ReIsEr3Fs" 

#define REISER_MAGIC_OK(pMagic)									\
				(	0 == _stricmp(pMagic,REISER_MAGIC) ||		\
					0 == _stricmp(pMagic,REISER2_MAGIC) ||		\
					0 == _stricmp(pMagic,REISER3_MAGIC))

/////////////////////////////////////////////////////////////////////

struct reiser_super_block {
	_U32	BlockCount;
	_U32	FreeBlocks;
	_U32	RootBlock;
	_U32	JournalBlock;
	_U32	JournalDevice;
	_U32	OrigJournalSize;
	_U32	JournalTransMax;
	_U32	JournalMagic;
	_U32	JournalMaxBatch;
	_U32	JournalMaxCommitAge;
	_U32	JournalMaxTransAge;
	_U16	BlockSize;
	_U16	OIDMaxSize;
	_U16	OIDCurrentSize;
	_U16	State;
	char    MagicString[12];
    _U32	HashFunctionCode;
	_U16	TreeHeight;
	_U16	BitmapNumber;
	_U16	Version;
	_U16	reserved;
	_U32	InodeGeneration;
};

/////////////////////////////////////////////////////////////////////

struct reiser_key_v2
{
	_U32 DirectoryID;
	_U32 ObjectID;
	_U64 Offset:60;
	_U64 Type:4;
};

struct reiser_key_v1
{
	_U32 DirectoryID;
	_U32 ObjectID;
	_U32 Offset;
	_U32 Type;
};

union reiser_key
{
	reiser_key_v1 v1;
	reiser_key_v2 v2;
};

struct working_reiser_key
{
	_U32 DirectoryID;
	_U32 ObjectID;
	_U64 Offset;
	_U32 Type;
};

// This is the ugly hack mentioned in the Keys section of the 
// above mentioned document. Only used in one place @ moment...
// sigh...

#define is_key_format_v1(type) (((type) == 0) || ((type) == 15))

/////////////////////////////////////////////////////////////////////

struct reiser_block_header
{
	_U16 Level;
	_U16 cItems;
	_U16 FreeSpace;
	_U16 reserved;
	reiser_key RightKey;
};

/////////////////////////////////////////////////////////////////////

struct reiser_pointer
{
	_U32 BlockNumber;
	_U16 Size;
	_U16 reserved;
};

/////////////////////////////////////////////////////////////////////

struct reiser_item_header
{
	reiser_key Key;
	_U16 Count;
	_U16 Length;
	_U16 Location;
	_U16 Version;
};

///////////////////////////////////////////////////////////////////////

struct reiser_stat_item_v1
{
_U16 Mode;				// file type and permissions
_U16 NumLinks;			// number of hard links
_U16 UID; 				// user id
_U16 GID;				// group id
_U32 Size; 			// file size in bytes
_U32 Atime;			// time of last access
_U32 Mtime;			// time of last modification
_U32 Ctime;			// time stat data was last changed
_U32 RdevBlocks;		// Device number 
_U32 BlocksUsed;		// number of blocks file uses
_U32 FirstDirByte;		// first byte of file which is stored in a direct item
						// if it equals 1 it is a symlink
						// if it equals 0xffffffff there is no direct item.
};

struct reiser_stat_item_v2
{
_U16 Mode;				// file type and permissions
_U16 Reserved; 	
_U32 NumLinks;			// number of hard links
_U64 Size; 			// file size in bytes
_U32 UID; 				// user id
_U32 GID; 				// group id
_U32 Atime; 			// time of last access
_U32 Mtime; 			// time of last modification
_U32 Ctime; 			// time stat data was last changed
_U32 Blocks; 			// number of blocks file uses
_U32 RdevGenFirst;		// Device number/
						// File's generation/
						// first byte of file which is stored in a direct item
						// if it equals 1 it is a symlink
						// if it equals 0xffffffff there is no direct item.
};

union reiser_stat_item
{
	reiser_stat_item_v1 v1;
	reiser_stat_item_v2 v2;
};

/////////////////////////////////////////////////////////////////////

struct reiser_directory_header
{
	_U32 HashGen;
	_U32 DirectoryID;
	_U32 ObjectID;
	_U16 NameLocation;
	_U16 State;
};

#pragma pack(pop)

/////////////////////////////////////////////////////////////////////

enum MMcGReiser_Error
{
	ERR_NO_ERROR,
	ERR_NO_SUCH_KEY,
	ERR_CALL_CLOSE_FIRST,
	ERR_OPENING_FILE,
	ERR_READING_FILE,
	ERR_OUT_OF_MEMORY,
	ERR_FS_CORRUPT,
	ERR_INVALID_PARAM,
	ERR_NOT_ENOUGH_BUFFER,
	ERR_NOT_INIT,
	ERR_NOT_DIRECTORY,
	ERR_CREATE_FILE,
	ERR_PATH_NOT_FOUND,
	ERR_FILE_ALREADY_EXISTS,
};

/////////////////////////////////////////////////////////////////////

#define REISER_CCH_MAX_NAME 256

typedef struct _ReiserItem
{
	TCHAR szName[REISER_CCH_MAX_NAME];
	_U32 dwDirectoryID;
	_U32 dwObjectID;
	_U64 size;
	_U32 dwCreate;
	_U32 dwModify;
	_U32 dwAccess;
	_U32 dwMode;
}ReiserItem;

/////////////////////////////////////////////////////////////////////

typedef bool (*PFNSEARCH)(void* pParam,reiser_item_header *pItemHdr,_U8* pBlock);

/////////////////////////////////////////////////////////////////////

struct STAT_ITEM_PARAM 
{
	reiser_item_header hdr;
	reiser_stat_item stat_item;
};

/////////////////////////////////////////////////////////////////////

struct READ_DIRECTORY_PARAM
{
	ReiserItem *pItems;
	_U32* pcItemsRequired;
	_U32 cItemsWritten; // used to keep track across multiple CB's
	_U32 cItemsTotal;
};

/////////////////////////////////////////////////////////////////////

struct READ_ITEM_PARAM
{
	_U8  *pData;
	_U32 cData;
	_U32 cbWritten;  // used to keep track across multiple CB's
	reiser_super_block* pSB;
	_U64 llBeginFileOffset;
	HANDLE hFileImage;
};

/////////////////////////////////////////////////////////////////////

struct READ_ITEM_TO_FILE_PARAM
{
	HANDLE hFileWrite;
	HANDLE hFileImage;
	reiser_super_block* pSB;
	_U64 llBeginFileOffset;
	_U64 FileSize;
};

/////////////////////////////////////////////////////////////////////

enum CompareKeyOption
{
	CompareKeyNormal,
	CompareKeyIgnoreOffset,
};

/////////////////////////////////////////////////////////////////////

struct cache_item
{
	_U8* pBlk;
	_U32 lastAccessed;
};

/////////////////////////////////////////////////////////////////////

class MMCGREISER_API ReiserReader
{
public:
	ReiserReader();
	~ReiserReader();

public:
	bool	Init(HANDLE hFile, _I64 llBeginFileOffset);
	void	Close();
	_U32	GetLastError() {return m_dwLastError;}

	bool	ReadDirectory(	_U32 DirectoryID, _U32 ObjectID, 
							ReiserItem *pItems, _U32* pcItems);

	bool ReadItem(	_U32 DirectoryID, _U32 ObjectID, 
					_U8 *pData, _U32* pcData);

	bool ReadItemToFile(_U32 DirectoryID, _U32 ObjectID, TCHAR *pszFile);

	bool ReadSymbolicLink(	_U32 DirectoryID,_U32 ObjectID, 
							TCHAR *pszBuf,_U32 *pcchBuf);

	bool GetItemFromPath(TCHAR *pszPath,ReiserItem* pItem);
	
#ifdef _DEBUG 
	bool	RecDumpFSTree(_U32 block);
	bool	DumpFSTree();
#endif

protected:
	bool LoadBlock(_U32 block,_U8* pBlk);

	bool SearchFromRoot(working_reiser_key* pKeySearch, PFNSEARCH pfnCallback, 
						void *pParam);

	_I32 CompareKey(	working_reiser_key* p1,working_reiser_key* p2,
						CompareKeyOption opt);


	bool RecSearchBlock(	_U32 block, working_reiser_key* pKeySearch, 
						PFNSEARCH pfnCallback, void *pParam);
									
	static bool StatItemCB(void* pParam,reiser_item_header *pItemHdr,_U8* pBlock);
	static bool ReadDirectoryCB(void* pParam,reiser_item_header *pItemHdr,_U8* pBlock);

	static bool ReadDirectItemCB(void* pParam,reiser_item_header *pItemHdr,_U8* pBlock);
	static bool ReadIndirectItemCB(void* pParam,reiser_item_header *pItemHdr,_U8* pBlock);

	static bool ReadDirectItemToFileCB(void* pParam,reiser_item_header *pItemHdr, _U8* pBlock);
	static bool ReadIndirectItemToFileCB(void* pParam,reiser_item_header *pItemHdr, _U8* pBlock);

protected:
	_U32				m_dwLastError;
	bool				m_fInit;
	HANDLE				m_hFile;
	_I64				m_llBeginFileOffset;
	reiser_super_block	*m_pSB;
	
	map<_U32, cache_item>	*m_pmapMUBlockCache;
	_U32					m_lb_time;
	
	vector<_U8*>			*m_pvecRSBCache;
	_U32					m_RSBLevel;
	_U32					m_RSBMaxLevel;

	_U32					m_cDirectType;
	_U32					m_cIndirectType;
};

/////////////////////////////////////////////////////////////////////

} // namespace MMcGReiser

/////////////////////////////////////////////////////////////////////