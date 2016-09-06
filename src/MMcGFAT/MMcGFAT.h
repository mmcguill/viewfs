/////////////////////////////////////////////////////////////////////
//
//
//
//
//
/////////////////////////////////////////////////////////////////////

#pragma pack(1)

typedef struct _BIOSParameterBlock
{
	WORD BPB_BytsPerSec;
	BYTE BPB_SecPerClus;
	WORD BPB_RsvdSecCnt;
	BYTE BPB_NumFATs;
	WORD BPB_RootEntCnt;
	WORD BPB_TotSec16;
	BYTE BPB_Media;
	WORD BPB_FATSz16;
	WORD BPB_SecPerTrk;
	WORD BPB_NumHeads;
	DWORD BPB_HiddSec;
	DWORD BPB_TotSec32;
}BIOSParameterBlock;

/////////////////////////////////////////////////////////////////////

typedef struct _FAT16_bpbex
{
	BYTE BS_DrvNum;
	BYTE BS_Reserved1;
	BYTE BS_BootSig;
	BYTE BS_VolID[4];
	BYTE BS_VolLab[11];
	BYTE BS_FilSysType[8];
}FAT16_bpbex;

/////////////////////////////////////////////////////////////////////

typedef struct _FAT32_bpbex
{
	DWORD BPB_FATSz32;		
	WORD BPB_ExtFlags;		
	WORD BPB_FSVer;			
	DWORD BPB_RootClus;		
	WORD BPB_FSInfo;		
	WORD BPB_BkBootSec;		
	BYTE BPB_Reserved[12];	
	BYTE BS_DrvNum;			
	BYTE BS_Reserved1;		
	BYTE BS_BootSig;		
	BYTE BS_VolID[4];		
	BYTE BS_VolLab[11];		
	BYTE BS_FilSysType[8];	
}FAT32_bpbex;

/////////////////////////////////////////////////////////////////////

typedef struct _BootSector
{
	BYTE jump[3];
	BYTE oemName[8];
	BIOSParameterBlock bios_param;
	union 
	{
		FAT16_bpbex fat16;
		FAT32_bpbex fat32;
	}FAT_Dep;
}BootSector;

/////////////////////////////////////////////////////////////////////

typedef struct _Dir_Entry
{
	BYTE DIR_Name[11];
	BYTE DIR_Attr;			// File attributes
	BYTE DIR_NTRes;			// Reserved for use by Windows NT. Set value to 0 when a file is created and never modify or look at it after that.
	BYTE DIR_CrtTimeTenth;	// Millisecond stamp at file creation time. This field actually contains a count of tenths of a second. The granularity of the seconds part of DIR_CrtTime is 2 seconds so this field is a count of tenths of a second and its valid value range is 0-199 inclusive.
	WORD DIR_CrtTime;		// Time file was created.
	WORD DIR_CrtDate;		// Date file was created.
	WORD DIR_LstAccDate;	// Last access date. Note that there is no last access time, only a date. This is the date of last read or write. In the case of a write, this should be set to the same date as DIR_WrtDate.
	WORD DIR_FstClusHI;		// High WORD of this entry’s first cluster number (always 0 for a FAT12 or FAT16 volume).
	WORD DIR_WrtTime;		// Time of last write. Note that file creation is considered a write.
	WORD DIR_WrtDate;		// Date of last write. Note that file creation is considered a write.
	WORD DIR_FstClusLO;		// Low WORD of this entry’s first cluster number.
	DWORD DIR_FileSize;		// 32-bit DWORD holding this file’s size in bytes.
}Dir_Entry;

/////////////////////////////////////////////////////////////////////

typedef struct _LFN_Entry
{
	BYTE LDIR_Ord;			// The order of this entry in the sequence of long dir 
							// entries associated with the short dir entry at the 
							// end of the long dir set.
							// If masked with 0x40 (LAST_LONG_ENTRY), this indicates 
							// the entry is the last long dir entry in a set of long 
							// dir entries. All valid sets of long dir entries must 
							// begin with an entry having this mask.
	WORD LDIR_Name1[5];		// Characters 1-5 of the long-name sub-component in this dir entry.
	BYTE LDIR_Attr;			// Attributes - must be ATTR_LONG_NAME
	BYTE LDIR_Type;			// If zero, indicates a directory entry that is a sub-component of 
							// a long name.  NOTE: Other values reserved for future extensions.
							// Non-zero implies other dirent types.
	BYTE LDIR_Chksum;		// Checksum of name in the short dir entry at the end of the long dir set.
	WORD LDIR_Name2[6];		// Characters 6-11 of the long-name sub-component in this dir entry.
	WORD LDIR_FstClusLO;	// Must be ZERO. This is an artifact of the FAT "first cluster" and 
							// must be zero for compatibility with existing disk utilities.  
							// It's meaningless in the context of a long dir entry.
	WORD LDIR_Name3[2];		// Characters 12-13 of the long-name sub-component in this dir entry.
}LFN_Entry;

#define FAT_NAME_LEN 256

#pragma pack()

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////

typedef enum _FATEnum {UNKNOWN, FAT12, FAT16, FAT32} FATEnum;

/////////////////////////////////////////////////////////////////////

typedef struct _FATInfo
{
	BootSector bs; // TODO: DMA this? not safe API as is? just a shallow copy
	DWORD FATSz;
	DWORD FAT32RootDirCluster;
	DWORD FAT16RootDirSector;
	DWORD FirstDataSector;
	DWORD TotSec;
	DWORD DataSec;
	DWORD CountofClusters;
	DWORD cbCluster;
	FATEnum FATType;
}FATInfo;

/////////////////////////////////////////////////////////////////////

typedef struct _FATItem
{
	TCHAR szName[MAX_PATH]; // TODO: Long Enough?
	WORD dateCreate;
	WORD timeCreate;
	WORD dateModify;
	WORD timeModify;
	WORD dateAccess;
	DWORD dwFirstCluster;
	DWORD dwSize;
	DWORD dwAttr;
	BOOL fDeleted;
}FATItem;

/////////////////////////////////////////////////////////////////////

#define ATTR_READ_ONLY  0x01
#define ATTR_HIDDEN 	0x02
#define ATTR_SYSTEM 	0x04
#define ATTR_VOLUME_ID 	0x08
#define ATTR_DIRECTORY	0x10
#define ATTR_ARCHIVE  	0x20
#define ATTR_LONG_NAME 		(ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)
#define ATTR_LONG_NAME_MASK	(ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID | ATTR_DIRECTORY | ATTR_ARCHIVE)
#define LAST_LONG_ENTRY 0x40

/////////////////////////////////////////////////////////////////////

DWORD FirstSectorOfCluster(DWORD n, FATInfo *pfi);
BOOL InitFATInfo(BootSector *pbs, FATInfo *pfi);

BOOL ReadCluster(HANDLE hFile, LARGE_INTEGER *plrgFileSecOffset, 
				 DWORD dwCluster,FATInfo *pfi,BYTE* pBuf,DWORD dwBufLen);

BOOL ReadEntireFileFromCluster(HANDLE hFile, LARGE_INTEGER *plrgFileSecOffset, 
							   FATInfo *pfi,DWORD dwCluster, BYTE* pBuf,DWORD dwBufLen);
DWORD TotalLinkedClustersForCluster(HANDLE hFile, LARGE_INTEGER *plrgFileSecOffset, 
									FATInfo *pfi, DWORD dwCluster);

BOOL ParseFATDirectory(FATInfo *pfi, BYTE *pDir, 
					   DWORD dwDirBufLen, FATItem *pItems, 
					   DWORD *pcItems);

BOOL IsEndOfFileCluster(DWORD dwCluster, FATInfo *pfi);
BOOL IsBadCluster(DWORD dwCluster, FATInfo *pfi);
BOOL IsFreeCluster(DWORD dwCluster, FATInfo *pfi);
BOOL FillFATItemStruct(LPTSTR pszName,Dir_Entry* pDir, FATItem *pItem);
void FormatAttributes(LPTSTR pszAttr, BYTE Attributes);
void FormatOldFilename(LPTSTR pszName, BYTE *pName);
int FormatLongFilename(LPTSTR pszName, Dir_Entry* pDir);

/////////////////////////////////////////////////////////////////////