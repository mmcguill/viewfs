// viewfsDlg.h : header file
//

#pragma once

#include <vector>
using namespace std;

/////////////////////////////////////////////////////////////////////

#pragma pack(1)

struct PartitionTableEntry
{
	BYTE state;
	BYTE begin_head;
	WORD begin_cylinder;
	BYTE type;
	BYTE end_head;
	WORD end_cylinder;
	DWORD cSecsMBRToFirstSector;
	DWORD cSectors;
};

/////////////////////////////////////////////////////////////////////

struct MasterBootRecord
{
	BYTE loader_code[0x1BE];
	PartitionTableEntry pte[4];
	WORD marker;
};

/////////////////////////////////////////////////////////////////////

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

typedef struct _NTFS_BPBEX
{
	DWORD	 unused1;
	LONGLONG TotalSectors;
	LONGLONG LogClusterMFT;
	LONGLONG LogClusterMFTMirr;
	DWORD	ClustersPerFileRecordSegment;
	DWORD	ClustersPerIndexBlock;
	LONGLONG VolSerial;
	DWORD	Checksum;
}NTFS_BPBEX,*PNTFS_BPBEX;

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
		NTFS_BPBEX	ntfs;
	}FS_Dep;
}BootSector;

#pragma pack()

/////////////////////////////////////////////////////////////////////

typedef struct _PartitionLoadInfo
{
	DWORD dwDisk;
	DWORD dwFS;
	TCHAR szDevicePath[MAX_PATH];
	ULARGE_INTEGER lrgFirstSector;
	ULARGE_INTEGER lrgSectors;
}PartitionLoadInfo;


/////////////////////////////////////////////////////////////////////

#define HARDCODED_REISER_SUPERBLOCK_START	0x10000
#define HARDCODED_REISER_SUPERBLOCK_SIZE	0x1000
#define HARDCODED_SUPERBLOCK_START  0x400
#define HARDCODED_SUPERBLOCK_SIZE	0x40
#define EXT2FS_MAGIC 0xef53

/////////////////////////////////////////////////////////////////////

// Enough Super Block to test for Ext2FS in an image

struct ext2_super_block {
	int	s_inodes_count;		/* Inodes count */
	int	s_blocks_count;		/* Blocks count */
	int	s_r_blocks_count;	/* Reserved blocks count */
	int	s_free_blocks_count;	/* Free blocks count */
	int	s_free_inodes_count;	/* Free inodes count */
	int	s_first_data_block;	/* First Data Block */
	int	s_log_block_size;	/* Block size */
	int	s_log_frag_size;	/* Fragment size */
	int	s_blocks_per_group;	/* # Blocks per group */
	int	s_frags_per_group;	/* # Fragments per group */
	int	s_inodes_per_group;	/* # Inodes per group */
	int	s_mtime;		/* Mount time */
	int	s_wtime;		/* Write time */
	short	s_mnt_count;		/* Mount count */
	short	s_max_mnt_count;	/* Maximal mount count */
	short	s_magic;		/* Magic signature */
};

/////////////////////////////////////////////////////////////////////

#define REISER_MAGIC "ReIsErFs"
#define REISER2_MAGIC "ReIsEr2Fs"
#define REISER3_MAGIC "ReIsEr3Fs" 

#define REISER_MAGIC_OK(pMagic)									\
				(	0 == stricmp(pMagic,REISER_MAGIC) ||		\
					0 == stricmp(pMagic,REISER2_MAGIC) ||		\
					0 == stricmp(pMagic,REISER3_MAGIC))

struct reiser_super_block {
	unsigned long	BlockCount;
	unsigned long	FreeBlocks;
	unsigned long	RootBlock;
	unsigned long	JournalBlock;
	unsigned long	JournalDevice;
	unsigned long	OrigJournalSize;
	unsigned long	JournalTransMax;
	unsigned long	JournalMagic;
	unsigned long	JournalMaxBatch;
	unsigned long	JournalMaxCommitAge;
	unsigned long	JournalMaxTransAge;
	unsigned short	BlockSize;
	unsigned short	OIDMaxSize;
	unsigned short	OIDCurrentSize;
	unsigned short	State;
	char			MagicString[12];
    unsigned long	HashFunctionCode;
	unsigned short	TreeHeight;
	unsigned short	BitmapNumber;
	unsigned short	Version;
	unsigned short	reserved;
	unsigned long	InodeGeneration;
};

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////

// CviewfsDlg dialog
class CviewfsDlg : public CDialog
{
// Construction
public:
	CviewfsDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	enum { IDD = IDD_VIEWFAT_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;
	vector <PartitionLoadInfo*> m_vecPartLoad;
	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()

	//BOOL ChangeDirectory(DWORD dwCluster);
	void FormatSizeEx(LPTSTR pszSize,ULARGE_INTEGER *psize);
	//BOOL CreateTmpViewOfItem(int nItem);

	void CleanupState();
	BOOL DoPhysicalDisks();
	BOOL SetupListView();
	BOOL DoLogicalDisks();

	//BOOL FillPartitionInfo(int nDisk,LPTSTR lpszDisk,LPTSTR pszDiskPath);
	BOOL FillPartitionInfo(HTREEITEM hPhys, int nDisk, 
							LPTSTR lpszDisk,LPTSTR pszDiskPath);

	BOOL RecAddPartition(HTREEITEM hParent,int nDisk,
								  LPTSTR lpszDisk,LPTSTR lpszDiskPath,int nPartition,
								  HANDLE hFile,PartitionTableEntry *lpte,
								  BOOL fPrimaryExtPartitionDone);

	void FormatPartitionState(LPTSTR lpszOut,DWORD dwLen,DWORD dwState);
	void FormatULarge(LPTSTR lpszOut,DWORD dwLen,PULARGE_INTEGER plrg);

	void GetPartitionTypeString(HANDLE hFile, LONGLONG *pllPartBeginOffset, 
								LPTSTR szType, DWORD dwLen,DWORD dwType);
	void RefreshDrives();
	void CleanupLogicalDrives();
	void CleanupPartitionList();

	BOOL LoadFileSystem(DWORD dwType, HANDLE hFile, LONGLONG llBeginFileOffset);

	HANDLE m_hFile;
	LARGE_INTEGER m_llBeginFileOffset;
	GUIFileSystemBaseHandler* m_pFSHandler;
	HMODULE m_hHandlerDll;

public:
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedOpendrive();
	afx_msg void OnBnClickedLoadimage();
	afx_msg void OnNMDblclkList1(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnSize(UINT nType, int cx, int cy);
protected:
	virtual void PostNcDestroy();
	void DisplaySelected();
public:
	afx_msg void OnBnClickedOpendisk();
	afx_msg void OnDestroy();
	afx_msg void OnBnClickedRefresh();
	afx_msg void OnBnClickedCdup();
//	afx_msg void OnNMClickTree(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnTvnSelchangedTree(NMHDR *pNMHDR, LRESULT *pResult);
	//afx_msg void OnNMReturnList1(NMHDR *pNMHDR, LRESULT *pResult);
	//afx_msg void OnLvnKeydownList1(NMHDR *pNMHDR, LRESULT *pResult);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg void OnHdnItemclickList1(NMHDR *pNMHDR, LRESULT *pResult);
};
