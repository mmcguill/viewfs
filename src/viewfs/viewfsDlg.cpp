/////////////////////////////////////////////////////////////////////
//
//
//
//
//
/////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "viewfs.h"
#include ".\viewfsdlg.h"
#include <setupapi.h>
#include <winioctl.h>

/////////////////////////////////////////////////////////////////////

#define PARAM_LOGICAL_DRIVE (-1)
#define PARAM_LOAD_IMAGE (-2)
#define PARAM_BROWSE_FOR_IMAGE (-3)
#define PARAM_PHYSICAL_DRIVE (-4)

/////////////////////////////////////////////////////////////////////

typedef struct _partition_type_entry
{
	DWORD	dwType;
	LPCTSTR lpszType;
	LPCTSTR lpszGUIHandlerDll; // This Dll must export GetGUIHandler();
}partition_type_entry;

/////////////////////////////////////////////////////////////////////

static const partition_type_entry partition_type_map[] = 
{
	{0x00,_T("empty partition-table entry"),				NULL},
	{0x01,_T("FAT"),										NULL},
	{0x02,_T("XENIX root file system"),						NULL},
	{0x03,_T("XENIX /usr file system (obsolete)"),			NULL},
	{0x04,_T("FAT"),										NULL},
	{0x05,_T("Extended"),									NULL},
	{0x06,_T("FAT"),							_T("MMcGFAT.DLL")},
	{0x07,_T("NTFS"),							_T("MMcGNTFS.DLL")},
	{0x08,_T("OS/2 (v1.0-1.3 only)"),						NULL},
	{0x09,_T("AIX data partition"),							NULL},
	{0x0A,_T("OS/2 Boot Manager"),							NULL},
	{0x0B,_T("FAT"),							_T("MMcGFAT.DLL")},
	{0x0C,_T("FAT"),							_T("MMcGFAT.DLL")},
	{0x0E,_T("FAT"),										NULL},
	{0x0F,_T("Extended"),									NULL},
	{0x10,_T("OPUS"),										NULL},
	{0x11,_T("OS/2 Boot Manager hidden 12-bit FAT partition"),NULL},
	{0x12,_T("Compaq Diagnostics partition"),NULL},
	{0x14,_T("OS/2 Boot Manager hidden sub-32M 16-bit FAT partition"),NULL},
	{0x16,_T("OS/2 Boot Manager hidden over-32M 16-bit FAT partition"),NULL},
	{0x17,_T("hidden NTFS partition"),NULL},
	{0x18,_T("AST special Windows swap file (\"Zero-Volt Suspend\" partition)"),NULL},
	{0x19,_T("Willowtech Photon coS"),NULL},
	{0x1B,_T("hidden Windows95 FAT32 partition"),NULL},
	{0x1C,_T("hidden Windows95 FAT32 partition (using LBA-mode INT 13 extensions)"),NULL},
	{0x1E,_T("hidden LBA VFAT partition"),NULL},
	{0x20,_T("Willowsoft Overture File System (OFS1)"),NULL},
	{0x21,_T("FSo2"),NULL},
	{0x24,_T("NEC MS-DOS 3.x"),NULL},
	{0x38,_T("Theos"),NULL},
	{0x3C,_T("PowerQuest PartitionMagic recovery partition"),NULL},
	{0x40,_T("VENIX 80286"),NULL},
	{0x41,_T("PowerPC boot partition"),NULL},
	{0x42,_T("SFS (Secure File System) by Peter Gutmann"),NULL},
	{0x45,_T("EUMEL/Elan"),NULL},
	{0x46,_T("EUMEL/Elan"),NULL},
	{0x47,_T("EUMEL/Elan"),NULL},
	{0x48,_T("EUMEL/Elan"),NULL},
	{0x4F,_T("Oberon boot/data partition"),NULL},
	{0x50,_T("OnTrack Disk Manager, read-only partition"),NULL},
	{0x51,_T("OnTrack Disk Manager, read/write partition"),NULL},
	{0x52,_T("CP/M"),NULL},
	{0x53,_T("OnTrack Disk Manager, write-only partition???"),NULL},
	{0x54,_T("OnTrack Disk Manager (DDO)"),NULL},
	{0x55,_T("EZ-Drive (see also INT 13/AH=FFh\"EZ-Drive\")"),NULL},
	{0x56,_T("GoldenBow VFeature"),NULL},
	{0x5C,_T("Priam EDISK"),NULL},
	{0x61,_T("SpeedStor"),NULL},
	{0x63,_T("Unix SysV/386, 386/ix / GNU HURD"),NULL},
	{0x64,_T("Novell NetWare 286"),NULL},
	{0x65,_T("Novell NetWare (3.11)"),NULL},
	{0x67,_T("Novell"),NULL},
	{0x68,_T("Novell"),NULL},
	{0x69,_T("Novell"),NULL},
	{0x70,_T("DiskSecure Multi-Boot"),NULL},
	{0x75,_T("PC/IX"),NULL},
	{0x7E,_T("F.I.X."),NULL},
	{0x80,_T("Minix v1.1 - 1.4a"),NULL},
	{0x81,_T("Linux"),NULL},
	{0x82,_T("Linux Swap"),NULL},
	{0x83,_T("Linux"),					_T("<LINUX_SPECIAL>")}, //_T("MMcGExt2FS.DLL")},// _T("MMcGReiser.DLL")}, // _T("MMcGExt2FS.DLL")},
	{0x85,_T("Linux EXT"),NULL},
	{0x86,_T("FAT16 volume/stripe set (Windows NT)"),NULL},
	{0x87,_T("NTFS volume/stripe set / HPFS Fault-Tolerant mirrored partition"),NULL},
	{0x93,_T("Amoeba file system"),NULL},
	{0x94,_T("Amoeba bad block table"),NULL},
	{0x98,_T("Datalight ROM-DOS SuperBoot"),NULL},
	{0x99,_T("Mylex EISA SCSI"),NULL},
	{0xA0,_T("Phoenix NoteBIOS Power Management \"Save-to-Disk\" partition"),NULL},
	{0xA5,_T("FreeBSD, BSD/386"),NULL},
	{0xA6,_T("OpenBSD"),NULL},
	{0xA9,_T("NetBSD (http://www.netbsd.org/)"),NULL},
	{0xB6,_T("Windows NT mirror set (master), FAT16 file system"),NULL},
	{0xB7,_T("Windows NT mirror set (master), NTFS file system"),NULL},
	{0xB8,_T("BSDI swap partition (secondarily file system)"),NULL},
	{0xBE,_T("Solaris boot partition"),NULL},
	{0xC0,_T("DR DOS/DR-DOS/Novell DOS secured partition"),NULL},
	{0xC1,_T("DR DOS 6.0 LOGIN.EXE-secured 12-bit FAT partition"),NULL},
	{0xC4,_T("DR DOS 6.0 LOGIN.EXE-secured 16-bit FAT partition"),NULL},
	{0xC6,_T("Windows NT mirror set (slave), FAT16 file system"),NULL},
	{0xC7,_T("Windows NT mirror set (slave), NTFS file system"),NULL},
	{0xCB,_T("Reserved for DR DOS/DR-DOS/OpenDOS secured FAT32"),NULL},
	{0xCC,_T("Reserved for DR DOS/DR-DOS secured FAT32 (LBA)"),NULL},
	{0xCE,_T("Reserved for DR DOS/DR-DOS secured FAT16 (LBA)"),NULL},
	{0xD0,_T("Multiuser DOS secured FAT12"),NULL},
	{0xD1,_T("Old Multiuser DOS secured FAT12"),NULL},
	{0xD4,_T("Old Multiuser DOS secured FAT16 (<= 32M)"),NULL},
	{0xD5,_T("Old Multiuser DOS secured extended partition"),NULL},
	{0xD6,_T("Old Multiuser DOS secured FAT16 (> 32M)"),NULL},
	{0xD8,_T("CP/M-86"),NULL},
	{0xDB,_T("CP/M, Concurrent CP/M, Concurrent DOS"),NULL},
	{0xE1,_T("SpeedStor 12-bit FAT extended partition"),NULL},
	{0xE3,_T("DOS read-only"),NULL},
	{0xE4,_T("SpeedStor 16-bit FAT extended partition"),NULL},
	{0xEB,_T("BeOS BFS (BFS1)"),NULL},
	{0xF1,_T("Storage Dimensions"),NULL},
	{0xF2,_T("DOS 3.3+ secondary partition"),NULL},
	{0xF4,_T("SpeedStor"),NULL},
	{0xF5,_T("Prologue"),NULL},
	{0xFE,_T("LANstep"),NULL},
	{0xFF,_T("Xenix bad block table"),NULL},
};

#define NUMBER_OF_PARTITION_TYPES \
	(sizeof(partition_type_map) / sizeof(partition_type_entry))

/////////////////////////////////////////////////////////////////////

// TODO: Sector Size???	

#define HARDCODED_SECTOR_SIZE 512

/////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
public:
};

/////////////////////////////////////////////////////////////////////

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

/////////////////////////////////////////////////////////////////////

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

/////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////

CviewfsDlg::CviewfsDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CviewfsDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_hFile = NULL;
	m_llBeginFileOffset.QuadPart = 0;
	m_pFSHandler = NULL;
	m_hHandlerDll = NULL;
}

/////////////////////////////////////////////////////////////////////

void CviewfsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

/////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CviewfsDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDOK, OnBnClickedOk)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST1, OnNMDblclkList1)
	ON_WM_DESTROY()
	ON_BN_CLICKED(IDC_REFRESH, OnBnClickedRefresh)
	ON_BN_CLICKED(IDC_CDUP, OnBnClickedCdup)
	ON_WM_SIZE()
	ON_NOTIFY(TVN_SELCHANGED, IDC_TREE, OnTvnSelchangedTree)
	ON_NOTIFY(HDN_ITEMCLICK, 0, OnHdnItemclickList1)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////

BOOL CviewfsDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// List View 

	if(!SetupListView())
	{
		AfxMessageBox(_T("Error Setting Up List View..."));
	}

	// Physical Disks

	if(!DoPhysicalDisks())
	{
		AfxMessageBox(_T("Error Doing Physical Disks Combo..."));
	}

	// Logical Drives 

	CTreeCtrl *pTree = (CTreeCtrl*)GetDlgItem(IDC_TREE);
	HTREEITEM hImg = pTree->InsertItem(LVIF_TEXT | LVIF_PARAM,_T("Images"),0,0,0,0,
		PARAM_LOAD_IMAGE,TVI_ROOT,TVI_LAST);

	pTree->InsertItem(LVIF_TEXT | LVIF_PARAM,_T("Load Image..."),0,0,0,0,
		PARAM_BROWSE_FOR_IMAGE,hImg,TVI_LAST);


	if(!DoLogicalDisks())
	{
		AfxMessageBox(_T("Error Doing Logical Disks Combo..."));
	}

	return TRUE;  // return TRUE  unless you set the focus to a control
}

/////////////////////////////////////////////////////////////////////

BOOL CviewfsDlg::DoLogicalDisks()
{
	CTreeCtrl *pTree = (CTreeCtrl*)GetDlgItem(IDC_TREE);
	HTREEITEM hLog = 
		pTree->InsertItem(LVIF_TEXT | LVIF_PARAM,_T("Logical Drives"),
					0,0,0,0,PARAM_LOGICAL_DRIVE,TVI_ROOT,TVI_LAST);

	//CComboBox *pCmb = (CComboBox*)GetDlgItem(IDC_CMBDRIVES);
	
	int nSize = GetLogicalDriveStrings(0,NULL);
	if(0 == nSize)
	{
		return FALSE;
	}

	LPTSTR pszDrives = new TCHAR[nSize];
	if(	NULL == pszDrives || 
		0 == (nSize = GetLogicalDriveStrings(nSize,pszDrives)))
	{
		return FALSE;
	}

	int i=0;
	int j=0;
	TCHAR szTmp[32];

	while(pszDrives[i] != 0)
	{
		_tcscpy(szTmp,&pszDrives[i]);
		int nLen = (int)_tcslen(szTmp);

		// Remove trailing backslash

		szTmp[nLen-1] = 0;
		//pCmb->AddString(szTmp);
		pTree->InsertItem(LVIF_TEXT | LVIF_PARAM,szTmp,0,0,0,0,
			0,hLog,TVI_LAST);

		i+=nLen+1;
	}

	delete pszDrives;
	return TRUE;
}

/////////////////////////////////////////////////////////////////////

BOOL CviewfsDlg::SetupListView()
{
	//CListCtrl *pLC = (CListCtrl*)GetDlgItem(IDC_LIST1);

	//// Partition List

	//pLC = (CListCtrl*)GetDlgItem(IDC_PART_LIST);

	//pLC->InsertColumn(0,"Desc",LVCFMT_LEFT,100);
	//pLC->InsertColumn(1,"Physical Disk",LVCFMT_LEFT,150);
	//pLC->InsertColumn(2,"Partition No.",LVCFMT_LEFT,20);
	//pLC->InsertColumn(3,"File System",LVCFMT_LEFT,75);
	//pLC->InsertColumn(4,"State",LVCFMT_LEFT,75);
	//pLC->InsertColumn(5,"Size",LVCFMT_LEFT,75);
	//pLC->InsertColumn(6,"First Sector",LVCFMT_LEFT,75);
	//pLC->InsertColumn(7,"Sector Count",LVCFMT_LEFT,75);
	//pLC->SetExtendedStyle(pLC->GetExtendedStyle() | LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);

	return TRUE;
}

/////////////////////////////////////////////////////////////////////

BOOL CviewfsDlg::DoPhysicalDisks()
{
	CTreeCtrl *pTree = (CTreeCtrl*)GetDlgItem(IDC_TREE);
	HTREEITEM hPhys = 
		pTree->InsertItem(LVIF_TEXT | LVIF_PARAM,_T("Physical Disks"),
					0,0,0,0,PARAM_PHYSICAL_DRIVE,TVI_ROOT,TVI_LAST);


	BOOL fRet = TRUE;

	GUID *guidDev = (GUID*) &GUID_DEVINTERFACE_DISK;
	
	HDEVINFO hDevInfo = 
		SetupDiGetClassDevs( guidDev,NULL,NULL,
		DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

	if(hDevInfo == INVALID_HANDLE_VALUE) 
	{
		return FALSE;
	}

	// Enumerate 

	SP_DEVICE_INTERFACE_DATA ifcData;
	ifcData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	int k=0;
	while(1)
	{
		if(!SetupDiEnumDeviceInterfaces(hDevInfo, NULL, guidDev, k, &ifcData))
		{
			DWORD err = GetLastError();
			if (err == ERROR_NO_MORE_ITEMS) 
			{
				break;
			}
			else
			{
				fRet = FALSE;
				break;
			}
		}


		// Need to find size of detailed data...

		SP_DEVINFO_DATA devdata;
		devdata.cbSize = sizeof(SP_DEVINFO_DATA);
		
		DWORD dwDetDataSize;
		SetupDiGetDeviceInterfaceDetail(hDevInfo,&ifcData, NULL,0,&dwDetDataSize,&devdata);

		
		// Should set this... if not trouble

		if(ERROR_INSUFFICIENT_BUFFER != GetLastError())
		{
			fRet = FALSE;
			break;
		}
		
		SP_DEVICE_INTERFACE_DETAIL_DATA *pDetData =
			(SP_DEVICE_INTERFACE_DETAIL_DATA*) new BYTE[dwDetDataSize];

		pDetData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
		if(NULL == pDetData)
		{
			fRet = FALSE;
			break;
		}

		// Get Detailed Info

        if(!SetupDiGetDeviceInterfaceDetail(hDevInfo,
				&ifcData, pDetData, dwDetDataSize, NULL, &devdata))
		{
		    delete [] (char*)pDetData;
			fRet = FALSE;
			break;
		}

        // Got a path to the device. Try to get some more info.

		TCHAR fname[256];
		if( !SetupDiGetDeviceRegistryProperty(hDevInfo, &devdata, 
			 SPDRP_FRIENDLYNAME, NULL,(PBYTE)fname, sizeof(fname),0))
		{
		    delete [] (char*)pDetData;
			fRet = FALSE;
			break;
		}

		// Add To Combo

		//int nLen = (int)_tcslen(pDetData->DevicePath);
		//LPTSTR pszDev = new TCHAR[nLen+1];
		//_tcsncpy(pszDev,pDetData->DevicePath,nLen+1);

		//

		if(!FillPartitionInfo(hPhys,k,fname,pDetData->DevicePath))
		{
			//TODO;
		}
		
		
		delete [] (char*)pDetData;
		k++;
	}

	SetupDiDestroyDeviceInfoList(hDevInfo);

	return fRet;
}

/////////////////////////////////////////////////////////////////////

BOOL CviewfsDlg::FillPartitionInfo(HTREEITEM hPhys, int nDisk, 
									LPTSTR lpszDisk,LPTSTR pszDiskPath)
{
	// Add drive to Partition Structure Tree

//	CTreeCtrl *pTree = (CTreeCtrl*)GetDlgItem(IDC_PART_TREE);
	CTreeCtrl *pTree2 = (CTreeCtrl*)GetDlgItem(IDC_TREE);

//	HTREEITEM hDisk = pTree->InsertItem(lpszDisk);
	HTREEITEM hDisk2 = pTree2->InsertItem(lpszDisk,0,0,hPhys);

    HANDLE hFile = CreateFile(pszDiskPath,GENERIC_READ,FILE_SHARE_WRITE ,NULL,OPEN_EXISTING,0,NULL);

	if(INVALID_HANDLE_VALUE == hFile)
	{
		//TODO:
		pTree2->InsertItem(LVIF_TEXT | LVIF_PARAM,_T("Error Opening Disk..."),0,0,0,0,0,hDisk2,TVI_ROOT);
		return FALSE;
	}

	DWORD cbRead = 0;

	DWORD BytesPerSec = HARDCODED_SECTOR_SIZE;

	BYTE *pBuffer = new BYTE[BytesPerSec];
	if(NULL == pBuffer)
	{
		//TODO:
		return FALSE;
	}

	if(!ReadFile(hFile,pBuffer,BytesPerSec,&cbRead,NULL))
	{
		//TODO:
		pTree2->InsertItem(LVIF_TEXT | LVIF_PARAM,_T("Error Reading Disk..."),0,0,0,0,0,hDisk2,TVI_ROOT);
		delete pBuffer;
		return FALSE;
	}

	
	// Careful: this may not be an MBR but just a standard MSDOS FAT Boot Sector

	// TODO: HACKHACK: Have to find correct way of dealing with non-partitioned
	// Disks such as my Creative Zen Micro

	if(pBuffer[3] == 'M' && 
		pBuffer[4] == 'S' && 
		pBuffer[5] == 'D' && 
		pBuffer[6] == 'O' && 
		pBuffer[7] == 'S')
	{
      //  CTreeCtrl *pTree = (CTreeCtrl*)GetDlgItem(IDC_PART_TREE);
//		CListCtrl *pList = (CListCtrl*)GetDlgItem(IDC_PART_LIST);
		BootSector *pbs = (BootSector*)pBuffer;

		DWORD dwTotSec;
		if(pbs->bios_param.BPB_TotSec16 != 0)
		{
			dwTotSec = pbs->bios_param.BPB_TotSec16;
		}
		else
		{
			dwTotSec = pbs->bios_param.BPB_TotSec32;
		}

		//TCHAR szSectorCount[32];
		TCHAR szSize[32];
		TCHAR szDesc[256];

		ULARGE_INTEGER lrgSize;
		//ULARGE_INTEGER tmp;

		//tmp.QuadPart = dwTotSec;
		//FormatULarge(szSectorCount,32,&tmp);

		lrgSize.QuadPart = dwTotSec * HARDCODED_SECTOR_SIZE;
		FormatSizeEx(szSize,&lrgSize);

		//_sntprintf(szDesc,256,_T("Disk(%u)Partition(0)"),nDisk);

		//int nItem = pList->InsertItem(pList->GetItemCount(),szDesc,0);
		//
		//pList->SetItem(nItem,1,LVIF_TEXT,lpszDisk,0,0,0,0);
		//pList->SetItem(nItem,2,LVIF_TEXT,_T("0"),0,0,0,0);
		//pList->SetItem(nItem,3,LVIF_TEXT,_T("FAT"),0,0,0,0);
		//pList->SetItem(nItem,4,LVIF_TEXT,_T("Normal"),0,0,0,0);
		//pList->SetItem(nItem,5,LVIF_TEXT,szSize,0,0,0,0);
		//pList->SetItem(nItem,6,LVIF_TEXT,_T("0"),0,0,0,0);
		//pList->SetItem(nItem,7,LVIF_TEXT,szSectorCount,0,0,0,0);
		
		// TODO: 

		PartitionLoadInfo *ppli = new PartitionLoadInfo;
		ppli->dwDisk = nDisk;
		ppli->dwFS = 0x06; // FAT
		_tcsncpy(ppli->szDevicePath,pszDiskPath,MAX_PATH);
		ppli->lrgFirstSector.QuadPart = 0;
		ppli->lrgSectors.QuadPart = dwTotSec;
		
		//pList->SetItemData(nItem,(DWORD_PTR)ppli);

		_sntprintf(szDesc,256,_T("Partition 0 [FAT] (%s)"),szSize);
		pTree2->InsertItem(LVIF_TEXT | LVIF_PARAM,szDesc,0,0,0,0,(DWORD_PTR)ppli,hDisk2,TVI_ROOT);
		m_vecPartLoad.push_back(ppli);

		// Add To Tree

		//_sntprintf(szDesc,256,_T("Partition [FAT] (%s)"),szSize);
		//pTree->InsertItem(szDesc,hDisk);
	}
	else
	{
		MasterBootRecord *pmbr = (MasterBootRecord*)pBuffer;

		for(int i=0;i<4;i++)
		{
			if(!RecAddPartition(hDisk2,nDisk,lpszDisk,pszDiskPath,0,hFile,&pmbr->pte[i],FALSE))
			{
				delete pBuffer;
				return FALSE;
			}
		}
	}

	delete pBuffer;
	return TRUE;
}

/////////////////////////////////////////////////////////////////////

BOOL CviewfsDlg::RecAddPartition(HTREEITEM hParent,int nDisk,
								  LPTSTR lpszDisk,LPTSTR lpszDiskPath,int nPartition,
								  HANDLE hFile,PartitionTableEntry *lpte,
								  BOOL fPrimaryExtPartitionDone)
{
	if(0 == lpte->type) // Empty => Bail but no fail
	{
		return TRUE;
	}

	//CTreeCtrl *pTree = (CTreeCtrl*)GetDlgItem(IDC_PART_TREE);
	CTreeCtrl *pTree2 = (CTreeCtrl*)GetDlgItem(IDC_TREE);
//	CListCtrl *pList = (CListCtrl*)GetDlgItem(IDC_PART_LIST);

	TCHAR szDesc[256];
	TCHAR szSize[32];
	TCHAR szType[64];

	ULARGE_INTEGER lrgSize;
	lrgSize.QuadPart = lpte->cSectors;
	lrgSize.QuadPart *= HARDCODED_SECTOR_SIZE;

	FormatSizeEx(szSize,&lrgSize);

	//	HTREEITEM hCurrent = pTree->InsertItem(szDesc,hParent);

	
	// MBR Sector => used when fPrimaryPartition TRUE to calc offset to
	// Extended partitions. 
	// LastExtendedMBRSector needed to calc actual sector offsets for
	// contained partitions (non extended)
	//
	// Careful: both of these statics intimately linked with argument 
	// fPrimaryExtPartitionDone

	static ULARGE_INTEGER lrgPrimaryExtendedPartition_MBRSector;
	static ULARGE_INTEGER lrgLastExtendedMBRSector;

	
	// Extended?

	if(0x05 == lpte->type || 0x0F == lpte->type) 
	{
		// Calculate Absolute Offset in Sectors & Bytes to this Extened Partitions MBR
		// using the accumlated relative offsets from previous containing partitions

		ULARGE_INTEGER lrgMBRSector;
		if(!fPrimaryExtPartitionDone)
		{
			fPrimaryExtPartitionDone = TRUE;
			lrgPrimaryExtendedPartition_MBRSector.QuadPart = lpte->cSecsMBRToFirstSector;
			lrgMBRSector.QuadPart = lrgPrimaryExtendedPartition_MBRSector.QuadPart;
		}
		else
		{
			lrgMBRSector.QuadPart = lrgPrimaryExtendedPartition_MBRSector.QuadPart + 
				lpte->cSecsMBRToFirstSector;
		}

		// Store This Offset, It will be used by child partitions of this extended
		// partition, when adding to listctrl and also to calc physical offset later
		// ie. this value is not used inside this block but in the 'else' (non-extended) 
		// part of this block

		lrgLastExtendedMBRSector.QuadPart = lrgMBRSector.QuadPart;

        // Calc Offset

		ULARGE_INTEGER lrgOffset;
		lrgOffset.QuadPart = lrgMBRSector.QuadPart * HARDCODED_SECTOR_SIZE;
		

		// Move to this location

		if(!SetFilePointer(hFile,lrgOffset.LowPart,(PLONG)&lrgOffset.HighPart,FILE_BEGIN))
		{
			return FALSE;
		}

		
		// Read
	
		DWORD cbRead = 0;
		DWORD BytesPerSec = HARDCODED_SECTOR_SIZE;

		BYTE *pBuffer = new BYTE[BytesPerSec];
		if(NULL == pBuffer)
		{
			return FALSE;
		}

		if(!ReadFile(hFile,pBuffer,BytesPerSec,&cbRead,NULL))
		{
			delete pBuffer;
			return FALSE;
		}

		
		// To Iterate is human, to recurse...

		MasterBootRecord *pmbr = (MasterBootRecord*)pBuffer;
		
		for(int i=0;i<4;i++)
		{
			if(!RecAddPartition(hParent/*hCurrent*/,nDisk,lpszDisk,lpszDiskPath,nPartition+1,
				hFile,&pmbr->pte[i],fPrimaryExtPartitionDone))
			{
				delete pBuffer;
				return FALSE;
			}
		}

		delete pBuffer;
	}
	else // Not Extended So we can add to List
	{
		ULARGE_INTEGER lrgMBRSector;

		if(!fPrimaryExtPartitionDone)
		{
			lrgMBRSector.QuadPart = lpte->cSecsMBRToFirstSector;
		}
		else
		{
			lrgMBRSector.QuadPart = lrgLastExtendedMBRSector.QuadPart + 
				lpte->cSecsMBRToFirstSector;
		}

		//TCHAR szState[256];
		//TCHAR szMBRSector[32];
		//TCHAR szSectorCount[32];
		//TCHAR szPart[16];

		ULARGE_INTEGER tmp;
		tmp.QuadPart = lpte->cSectors;

		//FormatPartitionState(szState,32,lpte->state);
		//FormatULarge(szMBRSector,32,&lrgMBRSector);
		//FormatULarge(szSectorCount,32,&tmp);

		//_sntprintf(szPart,16,_T("%u"),nPartition);
		
		//_sntprintf(szDesc,256,_T("Disk(%u)Partition(%u)"),nDisk,nPartition);
		
		
	/*	int nItem = pList->InsertItem(pList->GetItemCount(),szDesc,0);
		
		pList->SetItem(nItem,1,LVIF_TEXT,lpszDisk,0,0,0,0);
		pList->SetItem(nItem,2,LVIF_TEXT,szPart,0,0,0,0);
		pList->SetItem(nItem,3,LVIF_TEXT,szType,0,0,0,0);
		pList->SetItem(nItem,4,LVIF_TEXT,szState,0,0,0,0);
		pList->SetItem(nItem,5,LVIF_TEXT,szSize,0,0,0,0);
		pList->SetItem(nItem,6,LVIF_TEXT,szMBRSector,0,0,0,0);
		pList->SetItem(nItem,7,LVIF_TEXT,szSectorCount,0,0,0,0);*/
		
		// Allow Connection...

		PartitionLoadInfo *ppli = new PartitionLoadInfo;
		ppli->dwDisk = nDisk;
		ppli->dwFS = lpte->type;

		_tcsncpy(ppli->szDevicePath,lpszDiskPath,MAX_PATH);
		ppli->lrgFirstSector.QuadPart = lrgMBRSector.QuadPart;
		ppli->lrgSectors.QuadPart = tmp.QuadPart;
		
		//pList->SetItemData(nItem,(DWORD_PTR)ppli);



		LONGLONG llBeginFileOffset = ppli->lrgFirstSector.QuadPart * HARDCODED_SECTOR_SIZE;
		GetPartitionTypeString(hFile, &llBeginFileOffset, szType,64,lpte->type);

		//_sntprintf(szDesc,256,_T("Partition [%s] (%s) (%X)"),szType,szSize,);
		_sntprintf(szDesc,256,_T("Partition %u [%s] (%s) <%0.16I64X>"),nPartition,szType,szSize,llBeginFileOffset);
		pTree2->InsertItem(LVIF_TEXT | LVIF_PARAM,szDesc,0,0,0,0,
			(DWORD_PTR)ppli,hParent,TVI_ROOT);

		m_vecPartLoad.push_back(ppli);
	}

	return TRUE;
}

/////////////////////////////////////////////////////////////////////

void CviewfsDlg::FormatPartitionState(LPTSTR lpszOut,DWORD dwLen,DWORD dwState)
{
	 if(0x00 == dwState)
	 {
		 _tcsncpy(lpszOut,_T("Normal"),dwLen);
	 }
	 else if(0x80 == dwState)
	 {
		 _tcsncpy(lpszOut,_T("Boot"),dwLen);
	 }
	 else
	 {
		 _tcsncpy(lpszOut,_T("<Unknown>"),dwLen);
	 }
}

/////////////////////////////////////////////////////////////////////

void CviewfsDlg::FormatULarge(LPTSTR lpszOut,DWORD dwLen,PULARGE_INTEGER plrg)
{
	_sntprintf(lpszOut,dwLen,_T("%I64u"),plrg->QuadPart);
}

/////////////////////////////////////////////////////////////////////

void CviewfsDlg::GetPartitionTypeString(	HANDLE hFile, LONGLONG *pllPartBeginOffset, 
											LPTSTR szType, DWORD dwLen,DWORD dwType)
{
	for(int i=0;i<NUMBER_OF_PARTITION_TYPES;i++)
	{
		if(dwType == partition_type_map[i].dwType)
		{
			if(	partition_type_map[i].lpszGUIHandlerDll && 
				0 == _tcscmp(partition_type_map[i].lpszGUIHandlerDll,_T("<LINUX_SPECIAL>")))
			{
				DWORD cbRead;
				BYTE tmp[HARDCODED_REISER_SUPERBLOCK_SIZE];
				
				// Could be Linux Ext2FS, Check for super block at 0x400
				
				LARGE_INTEGER li;
                li.QuadPart = *pllPartBeginOffset + (ULONGLONG)HARDCODED_SUPERBLOCK_START;

				if (SetFilePointerEx(hFile,li,NULL,FILE_BEGIN) &&
					ReadFile(hFile,tmp,HARDCODED_SECTOR_SIZE,&cbRead,NULL))
				{
					ext2_super_block* pSB = (ext2_super_block*)&tmp;
					if(EXT2FS_MAGIC == (WORD)pSB->s_magic)
					{
						_tcsncpy(szType,_T("Ext2FS"),dwLen);
					}
				}				
				
				
				// Could be reiser...

				li.QuadPart = *pllPartBeginOffset + (ULONGLONG)HARDCODED_REISER_SUPERBLOCK_START;

				if (SetFilePointerEx(hFile,li,NULL,FILE_BEGIN) &&
					ReadFile(hFile,tmp,HARDCODED_REISER_SUPERBLOCK_SIZE,&cbRead,NULL))
				{
					reiser_super_block* pSB = (reiser_super_block*)&tmp;
					if(!REISER_MAGIC_OK(pSB->MagicString))
					{
						_tcsncpy(szType,partition_type_map[i].lpszType,dwLen);
					}
					else
					{
						// yep
						_tcsncpy(szType,_T("Reiser"),dwLen);
					}
				}	
				else
				{
					_tcsncpy(szType,partition_type_map[i].lpszType,dwLen);
				}
			}
			else
			{
				_tcsncpy(szType,partition_type_map[i].lpszType,dwLen);
			}

            return;
		}
	}

	_tcsncpy(szType,_T("<Unknown>"),dwLen);
}

/////////////////////////////////////////////////////////////////////

void CviewfsDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

/////////////////////////////////////////////////////////////////////

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CviewfsDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

/////////////////////////////////////////////////////////////////////

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CviewfsDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

/////////////////////////////////////////////////////////////////////

void CviewfsDlg::OnBnClickedOk()
{
	
}

/////////////////////////////////////////////////////////////////////

void CviewfsDlg::OnBnClickedOpendisk()
{
	//CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_PART_LIST);
	//POSITION pos = pList->GetFirstSelectedItemPosition();
	//if(NULL == pos)
	//{
	//	return;
	//}

	//int nSel = pList->GetNextSelectedItem(pos);

	CTreeCtrl* pTree = (CTreeCtrl*)GetDlgItem(IDC_TREE);
	ASSERT(pTree);
	HTREEITEM hSel = pTree->GetSelectedItem();
	if(NULL == hSel)
	{
		return;
	}

	// Get Info

	PartitionLoadInfo* ppli = (PartitionLoadInfo*)
		pTree->GetItemData(hSel);

	if(NULL == ppli) // error reading partition i'd say... clean this up
	{
		return;
	}

	// Try to Load...

	HANDLE hFile = CreateFile(ppli->szDevicePath,GENERIC_READ,
		FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);

	if(INVALID_HANDLE_VALUE == hFile)
	{
		AfxMessageBox(_T("Error Opening Disk Partition..."));
        return;
	}

	LONGLONG llBeginFileOffset = 
		ppli->lrgFirstSector.QuadPart * HARDCODED_SECTOR_SIZE;

	if(!LoadFileSystem(ppli->dwFS, hFile, llBeginFileOffset))
	{
		AfxMessageBox(_T("Error Opening Disk Partition..."));
        return;	
	}
}

/////////////////////////////////////////////////////////////////////

void CviewfsDlg::OnBnClickedOpendrive()
{
	// Load Drive

	//TCHAR szDrive[16];
	//CComboBox* pCmb = (CComboBox*)GetDlgItem(IDC_CMBDRIVES);
	CTreeCtrl* pTree = (CTreeCtrl*)GetDlgItem(IDC_TREE);
	HTREEITEM hSel = pTree->GetSelectedItem();
	CString strDrive = pTree->GetItemText(hSel);

	//pCmb->GetLBText(pCmb->GetCurSel(),szDrive);

	TCHAR szFmt[32] = _T("\\\\.\\");
	_tcscat(szFmt,strDrive);

	HANDLE hFile = CreateFile(szFmt,GENERIC_READ,FILE_SHARE_WRITE ,
		NULL,OPEN_EXISTING,0,NULL);

	if(INVALID_HANDLE_VALUE == hFile)
	{
		AfxMessageBox(_T("Error Opening File"));
        return;
	}

	_tcscat(szFmt,_T("\\"));
	TCHAR szTmp[256];
	DWORD dw;
	TCHAR szFS[32];

	// Determine File Type...

	if(!GetVolumeInformation(szFmt,szTmp,256,NULL,&dw,&dw,szFS,32))
	{
		CloseHandle(hFile);
		AfxMessageBox(_T("Error Opening File"));
        return;
	}

	DWORD dwFS;
	if (szFS[0] == 'N' &&
		szFS[1] == 'T' &&
		szFS[2] == 'F' &&
		szFS[3] == 'S')
	{
		dwFS = 0x07; // NutFS!
	}
	else if(szFS[0] == 'F' &&
			szFS[1] == 'A' &&
			szFS[2] == 'T')
	{
		dwFS = 0x0B; // FAT
	}
	else
	{
		CloseHandle(hFile);
		AfxMessageBox(_T("Do Not Understand File System..."));
		return;
	}

	if(!LoadFileSystem(dwFS,hFile,0))
	{
		AfxMessageBox(_T("Error Mounting Logical Drive..."));
        return;	
	}
}

/////////////////////////////////////////////////////////////////////

void CviewfsDlg::OnBnClickedLoadimage()
{
	CTreeCtrl* pTree= (CTreeCtrl*)GetDlgItem(IDC_TREE);
	HTREEITEM hSel = pTree->GetSelectedItem();
	ASSERT(hSel);

	DWORD_PTR param = pTree->GetItemData(hSel);
	// Load Image

	//TCHAR szFile[MAX_PATH];
	//GetDlgItemText(IDC_EDIMAGE,szFile,MAX_PATH);

	CString strFile;
	if(param == PARAM_BROWSE_FOR_IMAGE)
	{
		CFileDialog dlg(TRUE,0,0,
			OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST,0,this,0);
		if(IDCANCEL == dlg.DoModal())
		{
			return;
		}

		strFile = dlg.GetPathName();
	}
	else
	{
		strFile = pTree->GetItemText(hSel);
		strFile = strFile.Left(strFile.Find(_T(" [")));
	}

	
	HANDLE hFile = CreateFile(strFile,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);

	if(INVALID_HANDLE_VALUE == hFile)
	{
		AfxMessageBox(_T("Error Opening File"));
        return;
	}

	DWORD cbRead = sizeof(BootSector);
	BYTE *pBuffer = new BYTE[cbRead];
	if(NULL == pBuffer)
	{
		AfxMessageBox(_T("Error Memory"));
		return;
	}

	if(!ReadFile(hFile,pBuffer,cbRead,&cbRead,NULL))
	{
		delete pBuffer;
		AfxMessageBox(_T("Error Reading File"));
		return;
	}

	// Try to detect File System Type

	BootSector *pbs = (BootSector*)pBuffer;

	DWORD dwFS;
	if (pbs->oemName[0] == 'N' &&
		pbs->oemName[1] == 'T' &&
		pbs->oemName[2] == 'F' &&
		pbs->oemName[3] == 'S')
	{
		dwFS = 0x07; // NutFS!
	}
	else if((pbs->oemName[0] == 'M' &&
			pbs->oemName[1] == 'S' &&
			pbs->oemName[2] == 'D' &&
			pbs->oemName[3] == 'O' &&
			pbs->oemName[4] == 'S') || (
			pbs->oemName[0] == 'F' &&
			pbs->oemName[1] == 'A' &&
			pbs->oemName[2] == 'T') || (
			pbs->FS_Dep.fat16.BS_FilSysType[0] == 'F' &&
			pbs->FS_Dep.fat16.BS_FilSysType[1] == 'A' &&
			pbs->FS_Dep.fat16.BS_FilSysType[2] == 'T') || (
			pbs->FS_Dep.fat32.BS_FilSysType[0] == 'F' &&
			pbs->FS_Dep.fat32.BS_FilSysType[1] == 'A' &&
			pbs->FS_Dep.fat32.BS_FilSysType[2] == 'T'))
	{
		dwFS = 0x0B; // FAT
	}
	else
	{
		BYTE tmp[HARDCODED_REISER_SUPERBLOCK_SIZE];
		// Could be Linux Ext2FS, Check for super block at 0x400
		
		if (SetFilePointer(hFile,HARDCODED_SUPERBLOCK_START,NULL,FILE_BEGIN) &&
			ReadFile(hFile,tmp,HARDCODED_SECTOR_SIZE,&cbRead,NULL))
		{
			ext2_super_block* pSB = (ext2_super_block*)&tmp;
			if(EXT2FS_MAGIC == (WORD)pSB->s_magic)
			{
				// yep
				dwFS = 0x83;
			}
			else
			{
				delete pBuffer;
				CloseHandle(hFile);
				AfxMessageBox(_T("Do Not Understand File System..."));
				return;
			}
		}
		else if(SetFilePointer(hFile,HARDCODED_REISER_SUPERBLOCK_START,NULL,FILE_BEGIN) &&
				ReadFile(hFile,tmp,HARDCODED_REISER_SUPERBLOCK_SIZE,&cbRead,NULL))
		{
			reiser_super_block* pSB = (reiser_super_block*)&tmp;
			if(!REISER_MAGIC_OK(pSB->MagicString))
			{
				delete pBuffer;
				CloseHandle(hFile);
				AfxMessageBox(_T("Do Not Understand File System..."));
				return;
			}
			else
			{
				dwFS = 0x83;
			}
		}
		else
		{
			// TODO: May not be a boot sector but a Master Boot Record
			// with a partition table => try to detect this...
			// MasterBootRecord *pmbr = (MasterBootRecord*)pBuffer;
			// TODO: Display Partitions and allowm user to choose...

			delete pBuffer;
			CloseHandle(hFile);
			AfxMessageBox(_T("Do Not Understand File System..."));
			return;
		}
	}

	if(!LoadFileSystem(dwFS,hFile,0))
	{
		delete pBuffer;
		AfxMessageBox(_T("Error Mounting Image..."));
        return;	
	}

	if(param == PARAM_BROWSE_FOR_IMAGE)
	{
		TCHAR szType[64];
		LONGLONG ll = 0;
		GetPartitionTypeString(hFile,&ll,szType,64,dwFS);

		strFile += _T(" [");
		strFile += szType;
		strFile += _T("]");

		// All went well add it to tree and change selection to it

		HTREEITEM hImg = pTree->GetParentItem(hSel);
		HTREEITEM hNew = pTree->InsertItem(LVIF_TEXT | LVIF_PARAM,strFile,0,0,0,0,0,
			hImg,TVI_LAST);

		pTree->Select(hNew,TVGN_CARET);
	}

	delete pBuffer;
}

/////////////////////////////////////////////////////////////////////

BOOL CviewfsDlg::LoadFileSystem(DWORD dwType, HANDLE hFile, 
								 LONGLONG llBeginFileOffset)
{
	// Find handler

	GUIFileSystemBaseHandler* pFSHandler = NULL;
	HMODULE hHandlerDll = NULL;

	for(int i=0;i<NUMBER_OF_PARTITION_TYPES;i++)
	{
		if(dwType == partition_type_map[i].dwType)
		{
			if(	partition_type_map[i].lpszGUIHandlerDll &&
				0 == _tcscmp(partition_type_map[i].lpszGUIHandlerDll,_T("<LINUX_SPECIAL>")))
			{
				DWORD cbRead;
				BYTE tmp[HARDCODED_REISER_SUPERBLOCK_SIZE];

				// Could be Linux Ext2FS, Check for super block at 0x400
				
				LARGE_INTEGER li;
                li.QuadPart = llBeginFileOffset + (ULONGLONG)HARDCODED_SUPERBLOCK_START;

				if (SetFilePointerEx(hFile,li,NULL,FILE_BEGIN) &&
					ReadFile(hFile,tmp,HARDCODED_SECTOR_SIZE,&cbRead,NULL))
				{
					ext2_super_block* pSB = (ext2_super_block*)&tmp;
					if(EXT2FS_MAGIC == (WORD)pSB->s_magic)
					{
						// yep
						hHandlerDll = LoadLibrary(_T("MMcGExt2FS.DLL"));
					}
				}				

				
				// Reiser??

				li.QuadPart = llBeginFileOffset + (ULONGLONG)HARDCODED_REISER_SUPERBLOCK_START;

				if (SetFilePointerEx(hFile,li,NULL,FILE_BEGIN) &&
					ReadFile(hFile,tmp,HARDCODED_REISER_SUPERBLOCK_SIZE,&cbRead,NULL))
				{
					reiser_super_block* pSB = (reiser_super_block*)&tmp;
					if(!REISER_MAGIC_OK(pSB->MagicString))
					{
					}
					else
                    {
						// yep
						hHandlerDll = LoadLibrary(_T("MMcGReiser.DLL"));
					}
				}	
				else
				{
					AfxMessageBox(_T("This File System is currently unsupported."));
					CloseHandle(hFile);
					return FALSE;
				}
			}
			else
			{
				// Load Handler
				hHandlerDll = LoadLibrary(partition_type_map[i].lpszGUIHandlerDll);
				if(NULL == hHandlerDll)
				{
					AfxMessageBox(_T("This File System is currently unsupported."));
					CloseHandle(hFile);
					return FALSE;
				}
			}
			
			PFN_GET_GUI_FS_HANDLER fn = (PFN_GET_GUI_FS_HANDLER)
				GetProcAddress(hHandlerDll,kszHandlerFuncName);

			if(NULL == fn)
			{
				FreeLibrary(hHandlerDll);
				AfxMessageBox(_T("Error Loading File System Handler."));
				CloseHandle(hFile);
				return FALSE;
			}
			
			if(!fn(dwType, &pFSHandler))
			{
				FreeLibrary(hHandlerDll);
				AfxMessageBox(_T("Error Loading File System Handler."));
				CloseHandle(hFile);
				return FALSE;
			}
			
			break;
		}
	}

	if(NULL == pFSHandler)
	{
		FreeLibrary(hHandlerDll);
		AfxMessageBox(_T("Error Loading File System Handler."));
		CloseHandle(hFile);
		return FALSE;
	}

	// Yes we can handle => Initialize & Display Root Dir...
	
	CListCtrl *pLC = (CListCtrl*)GetDlgItem(IDC_LIST1);

	if (!pFSHandler->Init(hFile,llBeginFileOffset,pLC->m_hWnd))
	{
		AfxMessageBox(_T("Error Initializing File System..."));
		CloseHandle(hFile);
		return FALSE;
	}

	// Load new state

	pLC->DeleteAllItems();
	CleanupState();

	m_hFile = hFile;
	m_llBeginFileOffset.QuadPart = llBeginFileOffset;
	m_hHandlerDll = hHandlerDll;
	m_pFSHandler = pFSHandler;

	TCHAR szPath[MAX_PATH];
	if (!m_pFSHandler->DisplayRootDirectory() ||
		!m_pFSHandler->GetCurrentPath(szPath,MAX_PATH))
	{
		CleanupState();
		AfxMessageBox(_T("Error Displaying Root Directory..."));
		return FALSE;
	}

	// & Path

	SetDlgItemText(IDC_STATUS,szPath);

	return TRUE;
}

/////////////////////////////////////////////////////////////////////

void CviewfsDlg::FormatSizeEx(LPTSTR pszSize,ULARGE_INTEGER *psize)
{
	DWORD dwGig = (1024*1024*1024);
	ULARGE_INTEGER Tera;
	Tera.QuadPart = dwGig;
	Tera.QuadPart *= 1024;

	if(psize->QuadPart >= Tera.QuadPart)
	{
		_sntprintf(pszSize,32,_T("%.2f Tb"),(float)psize->QuadPart / Tera.QuadPart);
	}
	else if(psize->QuadPart >= dwGig)
	{
		_sntprintf(pszSize,32,_T("%.2f Gb"),(float)psize->QuadPart / (1024*1024*1024));
	}
	else if(psize->QuadPart >= (1024*1024))
	{
		_sntprintf(pszSize,32,_T("%.2f Mb"),(float)psize->QuadPart / (1024*1024));
	}
	else if(psize->QuadPart >= (1024))
	{
		_sntprintf(pszSize,32,_T("%.1f Kb"),(float)psize->QuadPart / 1024);
	}
	else
	{
		_sntprintf(pszSize,32,_T("%d Bytes"),psize->QuadPart);
	}
}

/////////////////////////////////////////////////////////////////////

void CviewfsDlg::OnNMDblclkList1(NMHDR *pNMHDR, LRESULT *pResult)
{
	*pResult = 0;

	if(m_pFSHandler)
	{
		if(!m_pFSHandler->PerformDefaultItemAction())
		{
			AfxMessageBox(_T("Error Handling Dbl Click..."));
		}
		
		TCHAR szPath[256] = {0};
		if(!m_pFSHandler->GetCurrentPath(szPath,256))
		{
			AfxMessageBox(_T("Error Handling Dbl Click..."));
		}
	
		SetDlgItemText(IDC_STATUS,szPath);
	}
}

/////////////////////////////////////////////////////////////////////
//
//void CviewfsDlg::OnLvnKeydownList1(NMHDR *pNMHDR, LRESULT *pResult)
//{
//	LPNMLVKEYDOWN pLVKeyDow = reinterpret_cast<LPNMLVKEYDOWN>(pNMHDR);
//	// TODO: Add your control notification handler code here
//	*pResult = 0;
//
//	if(pLVKeyDow->wVKey == VK_RETURN)
//	{
//		if(m_pFSHandler)
//		{
//			if(!m_pFSHandler->OnListEnterKey())
//			{
//				AfxMessageBox(_T("Error Handling Dbl Click..."));
//			}
//			
//			TCHAR szPath[256] = {0};
//			if(!m_pFSHandler->GetCurrentPath(szPath,256))
//			{
//				AfxMessageBox(_T("Error Handling Dbl Click..."));
//			}
//		
//			SetDlgItemText(IDC_STATUS,szPath);
//		}
//	}
//}
//
///////////////////////////////////////////////////////////////////////
//
//void CviewfsDlg::OnNMReturnList1(NMHDR *pNMHDR, LRESULT *pResult)
//{
//	*pResult = 0;
//
//	if(m_pFSHandler)
//	{
//		if(!m_pFSHandler->OnListEnterKey())
//		{
//			AfxMessageBox(_T("Error Handling Dbl Click..."));
//		}
//		
//		TCHAR szPath[256] = {0};
//		if(!m_pFSHandler->GetCurrentPath(szPath,256))
//		{
//			AfxMessageBox(_T("Error Handling Dbl Click..."));
//		}
//	
//		SetDlgItemText(IDC_STATUS,szPath);
//	}
//}

/////////////////////////////////////////////////////////////////////

void CviewfsDlg::PostNcDestroy()
{
	CleanupState();
	CDialog::PostNcDestroy();
}

/////////////////////////////////////////////////////////////////////

void CviewfsDlg::CleanupState()
{
	if(m_pFSHandler)
	{
		m_pFSHandler->CloseAndDestroy();
	}
	m_pFSHandler = NULL;

	if(m_hHandlerDll)
	{
		FreeLibrary(m_hHandlerDll);
	}
	m_hHandlerDll = NULL;

	if(m_hFile)
	{
		if(INVALID_HANDLE_VALUE != m_hFile)
		{
			CloseHandle(m_hFile);
		}

		m_hFile = NULL;
	}
	m_llBeginFileOffset.QuadPart = 0;
}

/////////////////////////////////////////////////////////////////////

void CviewfsDlg::OnDestroy()
{
	CleanupPartitionList();

	CDialog::OnDestroy();
}

/////////////////////////////////////////////////////////////////////

void CviewfsDlg::CleanupPartitionList()
{
	CTreeCtrl *pTree = (CTreeCtrl*)GetDlgItem(IDC_TREE);
	pTree->DeleteAllItems();

	for(vector<PartitionLoadInfo*>::iterator it = m_vecPartLoad.begin();it != m_vecPartLoad.end();it++)
	{
		ASSERT(*it);
		delete (*it);
	}
	m_vecPartLoad.clear();

	
	//int cItems = pList->GetItemCount();

	//for(int i=0;i<cItems;i++)
	//{
 //       PartitionLoadInfo *ppli = (PartitionLoadInfo*)pList->GetItemData(i);

	//	// Nullify just in case (Race Condition)

	//	pList->SetItemData(i,NULL);

 //       if(ppli)
	//	{
	//		delete ppli;
	//	}
	//}

}

/////////////////////////////////////////////////////////////////////

void CviewfsDlg::CleanupLogicalDrives()
{
//	CComboBox* pCmb = (CComboBox*)GetDlgItem(IDC_CMBDRIVES);
//	pCmb->ResetContent();
}

/////////////////////////////////////////////////////////////////////

void CviewfsDlg::OnBnClickedRefresh()
{
	DisplaySelected();
}

/////////////////////////////////////////////////////////////////////

void CviewfsDlg::RefreshDrives()
{
	CleanupPartitionList();
	CleanupLogicalDrives();

	// Physical Disks

	if(!DoPhysicalDisks())
	{
		AfxMessageBox(_T("Error Doing Physical Disks Combo..."));
	}

	// Logical Drives 

	if(!DoLogicalDisks())
	{
		AfxMessageBox(_T("Error Doing Logical Disks Combo..."));
	}
}

/////////////////////////////////////////////////////////////////////

void CviewfsDlg::OnBnClickedCdup()
{
	if(m_pFSHandler)
	{
		if(!m_pFSHandler->CDUp())
		{
			AfxMessageBox(_T("Error CDing Up..."));
		}

		TCHAR szPath[256] = {0};
		if(!m_pFSHandler->GetCurrentPath(szPath,256))
		{
			AfxMessageBox(_T("Error Handling Dbl Click..."));
		}
	
		SetDlgItemText(IDC_STATUS,szPath);
	}
}

/////////////////////////////////////////////////////////////////////

void CviewfsDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);

	if(!IsWindow(m_hWnd))
	{
		return;
	}

	CListCtrl *pLC = (CListCtrl *)GetDlgItem(IDC_LIST1);
	CTreeCtrl *pTree = (CTreeCtrl *)GetDlgItem(IDC_TREE);
	CWnd* pStat = GetDlgItem(IDC_STATUS);
	CWnd* pTool = GetDlgItem(IDC_STATUS);
	if(NULL == pLC || NULL == pTree || NULL == pStat)
	{
		return;
	}

	// Move Status bar to fill bottom

	CRect rc;
	pStat->GetClientRect(rc);
	pStat->SetWindowPos(NULL,2,cy-rc.Height()-2,cx,rc.Height(),SWP_NOZORDER);

	
	// get toolbar size

	CRect rc3;
	pTool->GetWindowRect(rc3);
	int cyTool = rc3.bottom - rc3.top;

	// Get Main Height 

	int acy = cy - cyTool - rc.bottom - 20;

	// Resize Tree

	CRect rc2;
	pTree->GetWindowRect(rc2);
	pTree->SetWindowPos(NULL,0,0,rc2.right-rc2.left,acy,SWP_NOMOVE | SWP_NOZORDER);

	// Resize List

	pLC->SetWindowPos(NULL,0,0,cx - (rc2.right-rc2.left + 5),acy,SWP_NOMOVE | SWP_NOZORDER);

	//CRect rc;
	//pOldLine->GetWindowRect(&rc);
	//ScreenToClient(&rc);

	//int acy = rc.top - 5;

	//CRect rc1;
	//pTree->GetWindowRect(rc1);
	//pTree->SetWindowPos(NULL,0,0,rc1.Width(),acy,SWP_NOMOVE);


}

/////////////////////////////////////////////////////////////////////
//void CviewfsDlg::OnNMClickTree(NMHDR *pNMHDR, LRESULT *pResult)
//{
//	// TODO: Add your control notification handler code here
//	*pResult = 0;
//}

void CviewfsDlg::OnTvnSelchangedTree(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);
	// TODO: Add your control notification handler code here
	*pResult = 0;

	DisplaySelected();
}

/////////////////////////////////////////////////////////////////////

void CviewfsDlg::DisplaySelected()
{
	CListCtrl* pLC = (CListCtrl*)GetDlgItem(IDC_LIST1);
	pLC->DeleteAllItems();
	CleanupState();

	CTreeCtrl* pTree = (CTreeCtrl*)GetDlgItem(IDC_TREE);
	HTREEITEM hSel = pTree->GetSelectedItem();

	if(NULL == hSel)
	{
		return;
	}

	HTREEITEM hParent = pTree->GetParentItem(hSel);

	if(NULL == hParent)
	{
		return;
	}

	HTREEITEM hGP = pTree->GetParentItem(hParent);

	DWORD_PTR param = (int)pTree->GetItemData(hParent);

	if(PARAM_LOGICAL_DRIVE == param)
	{
		OnBnClickedOpendrive();
	}
	else if(PARAM_LOAD_IMAGE == param)
	{
		OnBnClickedLoadimage();
	}
	else if(NULL != hGP)
	{
		param = pTree->GetItemData(hGP);

		if(PARAM_PHYSICAL_DRIVE == param)
		{
			OnBnClickedOpendisk();
		}
	}
}

/////////////////////////////////////////////////////////////////////

BOOL CviewfsDlg::PreTranslateMessage(MSG* pMsg)
{
	if(pMsg->message == WM_KEYDOWN)
	{
		if(	GetFocus()->m_hWnd != GetDlgItem(IDC_LIST1)->m_hWnd || 
			NULL == m_pFSHandler)
		{
			return CDialog::PreTranslateMessage(pMsg);
		}

		if(pMsg->wParam == VK_RETURN)
		{
			if(!m_pFSHandler->PerformDefaultItemAction())
			{
				AfxMessageBox(_T("Error Handling Dbl Click..."));
			}
			
			TCHAR szPath[256] = {0};
			if(!m_pFSHandler->GetCurrentPath(szPath,256))
			{
				AfxMessageBox(_T("Error Handling Key"));
			}
		
			SetDlgItemText(IDC_STATUS,szPath);

			return TRUE;
		}
		else if(pMsg->wParam == VK_BACK)
		{
			if(!m_pFSHandler->CDUp())
			{
				AfxMessageBox(_T("Error Handling Key"));
			}
			
			TCHAR szPath[256] = {0};
			if(!m_pFSHandler->GetCurrentPath(szPath,256))
			{
				AfxMessageBox(_T("Error Handling Key"));
			}
		
			SetDlgItemText(IDC_STATUS,szPath);

			return TRUE;
		}
	}

	return CDialog::PreTranslateMessage(pMsg);
}

/////////////////////////////////////////////////////////////////////

void CviewfsDlg::OnHdnItemclickList1(NMHDR *pNMHDR, LRESULT *pResult)
{
	if(NULL == m_pFSHandler)
	{
		*pResult = 0;
		return;
	}

	LPNMHEADER phdr = reinterpret_cast<LPNMHEADER>(pNMHDR);
	
	m_pFSHandler->ColumnSort(phdr->iItem);

	*pResult = 0;
}

/////////////////////////////////////////////////////////////////////