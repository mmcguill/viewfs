/////////////////////////////////////////////////////////////////////
//
// Original Filename: MMcGNTFS.h
// Author: Mark McGuill
// Create Date: 18/03/2005
//
// NTFS Parsing Stuff
//
/////////////////////////////////////////////////////////////////////

#pragma once

/////////////////////////////////////////////////////////////////////

#include <vector>
using namespace std;

/////////////////////////////////////////////////////////////////////

#ifdef MMCGNTFS_API_EXPORTS
#define MMCGNTFS_API __declspec(dllexport)
#else
#define MMCGNTFS_API __declspec(dllimport)
#endif

/////////////////////////////////////////////////////////////////////

namespace MMcGNTFS
{

/////////////////////////////////////////////////////////////////////

#pragma pack(1)

typedef struct _BPB
{
	WORD BytesPerSector;
	BYTE SectorsPerCluster;
	WORD ReservedSectors;
	BYTE unused1[3];
	WORD unused2;
	BYTE MediaDescriptor;
	WORD unused3;
	WORD SectorsPerTrack;
	WORD NumberOfHeads;
	DWORD HiddenSectors;
	DWORD unused4;
}BPB,*PBPB;

/////////////////////////////////////////////////////////////////////

typedef struct _BPBEX
{
	DWORD	 unused1;
	LONGLONG TotalSectors;
	LONGLONG LogClusterMFT;
	LONGLONG LogClusterMFTMirr;
	DWORD	ClustersPerFileRecordSegment;
	DWORD	ClustersPerIndexBlock;
	LONGLONG VolSerial;
	DWORD	Checksum;
}BPBEX,*PBPBEX;

/////////////////////////////////////////////////////////////////////

typedef struct _BOOT_SECTOR
{
	BYTE Jump[3];
	LONGLONG OEMID;
	BPB bpb;
	BPBEX bpbex;
	BYTE Bootstrap[426];
	WORD EOSMarker;
}BOOT_SECTOR, *PBOOT_SECTOR;

/////////////////////////////////////////////////////////////////////

	//BYTE sig[4];			// Looks like ascii "FILE"
	//BYTE unknown1[16];	// TODO: Figure out these
	//WORD offsetFirstAttr;	// From Start of this entry
	//BYTE unknown2[2];		// ??
	//DWORD used_size;		// How much of this entry is actually used
	//DWORD entry_size;		// entry size in MFT i'm guessing (always 0x400, ie 1Kb)

typedef struct _MFT_FILE_ENTRY
{
	DWORD  	   	Magic;
	WORD  	   	OffsetUpdateSeq;
	WORD 	  	SizeUpdateSeq;
	LONGLONG 	LSN;
	WORD 	  	SeqNumber;
	WORD 	  	HardlinkCount;
	WORD 	  	offsetFirstAttr;
	WORD 	  	Flags;
	DWORD 	  	used_size;
	DWORD 	  	entry_size;
	LONGLONG 	FileRefBaseFile;
	WORD 	  	NextAttributeId;
}MFT_FILE_ENTRY,*PMFT_FILE_ENTRY;

#define HARDCODED_MFT_ENTRY_SIZE	0x400
#define MFT_ENTRY_MAGIC				0x454c4946

/////////////////////////////////////////////////////////////////////

// 4 different types - try to account for this using unions
// Resident, Non named
// Resident, named
// Non-Resident, Non name
// Non-Resident, named

typedef struct _ATTR_HEADER
{
	DWORD Type;
	WORD Length;	// Includes header => Linux docs as DWORD but ive found its 
					// only word or looks that way
	WORD padding;
	BYTE fNonResident;
	BYTE NameLength;
	WORD OffsetName;
	WORD Flags;
	WORD AttrId;
	union
	{
		struct
		{
			DWORD AttrLength;
			WORD OffsetAttr;
			BYTE fIndexed;
			BYTE pad;
		}resident;
		struct
		{
			ULONGLONG StartVCN;
			ULONGLONG LastVCN;
			WORD OffsetDataRuns;
			WORD CompressionUnitSize;
			BYTE pad[4];
			ULONGLONG AllocSize;
			ULONGLONG RealSize;
			ULONGLONG InitDataSize;
		}non_resident;
	}extra;
}ATTR_HEADER,*PATTR_HEADER;

#define SIZEOF_ATTR_HEADER_RESIDENT 24
#define SIZEOF_ATTR_HEADER_NON_RESIDENT 64

// Attribute should begin after this if non-named
// if named, the name will follow (in UNICODE) followed
// by the attribute

/////////////////////////////////////////////////////////////////////

typedef struct _ATTR_STANDARD_INFORMATION
{
	LONGLONG CTime;
	LONGLONG ATime;
	LONGLONG MTime;
	LONGLONG RTime;
	DWORD DOSPermissions;
	DWORD MaxVersions;
	DWORD Version;
	DWORD ClassId;
	DWORD OwnerId;				// Win2K+
	DWORD SecurityId;			// Win2K+
	DWORD QuotaCharged;			// Win2K+
	DWORD UpdateSequenceNumber;	// Win2K+
}ATTR_STANDARD_INFORMATION,*PATTR_STANDARD_INFORMATION;

#define SIZEOF_ATTR_SI_SMALL 48 // Old Stylee
#define SIZEOF_ATTR_SI_LARGE 64 // 2k

/////////////////////////////////////////////////////////////////////

typedef struct _ATTR_FILE_NAME
{
	LONGLONG RefParent;
	FILETIME CTime;
	FILETIME ATime;
	FILETIME MTime;
	FILETIME RTime;
	LONGLONG AllocSize;
	LONGLONG RealSize;
	DWORD 	Flags;
	DWORD 	unused; //Used by EAs and Reparse
	BYTE 	cchFilename; 
	BYTE 	FilenameNamespace;
	//WCHAR   Filename[]; // FILENAME here
}ATTR_FILE_NAME,*PATTR_FILE_NAME;

#define FILE_NAME_LEN 256
#define SIZEOF_ATTR_FILE_NAME (66)
#define MAX_SIZEOF_ATTR_FILE_NAME (66 + (FILE_NAME_LEN * 2))

enum
{
	FILE_NAMESPACE_POSIX			= 0x00,
	FILE_NAMESPACE_WIN32			= 0x01,
	FILE_NAMESPACE_DOS				= 0x02,
	FILE_NAMESPACE_WIN32_AND_DOS	= 0x03,
};

/////////////////////////////////////////////////////////////////////

typedef struct _INDEX_HEADER
{
	DWORD	OffsetFirstIndexEntry;
	DWORD	TotalSizeIndexEntries;
	DWORD	AllocSizeIndexEntries;
	BYTE	Flags;
	BYTE	Padding[3];
}INDEX_HEADER, *PINDEX_HEADER;

enum 
{
	INDEX_HEADER_FLAG_SMALL = 0x00,
	INDEX_HEADER_FLAG_LARGE = 0x01,
};

/////////////////////////////////////////////////////////////////////

typedef struct _ATTR_INDEX_ROOT
{
	DWORD  	Type;
	DWORD 	CollationRule;
	DWORD 	SizeIndexAllocationEntry;
	BYTE 	ClustersPerIndexRecord;
	BYTE	Padding[3];
	INDEX_HEADER Header;
}ATTR_INDEX_ROOT,*PATTR_INDEX_ROOT;

/////////////////////////////////////////////////////////////////////

typedef struct _ATTR_INDEX_ALLOCATION
{
	DWORD  				magic; // 'INDX'
	WORD 				offsetUpdSeq;
	WORD 				SizeUpdSeq;
	LONGLONG 			logSeq;
	LONGLONG 			VCN;
	INDEX_HEADER		Header;
}ATTR_INDEX_ALLOCATION,*PATTR_INDEX_ALLOCATION;

/////////////////////////////////////////////////////////////////////

// TODO: this is slightly different for views or 
// non directory indexes

typedef struct _INDEX_ENTRY
{
	LONGLONG	MFTRef;
	WORD 		Size;
	WORD 		SizeAttr;
	WORD 		IndexFlags;
	BYTE		Padding[2];
}INDEX_ENTRY,*PINDEX_ENTRY;

#define SIZEOF_INDEX_ENTRY 16

// Attribute follows

enum 
{
	INDEX_ENTRY_PARENT = 0x0001,	// Parent of others
	INDEX_ENTRY_LAST = 0x0002,	// Last Entry
};

/////////////////////////////////////////////////////////////////////

// NTFS_ATTRIBUTE_LIST is just a sequence of these...

typedef struct _ATTRIBUTE_LIST_ENTRY
{
	DWORD Type;
	WORD RecordLength;
	BYTE NameLength;
	BYTE OffsetName;
	LONGLONG StartingVCN;
	LONGLONG BaseFileRef;
	WORD AttributeId;
}ATTRIBUTE_LIST_ENTRY,*PATTRIBUTE_LIST_ENTRY;

// Name Follows (UNICODE)

#define SIZEOF_ATTRIBUTE_LIST_ENTRY 26
/////////////////////////////////////////////////////////////////////

#define FILE_ENTRY_FROM_FILEREF(x)   (x & 0x0000FFFFFFFFFFFF)
#define FILE_SEQ_NO_FROM_FILEREF(x)  (x & 0xFFFF000000000000)

/////////////////////////////////////////////////////////////////////

#pragma pack()

/////////////////////////////////////////////////////////////////////

enum 
{
	ERR_NO_ERROR,
	ERR_OUT_OF_MEMORY,
	ERR_CALL_CLOSE_FIRST,
	ERR_OPENING_FILE,
	ERR_READING_FILE,
	ERR_INVALID_PARAM,
	ERR_ATTR_NOT_AVAILABLE,
	ERR_BUFFER_TOO_SMALL,
	ERR_CANNOT_PARSE,
	ERR_INVALID_MFT_ENTRY,
	ERR_NOT_INIT,
	ERR_INITIALIZING,
	ERR_CORRUPT_ATTRIBUTE,
	ERR_CREATE_FILE,
	ERR_WRITING_FILE,
	ERR_FILE_ALREADY_EXISTS,
};

/////////////////////////////////////////////////////////////////////

enum 
{
	ATTR_TYPE_STANDARD_INFORMATION =	0x10,
	ATTR_TYPE_ATTRIBUTE_LIST		 =	0x20,
	ATTR_TYPE_FILE_NAME				=	0x30,
	ATTR_TYPE_VOLUME_VERSION		 = 	0x40,
	ATTR_TYPE_OBJECT_ID				 = 	0x40,	
	ATTR_TYPE_SECURITY_DESCRIPTOR  = 	0x50, 	
	ATTR_TYPE_VOLUME_NAME			 = 	0x60,	
	ATTR_TYPE_VOLUME_INFORMATION	 = 	0x70, 	
	ATTR_TYPE_DATA					 = 	0x80,	
	ATTR_TYPE_INDEX_ROOT			 = 	0x90,	
	ATTR_TYPE_INDEX_ALLOCATION		 = 	0xA0,	
	ATTR_TYPE_BITMAP				 = 	0xB0,	
	ATTR_TYPE_SYMBOLIC_LINK			 = 	0xC0,	
	ATTR_TYPE_REPARSE_POINT			 = 	0xC0,	
	ATTR_TYPE_EA_INFORMATION		 = 	0xD0,	
	ATTR_TYPE_EA					 = 	0xE0,	
	ATTR_TYPE_PROPERTY_SET			 = 	0xF0,	
	ATTR_TYPE_LOGGED_UTILITY_STREAM	 = 0x100,
};

/////////////////////////////////////////////////////////////////////

enum
{
	CORE_FILE_MFT,
	CORE_FILE_MFTMIRR,
	CORE_FILE_LOGFILE,
	CORE_FILE_VOLUME,
	CORE_FILE_ATTRDEF,
	CORE_FILE_ROOTDIR,
	CORE_FILE_BITMAP,
	CORE_FILE_BOOT,
	CORE_FILE_BADCLUS,
	CORE_FILE_QUOTA,
	CORE_FILE_SECURE,
	CORE_FILE_UPCASE,
	CORE_FILE_EXTEND,
};

/////////////////////////////////////////////////////////////////////

enum
{
	FILE_ATTR_READ_ONLY				=0x0001,
	FILE_ATTR_HIDDEN				=0x0002,
	FILE_ATTR_SYSTEM				=0x0004,
	FILE_ATTR_ARCHIVE				=0x0020,
	FILE_ATTR_DEVICE				=0x0040,
	FILE_ATTR_NORMAL				=0x0080,
	FILE_ATTR_TEMPORARY				=0x0100,
	FILE_ATTR_SPARSE_FILE			=0x0200,
	FILE_ATTR_REPARSE_POINT			=0x0400,
	FILE_ATTR_COMPRESSED			=0x0800,
	FILE_ATTR_OFFLINE				=0x1000,
	FILE_ATTR_NOT_CONTENT_INDEXED	=0x2000,
	FILE_ATTR_ENCRYPTED				=0x4000,
	FILE_ATTR_DIRECTORY				=0x10000000,
	FILE_ATTR_INDEX_VIEW			=0x20000000,
};

#define REGULAR_FILE_MASK 0x000029A7 // {A,N,H,RO,S,NCI,TMP,C}

#define IS_DIRECTORY(attr) \
			(FILE_ATTR_DIRECTORY == \
			(FILE_ATTR_DIRECTORY & (attr)))

#define IS_COMPRESSED_FILE(attr) \
			(FILE_ATTR_COMPRESSED == \
			(FILE_ATTR_COMPRESSED & (attr)))

#define IS_REGULAR_FILE(attr)							\
			(0 == (attr) ||								\
				((REGULAR_FILE_MASK  & (attr)) &&		\
				(!(~REGULAR_FILE_MASK  & (attr)))))		\
			

/////////////////////////////////////////////////////////////////////

class Attribute
{
public:
	Attribute()
	{
		m_dwType = 0;
		m_lpszName = NULL;
	}

	~Attribute()
	{
		if(m_lpszName)
		{
			delete m_lpszName;
			m_lpszName = NULL;
		}
	}

	// NOTE:	lpsName does not have to be null-termed, makes things easy
	//			for me

	bool Init(DWORD dwType, LPWSTR lpsName, DWORD cchName)
	{
		m_dwType = dwType;

		if(m_lpszName)
		{
			delete m_lpszName;
		}
		m_lpszName = NULL;

		if(cchName && lpsName)
		{
			m_lpszName = new WCHAR[cchName+1];
			//wcsncpy_s(m_lpszName,cchName,lpsName,_TRUNCATE);
			wcsncpy(m_lpszName,lpsName,cchName);
			m_lpszName[cchName] = 0;
		}

		return true;
	}

	LPWSTR GetName() {return m_lpszName;}
	DWORD GetType() {return m_dwType;}

private:
	DWORD m_dwType;
	LPWSTR m_lpszName;
};

typedef Attribute ATTRIBUTE;
typedef ATTRIBUTE* PATTRIBUTE;

/////////////////////////////////////////////////////////////////////

struct DataRun
{
	_U64 length;
	_I64 offset;
	bool fSparse;
};

/////////////////////////////////////////////////////////////////////

class  MMCGNTFS_API DataRunHelper
{
public:
	DataRunHelper();

	~DataRunHelper();

	void Clear();
	
	bool Init(HANDLE hFile, LONGLONG llBeginOffset, DWORD cbCluster);

	bool SetAttributeHeader(PATTR_HEADER pah);

	bool GetData(	LONGLONG llOffsetFrom,LONGLONG cbReq,BYTE *pBuf,
					PLONGLONG pBufLen);

	bool GetAllDataToMem(BYTE *pBuf, _U64 *pcbBuf, bool fCompressed);

	bool GetAllDataToFile(HANDLE hWrite, bool fCompressed); 

	DWORD GetLastError() {return m_dwLastError;}

protected:
	bool GetAllData(HANDLE hWrite, BYTE* pBuf, _U64 *pcbBuf, 
					bool fCompressed, bool fFile);

	bool AddRawRun(BYTE* pRun, DWORD dwLen);

	bool DecompressLZChunk(	_U8* pCompBuf, DWORD cbCompBuf, 
							HANDLE hWrite, BYTE *pBuf, 
							_U64 *pcbBuf, bool fFile);

	bool GetSingleUncompressedRun(	HANDLE hWrite, BYTE *pBuf, 
									_U64 *pcbBuf, int cRun, 
									bool fFile);

	bool VCNToLCN(_U64 VCN, _U64 *pLCN, bool *pfIsSparse);

private:
	DataRun*		m_pRuns;
	int				m_cRuns;
	HANDLE			m_hFile;
	LONGLONG		m_llBeginOffset;
	DWORD			m_cbCluster;
	_U8*			m_pDecompBuf;
	_U8*			m_pCompBuf;
	bool			m_fInit;
	PATTR_HEADER	m_pah;
	DWORD			m_dwLastError;
};

/////////////////////////////////////////////////////////////////////

//class MMCGNTFS_API DataHelper
//{
//public:
//	DataHelper();
//				
//	bool Init(	HANDLE hFile,LONGLONG llBeginFileOffset,
//				DWORD cbCluster,PATTR_HEADER pah);
//
//	bool GetAllData(BYTE* pBuf, PULONGLONG pcbBuf);
//
//	bool GetAllDataToFile(HANDLE hWrite, bool fCompressed);
//
//	DWORD GetLastError() {return m_dwLastError;}
//
//private:
//	PATTR_HEADER m_pah;
//	HANDLE m_hFile;
//	LONGLONG m_llBeginFileOffset;
//	DWORD m_cbCluster;
//	bool m_fInit;
//	DWORD m_dwLastError;
//};

/////////////////////////////////////////////////////////////////////

struct GetAttrHdrs_Ret
{
	GetAttrHdrs_Ret(LONGLONG _MFTEntry, int _offsetAttrHdr)
	{
		MFTEntry = _MFTEntry;
		offsetAttrHdr = _offsetAttrHdr;
	}

	LONGLONG MFTEntry;
	int offsetAttrHdr;
};

/////////////////////////////////////////////////////////////////////

#define DIRECTORY_STREAM_NAME L"$I30"

/////////////////////////////////////////////////////////////////////

typedef struct _NTFSItem
{
	WCHAR szName[FILE_NAME_LEN];
	LONGLONG mftIndex;
	LONGLONG size;
	FILETIME ftCreate;
	FILETIME ftMod;
	FILETIME ftAccess;
	DWORD dwAttr;
	bool fDeleted;
	DWORD FilenameNamespace;
}NTFSItem;

/////////////////////////////////////////////////////////////////////
// Class To Read NTFS From Disk or Image

class MMCGNTFS_API NTFSReader
{
public:
	NTFSReader();
	~NTFSReader();

	// Initialise NTFSReader

	bool Init(HANDLE hFile, LONGLONG llBeginFileOffset);

	// Close NTFSReader

	bool Close();

	// Returns last error encountered by NTFSReader

	DWORD GetLastError();

	bool ReadDirectory(LONGLONG llFile, NTFSItem* pFiles, PLONGLONG pcFiles);

	bool ReadItemDataToFile(LONGLONG llFile, LPTSTR lpszPath, bool fCompressed);

	//bool GetParentMFTEntry(LONGLONG llFile, PLONGLONG pllParent);

protected:
	
	bool LoadMFTEntry(LONGLONG llMFTEntry, BYTE* pBuf);

	// Gets Attribute Data
	
	bool GetAttribute(	LONGLONG llFile, DWORD dwAttr, LPWSTR lpszName, 
						BYTE* pBuf, PULONGLONG pcbBuf,PULONGLONG pcbRealSize);

	// Gets Attribute Data to File

	bool GetAttributeToFile(LONGLONG llFile, DWORD dwAttr, 
							LPWSTR lpszName, HANDLE hWrite, bool fCompressed);

	
	bool GetAttributeHeaders(LONGLONG llFile, DWORD dwAttr, LPWSTR lpszName, 
							vector<GetAttrHdrs_Ret*> *pvecRet);

	
	//bool GetFilenameAttr(LONGLONG llFile, DWORD dwNamespace, BYTE* pFN, LONGLONG cbFN);

	
	// Initialize & load $MFT and its data run helper

	bool InitMFT();

	
	// Gets MFT Entry for a file upto max 1Kb (0x400)

	bool GetMFTFileEntry(LONGLONG llFile, BYTE *pBuf, DWORD dwBufLen);



	bool RecReadDir(PINDEX_ENTRY pie, BYTE* pAlloc,DWORD cbAlloc, BYTE* pBitmap, 
					DWORD cbBitmap, NTFSItem* pFiles,PLONGLONG pcFiles);

	// Private Helper Function For GetAttribute()

	bool AttrHdrMatch(PATTR_HEADER pah, DWORD dwAttr, LPWSTR lpszName);

	
	// Private Helper Function For GetAttribute()

	bool AttrListEntryMatch(PATTRIBUTE_LIST_ENTRY pale,DWORD dwAttr, LPWSTR lpszName);

protected:
	bool			m_fInit; //TODO: All functions should check this!!
	HANDLE			m_hFile;
	LONGLONG		m_llBeginFileOffset;
	DWORD			m_dwLastError;
	DataRunHelper   m_drhMFT;
	DataRunHelper   m_drhFile;

	// Metrics

	DWORD			m_cbSector;
	DWORD			m_cbCluster;
	DWORD			m_cbIndexBlock;
	LONGLONG		m_llLCNMFT;		
	LONGLONG		m_llLCNMFTMirr;
	LONGLONG		m_llTotalSectors;
	
	// Perf

	BYTE *m_pGetAttributeMemCache;
	BYTE *m_pGetMFTEntryCache;
	LONGLONG m_llGetAttributeLastFile;
	LONGLONG m_llGetMFTEntryLastFile;
private:
};

/////////////////////////////////////////////////////////////////////

} // namespace MMcGNTFS

/////////////////////////////////////////////////////////////////////