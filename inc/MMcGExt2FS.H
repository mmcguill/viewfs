/////////////////////////////////////////////////////////////////////
// 07/APR/2005
//
// Project: MMcGExt2FS
// File: MMcGExt2FS.h
// Author: Mark McGuill
//
// Copyright (C) 2005  Mark McGuill
//
/////////////////////////////////////////////////////////////////////

#ifdef MMCGEXT2FS_API_EXPORTS
	#define MMCGEXT2FS_API __declspec(dllexport)
#else
	#define MMCGEXT2FS_API __declspec(dllimport)
#endif

/////////////////////////////////////////////////////////////////////

namespace MMcGExt2FS
{

/////////////////////////////////////////////////////////////////////

typedef long			__le32;
typedef short			__le16;
typedef unsigned long	__u32;
typedef unsigned short	__u16;
typedef unsigned char	__u8;

/////////////////////////////////////////////////////////////////////

#define EXT2FS_MAGIC 0xef53
#define CB_BLOCK(x) (((1 << x->s_log_block_size) * 0x400))

/*
 * Constants relative to the data blocks
 */
#define	EXT2_NDIR_BLOCKS		12
#define	EXT2_IND_BLOCK			EXT2_NDIR_BLOCKS
#define	EXT2_DIND_BLOCK			(EXT2_IND_BLOCK + 1)
#define	EXT2_TIND_BLOCK			(EXT2_DIND_BLOCK + 1)
#define	EXT2_N_BLOCKS			(EXT2_TIND_BLOCK + 1)

/////////////////////////////////////////////////////////////////////

#pragma pack(push, 1)

/////////////////////////////////////////////////////////////////////

struct ext2_super_block {
	__le32	s_inodes_count;		/* Inodes count */
	__le32	s_blocks_count;		/* Blocks count */
	__le32	s_r_blocks_count;	/* Reserved blocks count */
	__le32	s_free_blocks_count;	/* Free blocks count */
	__le32	s_free_inodes_count;	/* Free inodes count */
	__le32	s_first_data_block;	/* First Data Block */
	__le32	s_log_block_size;	/* Block size */
	__le32	s_log_frag_size;	/* Fragment size */
	__le32	s_blocks_per_group;	/* # Blocks per group */
	__le32	s_frags_per_group;	/* # Fragments per group */
	__le32	s_inodes_per_group;	/* # Inodes per group */
	__le32	s_mtime;		/* Mount time */
	__le32	s_wtime;		/* Write time */
	__le16	s_mnt_count;		/* Mount count */
	__le16	s_max_mnt_count;	/* Maximal mount count */
	__le16	s_magic;		/* Magic signature */
	__le16	s_state;		/* File system state */
	__le16	s_errors;		/* Behaviour when detecting errors */
	__le16	s_minor_rev_level; 	/* minor revision level */
	__le32	s_lastcheck;		/* time of last check */
	__le32	s_checkinterval;	/* max. time between checks */
	__le32	s_creator_os;		/* OS */
	__le32	s_rev_level;		/* Revision level */
	__le16	s_def_resuid;		/* Default uid for reserved blocks */
	__le16	s_def_resgid;		/* Default gid for reserved blocks */
	/*
	 * These fields are for EXT2_DYNAMIC_REV superblocks only.
	 *
	 * Note: the difference between the compatible feature set and
	 * the incompatible feature set is that if there is a bit set
	 * in the incompatible feature set that the kernel doesn't
	 * know about, it should refuse to mount the filesystem.
	 * 
	 * e2fsck's requirements are more strict; if it doesn't know
	 * about a feature in either the compatible or incompatible
	 * feature set, it must abort and not try to meddle with
	 * things it doesn't understand...
	 */
	__le32	s_first_ino; 		/* First non-reserved inode */
	__le16   s_inode_size; 		/* size of inode structure */
	__le16	s_block_group_nr; 	/* block group # of this superblock */
	__le32	s_feature_compat; 	/* compatible feature set */
	__le32	s_feature_incompat; 	/* incompatible feature set */
	__le32	s_feature_ro_compat; 	/* readonly-compatible feature set */
	__u8	s_uuid[16];		/* 128-bit uuid for volume */
	char	s_volume_name[16]; 	/* volume name */
	char	s_last_mounted[64]; 	/* directory where last mounted */
	__le32	s_algorithm_usage_bitmap; /* For compression */
	/*
	 * Performance hints.  Directory preallocation should only
	 * happen if the EXT2_COMPAT_PREALLOC flag is on.
	 */
	__u8	s_prealloc_blocks;	/* Nr of blocks to try to preallocate*/
	__u8	s_prealloc_dir_blocks;	/* Nr to preallocate for dirs */
	__u16	s_padding1;
	/*
	 * Journaling support valid if EXT3_FEATURE_COMPAT_HAS_JOURNAL set.
	 */
	__u8	s_journal_uuid[16];	/* uuid of journal superblock */
	__u32	s_journal_inum;		/* inode number of journal file */
	__u32	s_journal_dev;		/* device number of journal file */
	__u32	s_last_orphan;		/* start of list of inodes to delete */
	__u32	s_hash_seed[4];		/* HTREE hash seed */
	__u8	s_def_hash_version;	/* Default hash version to use */
	__u8	s_reserved_char_pad;
	__u16	s_reserved_word_pad;
	__le32	s_default_mount_opts;
 	__le32	s_first_meta_bg; 	/* First metablock block group */
	__u32	s_reserved[190];	/* Padding to the end of the block */
};

/////////////////////////////////////////////////////////////////////

struct ext2_group_desc
{
	__le32	bg_block_bitmap;		/* Blocks bitmap block */
	__le32	bg_inode_bitmap;		/* Inodes bitmap block */
	__le32	bg_inode_table;		/* Inodes table block */
	__le16	bg_free_blocks_count;	/* Free blocks count */
	__le16	bg_free_inodes_count;	/* Free inodes count */
	__le16	bg_used_dirs_count;	/* Directories count */
	__le16	bg_pad;
	__le32	bg_reserved[3];
};

/////////////////////////////////////////////////////////////////////

struct ext2_inode {
	__le16	i_mode;		/* File mode */
	__le16	i_uid;		/* Low 16 bits of Owner Uid */
	__le32	i_size;		/* Size in bytes */
	__le32	i_atime;	/* Access time */
	__le32	i_ctime;	/* Creation time */
	__le32	i_mtime;	/* Modification time */
	__le32	i_dtime;	/* Deletion Time */
	__le16	i_gid;		/* Low 16 bits of Group Id */
	__le16	i_links_count;	/* Links count */
	__le32	i_blocks;	/* Blocks count */
	__le32	i_flags;	/* File flags */
	__le32  l_i_reserved1;
	__le32	i_block[EXT2_N_BLOCKS];/* Pointers to blocks */
	__le32	i_generation;	/* File version (for NFS) */
	__le32	i_file_acl;	/* File ACL */
	__le32	i_dir_acl;	/* Directory ACL */
	__le32	i_faddr;	/* Fragment address */
	__u8	l_i_frag;	/* Fragment number */
	__u8	l_i_fsize;	/* Fragment size */
	__u16	i_pad1;
	__le16	l_i_uid_high;	/* these 2 fields    */
	__le16	l_i_gid_high;	/* were reserved2[0] */
	__u32	l_i_reserved2;
};

/////////////////////////////////////////////////////////////////////

#define EXT2_NAME_LEN 255

struct ext2_dir_entry {
	__le32	inode;			/* Inode number */
	__le16	rec_len;		/* Directory entry length */
	__u8	name_len;		/* Name length */
	__u8	file_type;
	char	name[255];
};

#define EXT2_DIR_PAD		 	4
#define EXT2_DIR_ROUND 			(EXT2_DIR_PAD - 1)
#define EXT2_DIR_REC_LEN(name_len)	(((name_len) + 8 + EXT2_DIR_ROUND) & \
					 ~EXT2_DIR_ROUND)

// name follows

/////////////////////////////////////////////////////////////////////

#pragma pack(pop)

/////////////////////////////////////////////////////////////////////
// Special Inodes

#define EXT2_BAD_INO  			1
#define EXT2_ROOT_INO 			2
#define EXT2_ACL_IDX_INO 		3
#define EXT2_ACL_DATA_INO 		4
#define EXT2_BOOT_LOADER_INO 	5
#define EXT2_UNDEL_DIR_INO 		6
#define EXT2_FIRST_INO 			11

/////////////////////////////////////////////////////////////////////

enum MMcGExt2FS_Error
{
	ERR_NO_ERROR,
	ERR_CALL_CLOSE_FIRST,
	ERR_OPENING_FILE,
	ERR_READING_FILE,
	ERR_OUT_OF_MEMORY,
	ERR_FS_CORRUPT,
	ERR_INVALID_PARAM,
	ERR_NOT_ENOUGH_BUFFER,
	ERR_CREATE_FILE,
	ERR_NOT_INIT,
	ERR_PATH_NOT_FOUND,
	ERR_FILE_ALREADY_EXISTS,
};

/////////////////////////////////////////////////////////////////////

typedef struct _Ext2FSItem
{
	TCHAR szName[256];
	LONGLONG llInode;
	LONGLONG size;
	DWORD dwCreate;
	DWORD dwModify;
	DWORD dwAccess;
	DWORD dwMode;
	bool fDeleted;
}Ext2FSItem;

/////////////////////////////////////////////////////////////////////

class MMCGEXT2FS_API Ext2FSReader
{
public:
	Ext2FSReader();
	~Ext2FSReader();

public:
	bool	Init(HANDLE hFile, LONGLONG llBeginFileOffset);
	void	Close();
	DWORD	GetLastError() {return m_dwLastError;}

	bool	GetItemFromPath(TCHAR* pszPath,Ext2FSItem *pItem);

	bool	ReadDirectory(LONGLONG llInode, Ext2FSItem *pItems, 
								 _U32* pcItems);

	bool	ReadItem(LONGLONG llInode, _U8* pBuffer);

	bool	ReadItemToFile(LONGLONG llInode, TCHAR *pszFile);

protected:
	bool	ReadInode(LONGLONG llInode, ext2_inode *pInode);

	bool	ReadIndirectBlock(	int nBlock, HANDLE hFileOut, BYTE* pBuffer, 
								int *pBufOffset, int nSize, 
								bool fUseFile);

	bool	ReadDIndirectBlock(	int nBlock, HANDLE hFileOut, BYTE* pBuffer, 
								int *pBufOffset, int nSize, 
								bool fUseFile);

	bool	ReadTIndirectBlock(	int nBlock, HANDLE hFileOut, BYTE* pBuffer, 
								int *pBufOffset, int nSize,
								bool fUseFile);

	bool	ReadBlock(DWORD nBlock, BYTE *pBlock);

protected:
	DWORD				m_dwLastError;
	bool				m_fInit;
	HANDLE				m_hFile;
	LONGLONG			m_llBeginFileOffset;
	ext2_super_block	*m_pSB;
	ext2_group_desc		*m_pBGs;
private:

private:
};

/////////////////////////////////////////////////////////////////////

} // namespace MMcGExt2FS

/////////////////////////////////////////////////////////////////////