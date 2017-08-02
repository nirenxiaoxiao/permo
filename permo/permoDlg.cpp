// permoDlg.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "permo.h"
#include "permoDlg.h"

#include <shlwapi.h>

#include "utils/PcapNetFilter.h"
#include "utils/Utils.h"
#include "utils/PortCache.h"

#include <Windows.h>
#include <Winsvc.h>
#include <WinIoCtl.h>

#define  MYNPF _T("NPF")
#define  MYWINRIN0 _T("MyWinRing0")
#define OLS_TYPE 40000
#define IOCTL_OLS_READ_MSR \
	CTL_CODE(OLS_TYPE, 0x821, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define SHOW_CPU_USAGE 0
#define SHOW_MEM_USAGE 1
#define SHOW_DISK_TEMP 2
#define SHOW_CPU_TEMP 3
#define SHOW_DISK_USAGE 4

HANDLE gHandle = INVALID_HANDLE_VALUE;
HANDLE gHandle2 = INVALID_HANDLE_VALUE;
TCHAR gDriverPath[MAX_PATH];
TCHAR gDriverPath2[MAX_PATH];
BOOL gIsMsr = FALSE;

void LoadDriver();
BOOL StopDriver(SC_HANDLE hSCManager,LPCTSTR DriverId);
BOOL RemoveDriver(SC_HANDLE hSCManager, LPCTSTR DriverId);
BOOL IsFileExist(LPCTSTR fileName);
BOOL Initialize(int driveId);
void LoadDriver(int driveId);
BOOL OpenDriver(int driveId);
void Remove();


// #ifdef _DEBUG
// #define new DEBUG_NEW
// #endif

CpermoDlg *pThis;

unsigned int nSkin;			//Ƥ�����

int nTempDisk=0;		//Ӳ���¶�
//��ϸ������ʾ2����(true)����3����(false)
bool bShowNetInfo;
//�Ƿ�����¶ȼ��,����CPU��Ӳ���¶�
bool bShowTempInfo;
int nTempCpu=0;		//cpu�¶�
//bool bIsWindowsVistaOrGreater;
unsigned int nFontSize;

int processor_count_ = -1;

//������������ʾ�����ݣ�0-cpu 1-�ڴ� 2-���� 3-�ϴ�
unsigned int nBandShow;
//����Ҽ��˵�
CMenu              m_BandMenu;
CMenu              m_BandFontSizeMenu;
CMenu              m_BandWidthMenu;
CMenu              m_BandHeightMenu;

// Capture thread
HANDLE g_hCaptureThread;
bool   g_bCapture = false;

// Adapter
int    g_nAdapters = 0;
int    g_iAdapter;
TCHAR  g_szAdapterNames[16][256];
static CRITICAL_SECTION _cs;

vector<CProInfo*> vecProInfo;

vector<CProInfo*> vecCpu;
vector<CProInfo*> vecMem;
vector<CProInfo*> vecNet;

PcapNetFilter filter;

void Lock()
{
	EnterCriticalSection(&_cs);
}

void Unlock()
{
	LeaveCriticalSection(&_cs);
}

int GetProcessIndex(int pid)
{
	int index = -1;
	for (int i=0; i<vecProInfo.size(); i++)
	{
		if (pid == vecProInfo[i]->id)
		{
			return i;
		}
	}
	return index;
}

void OnPacket(PacketInfoEx *pi)
{
	int index = GetProcessIndex(pi->pid);

	if( index == -1 ) // A new process
	{
// 		// Insert a ProcessItem
// 		Lock();
// 		proinfo item;
// 
// 		RtlZeroMemory(&item, sizeof(item));
// 
// 		item.active = true;
// 		item.dirty = false;
// 		item.pid = pi->pid; // The first pid is logged
// 		item.puid = pi->puid;
// 
// 
// 		//item.hidden = false;
// 
// 		item.txRate = 0;
// 		item.rxRate = 0;
// 		item.prevTxRate = 0;
// 		item.prevRxRate = 0;
// 
// 		// Add to process list
// 		vProInfo.push_back(item);
// 		Unlock();
		
	}
	else
	{
		Lock();

		// Update the ProcessItem that already Exists
		CProInfo *item = vecProInfo[index];

		if( !item->active )
		{
			item->active = true;
			item->pid = pi->pid; // The first pid is logged

			//_tcscpy_s(item.fullPath, MAX_PATH, pi->fullPath);

			item->txRate = 0;
			item->rxRate = 0;
			item->prevTxRate = 0;
			item->prevRxRate = 0;
		}

		if( pi->dir == DIR_UP )
		{
			item->txRate += pi->size;
		}
		else if( pi->dir == DIR_DOWN )
		{
			item->rxRate += pi->size;
		}
		item->dirty = true;

		Unlock();
	}
}
static DWORD WINAPI CaptureThread(LPVOID lpParam)
{
	
	PacketInfo pi;
	PacketInfoEx pie;

	PortCache pc;

	// Init Filter ------------------------------------------------------------
// 	if( !filter.Init())
// 	{
// 		return 1;
// 	}

	// Find Devices -----------------------------------------------------------
// 	if( !filter.FindDevices())
// 	{
// 		return 2;
// 	}

	// Select a Device --------------------------------------------------------
	if( !filter.Select(g_iAdapter))
	{
		return 3;
	}

	// Capture Packets --------------------------------------------------------
	while( g_bCapture )
	{
		int pid = -1;
		int processUID = -1;
		//TCHAR processName[MAX_PATH] = TEXT("Unknown");
		//TCHAR processFullPath[MAX_PATH] = TEXT("-");

		// - Get a Packet (Process UID or PID is not Provided Here)
		if (!filter.Capture(&pi, &g_bCapture))
		{
			Sleep(10000);
			::PostMessage(pThis->GetSafeHwnd(), WM_RECONNECT, 0, 0);
			break;
		}

		// - Stop
		if( !g_bCapture )
		{
			break;
		}

		// - Get PID
		if( pi.trasportProtocol == TRA_TCP )
		{
			pid = pc.GetTcpPortPid(pi.local_port);
			pid = ( pid == 0 ) ? -1 : pid;
		}
		else if( pi.trasportProtocol == TRA_UDP )
		{
			pid = pc.GetUdpPortPid(pi.local_port);
			pid = ( pid == 0 ) ? -1 : pid;
		}
		
		if (pid != -1)
		{
			// - Fill PacketInfoEx
			memcpy(&pie, &pi, sizeof(pi));

			pie.pid = pid;
			pie.puid = processUID;

			OnPacket(&pie);
		}
		else
		{
			TRACE("pid=-1\n");
			Sleep(50);
		}
	}

	// End --------------------------------------------------------------------
/*	filter.End();*/

	return 0;
}
BOOL Is64BitSystem()
{
	SYSTEM_INFO si;
	GetNativeSystemInfo(&si);
	if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ||    
		si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64 )
	{
		return TRUE;
	}
	return FALSE;	
}
void FileCopyTo(CString source, CString destination, CString searchStr, BOOL cover = TRUE)
{
	CString strSourcePath = source;
	CString strDesPath = destination;
	CString strFileName = searchStr;
	CFileFind filefinder;
	CString strSearchPath = strSourcePath + _T("\\") + strFileName;
	CString filename;
	BOOL bfind = filefinder.FindFile(strSearchPath);
	CString SourcePath, DisPath;
	while (bfind)
	{
		bfind = filefinder.FindNextFile();
		filename = filefinder.GetFileName();
		SourcePath = strSourcePath + _T("\\") + filename;
		DisPath = strDesPath + _T("\\") + filename;
		CopyFile((LPCTSTR)SourcePath, (LPCTSTR)DisPath, cover);
	}
	filefinder.Close();
}


void Remove()
{
	SC_HANDLE	hSCManager = NULL;
	hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	CloseHandle(gHandle2);
	StopDriver(hSCManager,MYWINRIN0);
	if (!pThis->bHadWinpcap)
	{
		CloseHandle(gHandle);
		StopDriver(hSCManager,MYNPF);
	}
	//RemoveDriver(hSCManager,MYWINRIN0);
	CloseServiceHandle(hSCManager);
}
//������
BOOL OpenDriver(int driverId)
{
	//char message[256];
	//char *str=_T("\\\\.\\") OLS_DRIVER_ID;
	if (0 == driverId)
	{
		gHandle = CreateFile(
			_T("\\\\.\\NPF"),
			GENERIC_READ | GENERIC_WRITE,
			0,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL
			);
		// 	CString tmp;
		// 	tmp.Format(_T("OpenDriver Failed:%d"), code);
		if(gHandle == INVALID_HANDLE_VALUE)
		{
			/*		AfxMessageBox(tmp);*/
			return FALSE;
		}
	}
	else
	{
		gHandle2 = CreateFile(
			_T("\\\\.\\WinRing0_2_0_0"),
			GENERIC_READ | GENERIC_WRITE,
			0,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL
			);
		if(gHandle2 == INVALID_HANDLE_VALUE)
		{
			/*		AfxMessageBox(tmp);*/
			return FALSE;
		}
	}
	return TRUE;
}
BOOL Initialize(int driveId)
{
	TCHAR dir[MAX_PATH];
	TCHAR *ptr;

	GetModuleFileName(NULL, dir, MAX_PATH);
	if((ptr = _tcsrchr(dir, '\\')) != NULL)
	{
		*ptr = '\0';
	}
	if (0 == driveId)
	{
		wsprintf(gDriverPath, _T("%s\\%s"), dir, _T("npf.sys"));
		if(IsFileExist(gDriverPath) == FALSE)
		{
			return FALSE;
		}
	}
	else
	{
		wsprintf(gDriverPath2, _T("%s\\%s"), dir, _T("WinRing0.sys"));
		if(IsFileExist(gDriverPath2) == FALSE)
		{
			return FALSE;
		}
	}
	LoadDriver(driveId);
	OpenDriver(driveId);
	return TRUE;

}
BOOL IsFileExist(LPCTSTR fileName)
{
	WIN32_FIND_DATA	findData;

	HANDLE hFile = FindFirstFile(fileName, &findData);
	if(hFile != INVALID_HANDLE_VALUE)
	{
		FindClose( hFile );
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}
//��������
void LoadDriver(int driveId)
{
	SC_HANDLE	hSCManager = NULL;
	SC_HANDLE	hService = NULL;
	hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (NULL == hSCManager)
	{
		return;
	}
	if (0 == driveId)
	{
		hService = CreateService(hSCManager,
			MYNPF,
			MYNPF,
			SERVICE_ALL_ACCESS,
			SERVICE_KERNEL_DRIVER,
			SERVICE_DEMAND_START,
			SERVICE_ERROR_NORMAL,
			gDriverPath,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL
			);
	}
	else
	{
		hService = CreateService(hSCManager,
			MYWINRIN0,
			MYWINRIN0,
			SERVICE_ALL_ACCESS,
			SERVICE_KERNEL_DRIVER,
			SERVICE_DEMAND_START,
			SERVICE_ERROR_NORMAL,
			gDriverPath2,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL
			);
	}
//	CString tmp;
	if(hService == NULL)
	{
		DWORD dwRtn = GetLastError();
		if( dwRtn != ERROR_IO_PENDING && dwRtn != ERROR_SERVICE_EXISTS ) 
		{
// 			tmp.Format(_T("��������ԭ�򴴽�����ʧ��:%u"), dwRtn);
// 			AfxMessageBox(tmp);
			return;
		}
		else
		{
/*			AfxMessageBox(_T("���񴴽�ʧ�ܣ������ڷ����Ѿ�������"));*/
		}
		//�����������ֻ��Ҫ����
		if (0 == driveId)
		{
			hService = OpenService(hSCManager, MYNPF, SERVICE_ALL_ACCESS);
		}
		else
		{
			hService = OpenService(hSCManager, MYWINRIN0, SERVICE_ALL_ACCESS);
		}
		if (hService == NULL)
		{
/*			AfxMessageBox(_T("���������ʧ�ܣ�"));*/
		}
		else
		{
/*			AfxMessageBox(_T("��������򿪳ɹ���"));*/
		}
	}
	else
	{
/*		AfxMessageBox(_T("�������񴴽��ɹ�"));*/
	}
	if (0 == driveId)
	{
		hService = OpenService(hSCManager, MYNPF, SERVICE_ALL_ACCESS);
	}
	else
	{
		hService = OpenService(hSCManager, MYWINRIN0, SERVICE_ALL_ACCESS);
	}
	
	if (hService == NULL)
	{
/*		AfxMessageBox(_T("���������ʧ�ܣ�"));*/
	}
	else
	{
/*		AfxMessageBox(_T("��������򿪳ɹ���"));*/
	}

	BOOL result=StartService(hService, 0, NULL);
	if (!result)
	{
		int dwRtn = GetLastError(); 
/*		tmp.Format(_T("������������ʧ��:%d"), dwRtn);*/
		if( dwRtn != ERROR_IO_PENDING && dwRtn != ERROR_SERVICE_ALREADY_RUNNING ) 
		{
/*			AfxMessageBox(tmp);*/
		}
		else
		{
/*			AfxMessageBox(_T("���������Ѿ������ˣ�"));*/
		}
	}
	else
	{
/*		AfxMessageBox(_T("�������������ɹ���"));*/
	}

	if (hService)
	{
		CloseServiceHandle(hService);
/*		AfxMessageBox(_T("�ر�hService"));*/
	}
	if (hSCManager)
	{
		CloseServiceHandle(hSCManager);
/*		AfxMessageBox(_T("�ر�hSCManager"));*/
	}
}
BOOL  StopDriver(SC_HANDLE hSCManager,LPCTSTR DriverId)
{
	SC_HANDLE		hService = NULL;
	BOOL			rCode = FALSE;
	SERVICE_STATUS	serviceStatus;
	DWORD		error = NO_ERROR;

	hService = OpenService(hSCManager, DriverId, SERVICE_ALL_ACCESS);

	if(hService != NULL)
	{
		rCode = ControlService(hService, SERVICE_CONTROL_STOP, &serviceStatus);
		if (!rCode)
		{
/*			AfxMessageBox(_T("ֹͣ����ʧ��"));*/
		}

		rCode = DeleteService(hService);
		if (!rCode)
		{
/*			AfxMessageBox(_T("ɾ������ʧ��"));*/
		}

		CloseServiceHandle(hService);
/*		AfxMessageBox(_T("�ر�hService"));*/
	}
	else
	{
/*		AfxMessageBox(_T("�򿪷���ʧ��"));*/
	}

	return rCode;
}
BOOL RemoveDriver(SC_HANDLE hSCManager, LPCTSTR DriverId)
{
	SC_HANDLE   hService = NULL;
	BOOL        rCode = FALSE;

	hService = OpenService(hSCManager, DriverId, SERVICE_ALL_ACCESS);
	if(hService == NULL)
	{
		rCode = TRUE;
	}
	else
	{
		rCode = DeleteService(hService);
		CloseServiceHandle(hService);
	}

	return rCode;
}

// CpermoDlg �Ի���

__int64 CompareFileTime(FILETIME time1, FILETIME time2)
{
	__int64 a = time1.dwHighDateTime << 32 | time1.dwLowDateTime;
	__int64 b = time2.dwHighDateTime << 32 | time2.dwLowDateTime;

	return   (b - a);
}

CpermoDlg::CpermoDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CpermoDlg::IDD, pParent)
	, nCPU(0)
	, nMem(0)
	, nTrans(255)
	, fNetUp(0.0)
	, fNetDown(0.0)
	, bTopmost(TRUE)
	, bShowBand(false)
	, bAutoHide(FALSE)
	, nShowWay(0)
	, _bMouseTrack(TRUE)
	, pcoControl(NULL)
	, bInfoDlgShowing(false)
	, nBandFontSize(16)
	, bLockWndPos(false)
	, nBandWidth(80)
	, nBandHeight(30)
	, bIsWndVisable(true)
	, bFullScreen(true)
	, bHideWndSides(false)
	, nNowBandShowIndex(0)
	, bBandShowCpu(false)
	, bBandShowMem(false)
	, bBandShowNetUp(false)
	, bBandShowNetDown(false)
	, nCount(0)
	, pLoc(NULL)
	, pSvc(NULL)
	, pEnumerator(NULL)
	, bBandShowDiskTem(false)
	, bHadWinpcap(false)
	, bIsWindowsVistaOrGreater(false)
	, bShowOneSideInfo(false)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

CpermoDlg::~CpermoDlg()
{
	delete pcoControl;
}

void CpermoDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CpermoDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_LBUTTONDOWN()
	ON_WM_TIMER()
	ON_COMMAND(IDM_TOPMOST, &CpermoDlg::OnTopmost)
	ON_WM_RBUTTONDOWN()
	ON_COMMAND(IDM_GREEN, &CpermoDlg::OnGreen)
	ON_COMMAND(IDM_BLUE, &CpermoDlg::OnBlue)
	ON_COMMAND(IDM_BLACK, &CpermoDlg::OnBlack)
	ON_COMMAND(IDM_RED, &CpermoDlg::OnRed)
	ON_COMMAND(IDM_ORANGE, &CpermoDlg::OnOrange)
	ON_COMMAND(IDM_EXIT, &CpermoDlg::OnExit)
	ON_MESSAGE(MSG_BAND_MENU,&CpermoDlg::OnBandMenu)
	ON_MESSAGE(WM_RECONNECT, &CpermoDlg::OnReconnect)
	//}}AFX_MSG_MAP
//	ON_WM_NCHITTEST()
	ON_WM_MOUSEHOVER()
	ON_WM_MOUSELEAVE()
	ON_WM_LBUTTONUP()
//	ON_WM_NCLBUTTONUP()
ON_WM_MOUSEMOVE()
ON_WM_ERASEBKGND()
END_MESSAGE_MAP()


// CpermoDlg ��Ϣ�������

BOOL CpermoDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// ���ô˶Ի����ͼ�ꡣ��Ӧ�ó��������ڲ��ǶԻ���ʱ����ܽ��Զ�
	//  ִ�д˲���
	SetIcon(m_hIcon, TRUE);			// ���ô�ͼ��
	SetIcon(m_hIcon, FALSE);		// ����Сͼ��

	// TODO: �ڴ���Ӷ���ĳ�ʼ������
// 	AllocConsole();
// 	freopen("CONOUT$","w",stdout);
	//bIsWindowsVistaOrGreater = false;
	//�жϲ���ϵͳ�汾
	get_processor_number();
	bIsWindowsVistaOrGreater = false;
 	DWORD dwVersion = 0;
 	DWORD dwMajorVersion = 0;
    DWORD dwMinorVersion = 0; 
 	dwVersion = ::GetVersion();
 	dwMajorVersion = (DWORD)(LOBYTE(LOWORD(dwVersion)));
     //dwMinorVersion = (DWORD)(HIBYTE(LOWORD(dwVersion)));
 	if (dwMajorVersion > 5)
 	{
 		bIsWindowsVistaOrGreater = true;
 	}
	/*
	If dwMajorVersion = 6 And dwMinorVersion = 1 Then GetWinVersion = "windows 7"
    If dwMajorVersion = 6 And dwMinorVersion = 0 Then GetWinVersion = "windows vista"
    If dwMajorVersion = 5 And dwMinorVersion = 1 Then GetWinVersion = "windows xp"
    If dwMajorVersion = 5 And dwMinorVersion = 0 Then GetWinVersion = "windows 2000"
	*/

	BOOL bRet = FALSE;
	::SystemParametersInfo(SPI_GETWORKAREA, 0, &rWorkArea, 0);   // ��ù�������С
	bRet = SetWorkDir();

	DeleteFiles();
	OpenConfig();
	InitSize();

	CreateInfoDlg();
	GetWindowRect(&rCurPos);
	
	InitializeCriticalSection(&_cs);

	//�����ļ�
	TCHAR direc[256];
	::GetCurrentDirectory(256, direc);//��ȡ��ǰĿ¼����
	CString dis;
	dis.Format(_T("%s"), direc);
	CString str32 = dis + _T("\\x32");
	CString str64 = dis + _T("\\x64");
	if (Is64BitSystem())
	{
		FileCopyTo(str64, dis, _T("WinRing0.sys"), TRUE);
	}
	else
	{
		FileCopyTo(str32, dis, _T("WinRing0.sys"), TRUE);
	}
	if (!::Initialize(1))
	{
		AfxMessageBox(_T("�ļ���ʧ"));
	}

	// Init Filter
	if( !filter.Init())
	{
		bHadWinpcap = false;

		TCHAR direc[256];
		::GetCurrentDirectory(256, direc);//��ȡ��ǰĿ¼����
		CString dis;
		dis.Format(_T("%s"), direc);
		CString str32 = dis + _T("\\x32");
		CString str64 = dis + _T("\\x64");
		if (Is64BitSystem())
		{
			FileCopyTo(str64, dis, _T("npf.sys"), TRUE);
			FileCopyTo(str64, dis, _T("wpcap.dll"), TRUE);
			FileCopyTo(str64, dis, _T("Packet.dll"), TRUE);
		}
		else
		{
			FileCopyTo(str32, dis, _T("npf.sys"), TRUE);
			FileCopyTo(str32, dis, _T("wpcap.dll"), TRUE);
			if (bIsWindowsVistaOrGreater)
			{
				FileCopyTo(str64, dis, _T("Packet.dll"), TRUE);
			}
			else
			{
				FileCopyTo(str32, dis, _T("Packet.dll"), TRUE);
			}
		}
		if (!::Initialize(0))
		{
			AfxMessageBox(_T("�ļ���ʧ"));
		}
	
		filter.Init();
	}
	else
	{
		bHadWinpcap = true;
	}
	// Find Devices
	g_nAdapters = filter.FindDevices();
	if( g_nAdapters <= 0 )
	{
		AfxMessageBox(_T("û�з��������������ӿڣ�������������ʧ��"));
	}
	// Get Device Names
	for(int i = 0; i < g_nAdapters; i++)
	{
		TCHAR *name = filter.GetName(i);

		// Save device name
		if( i < _countof(g_szAdapterNames))
		{
			_tcscpy_s(g_szAdapterNames[i], 256, name);
		}
	}
	// End
/*	filter.End();*/

	strNetDown.Format(_T("0.0KB/S"));
	strNetUp.Format(_T("0.0KB/S"));
	strCPU.Format(_T("0%%"));
	strMem.Format(_T("0%%"));

	if (IsIntel())
	{
		GetCpuTemp();
	}

	if (!::GetSystemTimes(&preidleTime, &prekernelTime, &preuserTime))
	{
		return -1;
	}
	m_SubMenu_NetPort.CreatePopupMenu();

	//��������������˵�
	for (int i = 0; i < g_nAdapters; i++)
	{
		m_SubMenu_NetPort.AppendMenu(MF_STRING, i + START_INDEX, g_szAdapterNames[i]);
	}
	if (g_iAdapter >= g_nAdapters)
	{
		g_iAdapter = 0;
	}
	//��ʼ��COM�ӿ�������ȡӲ���¶�
	hres = CoInitializeEx(0, COINIT_MULTITHREADED); 

	hres = CoInitializeSecurity(
		NULL, 
		-1, // COM authentication
		NULL, // Authentication services
		NULL, // Reserved
		RPC_C_AUTHN_LEVEL_DEFAULT, // Default authentication 
		RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation 
		NULL, // Authentication info
		EOAC_NONE, // Additional capabilities 
		NULL // Reserved
		);

	hres = CoCreateInstance(
		CLSID_WbemLocator, 
		0, 
		CLSCTX_INPROC_SERVER, 
		IID_IWbemLocator, (LPVOID *) &pLoc);

	hres = pLoc->ConnectServer(
		_bstr_t(L"ROOT\\WMI"), // Object path of WMI namespace
		NULL, // User name. NULL = current user
		NULL, // User password. NULL = current
		0, // Locale. NULL indicates current
		NULL, // Security flags.
		0, // Authority (e.g. Kerberos)
		0, // Context object 
		&pSvc // pointer to IWbemServices proxy
		);

	hres = CoSetProxyBlanket(
		pSvc, // Indicates the proxy to set
		RPC_C_AUTHN_WINNT, // RPC_C_AUTHN_xxx
		RPC_C_AUTHZ_NONE, // RPC_C_AUTHZ_xxx
		NULL, // Server principal name 
		RPC_C_AUTHN_LEVEL_CALL, // RPC_C_AUTHN_LEVEL_xxx 
		RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
		NULL, // client identity
		EOAC_NONE // proxy capabilities 
		);

	GetDiskTem();
	
	//�����˵�
	InitPopMenu(nCount);
	//Ĭ���ö�
	if (bTopmost)
	{
		m_Menu.CheckMenuItem(IDM_TOPMOST, MF_BYCOMMAND | MF_CHECKED);
	}
	if (bAutoHide)
	{
		m_Menu.CheckMenuItem(IDM_AUTOHIDE, MF_BYCOMMAND | MF_CHECKED);
	}
	if (bShowOneSideInfo)
	{
		m_Menu.CheckMenuItem(IDM_SHOWONESIDEINFO, MF_BYCOMMAND | MF_CHECKED);
	}
	m_Menu.CheckMenuItem(IDM_SHOWBYHOVER + nShowWay, MF_BYCOMMAND | MF_CHECKED);
	if (bShowNetInfo)
	{
		m_Menu.CheckMenuItem(IDM_SHOWNETINFO, MF_BYCOMMAND | MF_CHECKED);
	}
	else
	{
		m_Menu.CheckMenuItem(IDM_SHOWNETINFO, MF_BYCOMMAND | MF_UNCHECKED);
	}
	if (bShowTempInfo)
	{
		m_Menu.CheckMenuItem(IDM_SHOWTEMPINFO, MF_BYCOMMAND | MF_CHECKED);
	}
	else
	{
		m_Menu.CheckMenuItem(IDM_SHOWTEMPINFO, MF_BYCOMMAND | MF_UNCHECKED);
	}
	//��Ҫ��ʾ�������������д���
	if (bShowBand)
	{
		if (NULL == pcoControl)
		{
			pcoControl = new CNProgressBar(this);
			pcoControl->SetPosEx(0);
		}
		pcoControl->SetFontSize(nBandFontSize);
		SetBandWidth(nBandWidth);
		SetBandHeight(nBandHeight);
		pcoControl->Show(bShowBand);
		m_Menu.CheckMenuItem(IDM_SHOWBAND, MF_BYCOMMAND | MF_CHECKED);
	}
	else
	{
		m_Menu.CheckMenuItem(IDM_SHOWBAND, MF_BYCOMMAND | MF_UNCHECKED);
	}
	if (bFullScreen)
	{
		m_Menu.CheckMenuItem(IDM_FULLSCREEN, MF_BYCOMMAND | MF_CHECKED);
	}
	if (bLockWndPos)
	{
		m_Menu.CheckMenuItem(IDM_LOCKWNDPOS, MF_BYCOMMAND | MF_CHECKED);
	}
	if (bHideWndSides)
	{
		m_Menu.CheckMenuItem(IDM_HIDEWNDSIDES, MF_BYCOMMAND | MF_CHECKED);
	}
	if (bBandShowCpu)
	{
		m_BandMenu.CheckMenuItem(IDM_BANDSHOWCPU, MF_BYCOMMAND | MF_CHECKED);
		vBandShow.push_back(0);
	}
	if (bBandShowMem)
	{
		m_BandMenu.CheckMenuItem(IDM_BANDSHOWMEM, MF_BYCOMMAND | MF_CHECKED);
		vBandShow.push_back(1);
	}
	if (bBandShowNetDown)
	{
		m_BandMenu.CheckMenuItem(IDM_BANDSHOWNETDOWN, MF_BYCOMMAND | MF_CHECKED);
		vBandShow.push_back(2);
	}
	if (bBandShowNetUp)
	{
		m_BandMenu.CheckMenuItem(IDM_BANDSHOWNETUP, MF_BYCOMMAND | MF_CHECKED);
		vBandShow.push_back(3);
	}
	if (bBandShowDiskTem)
	{
		m_BandMenu.CheckMenuItem(IDM_BANDSHOWDISKTEM, MF_BYCOMMAND | MF_CHECKED);
		vBandShow.push_back(4);
	}
	if (bBandShowCpuTem)
	{
		m_BandMenu.CheckMenuItem(IDM_BANDSHOWCPUTEM, MF_BYCOMMAND | MF_CHECKED);
		vBandShow.push_back(5);
	}

	m_BandMenu.CheckMenuItem(IDM_BANDFONTSIZE12 + nBandFontSize - 12, MF_BYCOMMAND | MF_CHECKED);
	m_BandMenu.CheckMenuItem(IDM_BANDWIDTH50 + (nBandWidth-50)/10, MF_BYCOMMAND | MF_CHECKED);
	m_BandMenu.CheckMenuItem(IDM_BANDHEIGHT20 + (nBandHeight-20)/5, MF_BYCOMMAND | MF_CHECKED);
	m_Menu.CheckMenuItem(nSkin, MF_BYCOMMAND | MF_CHECKED); // ��ǰ��� 
	m_Menu.CheckMenuItem(IDM_FONTSIZE12 + nFontSize - 12, MF_BYCOMMAND | MF_CHECKED);
	IfAutoRun();//�ж��Ƿ��Ѿ���������

	//ȡ����������ʾ
	SetWindowLong(GetSafeHwnd(), GWL_EXSTYLE, WS_EX_TOOLWINDOW);

	//ÿ��һ��ˢ��CPU�������¶ȵ���Ϣ
	//���ö�ý�嶨ʱ��
	mm_Timer.CreateTimer((DWORD)this,1000,TimerCallbackTemp);
	//SetTimer(1, 1000, NULL);
	//ÿ��1.8����ȫ������
	SetTimer(2, 1800, NULL);
	//ÿ��2������������һ�¼��,��ֹ���ʧЧ,��ʱ��ô��
	SetTimer(3, 120000, NULL);
	
	::SetWindowLong( m_hWnd, GWL_EXSTYLE, GetWindowLong(m_hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
	::SetLayeredWindowAttributes( m_hWnd, 0, nTrans, LWA_ALPHA); // 120��͸���ȣ���Χ��0��255
	m_Menu.CheckMenuItem(IDM_TRANS0+(255-nTrans)/25, MF_BYCOMMAND | MF_CHECKED);
	
	pThis = this;

	if (bTopmost)
	{
		SetWindowPos(&wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	}

	StartCapture();

	return TRUE;  // ���ǽ��������õ��ؼ������򷵻� TRUE
}

// �����Ի��������С����ť������Ҫ����Ĵ���
//  �����Ƹ�ͼ�ꡣ����ʹ���ĵ�/��ͼģ�͵� MFC Ӧ�ó���
//  �⽫�ɿ���Զ���ɡ�

void CpermoDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // ���ڻ��Ƶ��豸������

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// ʹͼ���ڹ����������о���
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// ����ͼ��
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CPaintDC dc(this);
		//�����˸����,˫�����ͼ
		//���Ե�������һ���ڴ�DC��
		RECT rcClient;
		this->GetClientRect(&rcClient);
		CDC MemDC;
		CBitmap bitmap;
		MemDC.CreateCompatibleDC(&dc);
		bitmap.CreateCompatibleBitmap(&dc, rcClient.right - rcClient.left, rcClient.bottom - rcClient.top);
		MemDC.SelectObject(&bitmap);
		DrawBackground(&MemDC);
		DrawInfo(&MemDC);
		dc.BitBlt(0, 0, rcClient.right - rcClient.left, rcClient.bottom - rcClient.top, &MemDC,
			0, 0, SRCCOPY);
		bitmap.DeleteObject();
		MemDC.DeleteDC();
		CDialog::OnPaint();
	}
}

//���û��϶���С������ʱϵͳ���ô˺���ȡ�ù��
//��ʾ��
HCURSOR CpermoDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CpermoDlg::InitSize()
{
	if (-1 == rCurPos.left || -1 == rCurPos.top)
	{
		rCurPos.top = rWorkArea.bottom - 22;
		rCurPos.left = rWorkArea.right - 250;
	}
	if (bHideWndSides)
	{
		rCurPos.right = rCurPos.left + 150;
	}
	else
	{
		rCurPos.right = rCurPos.left + 220;
	}
	rCurPos.bottom = rCurPos.top + 22;
	MoveWindow(&rCurPos, TRUE);
}


void CpermoDlg::DrawBackground(CDC* pDC)
{
	CPen MyPen(PS_SOLID, 1, RGB(255, 255, 255));
	switch (nSkin)
	{
	case IDM_GREEN:
		{
			if (bHideWndSides)
			{
				CBrush MiBrush(RGB(150, 240, 150));
				CPen *pOldPen = pDC->SelectObject(&MyPen);
				CBrush *pOldBrush = pDC->SelectObject(&MiBrush);
				pDC->Rectangle(0, 0, 150, 22);
				pDC->SelectObject(pOldPen);
				pDC->SelectObject(pOldBrush);
			}
			else
			{
				CBrush RLBrush(RGB(2, 200, 20));
				CBrush MiBrush(RGB(150, 240, 150));
				CPen *pOldPen = pDC->SelectObject(&MyPen);
				CBrush *pOldBrush = pDC->SelectObject(&MiBrush);
				//���߿������
				pDC->Rectangle(0, 0, 220, 22);
				pDC->MoveTo(35, 0);
				pDC->LineTo(35, 22);
				pDC->MoveTo(185, 0);
				pDC->LineTo(185, 22);
				//ѡ�񲻴��߿�Ļ���
				pDC->SelectStockObject(NULL_PEN);
				pDC->SelectObject(&RLBrush);
				//������
				pDC->Rectangle(1, 1, 36, 22);
				//�Ҳ����
				pDC->Rectangle(186, 1, 220, 22);
				pDC->SelectObject(pOldPen);
				pDC->SelectObject(pOldBrush);
			}
		}
		break;
	case IDM_BLUE:
		{
			if (bHideWndSides)
			{
				CBrush MiBrush(RGB(66, 66, 66));
				CPen *pOldPen = pDC->SelectObject(&MyPen);
				CBrush *pOldBrush = pDC->SelectObject(&MiBrush);
				pDC->Rectangle(0, 0, 150, 22);
				pDC->SelectObject(pOldPen);
				pDC->SelectObject(pOldBrush);
			}
			else
			{
				CBrush RLBrush(RGB(26, 160, 225));
				CBrush MiBrush(RGB(66, 66, 66));
				CPen *pOldPen = pDC->SelectObject(&MyPen);
				CBrush *pOldBrush = pDC->SelectObject(&MiBrush);
				//���߿������
				pDC->Rectangle(0, 0, 220, 22);
				pDC->MoveTo(35, 0);
				pDC->LineTo(35, 22);
				pDC->MoveTo(185, 0);
				pDC->LineTo(185, 22);
				//ѡ�񲻴��߿�Ļ���
				pDC->SelectStockObject(NULL_PEN);
				pDC->SelectObject(&RLBrush);
				//������
				pDC->Rectangle(1, 1, 36, 22);
				//�Ҳ����
				pDC->Rectangle(186, 1, 220, 22);

				pDC->SelectObject(pOldPen);
				pDC->SelectObject(pOldBrush);
			}
		}
		break;
	case IDM_BLACK:
		{
			if (bHideWndSides)
			{
				CBrush MiBrush(RGB(100, 100, 100));
				CPen *pOldPen = pDC->SelectObject(&MyPen);
				CBrush *pOldBrush = pDC->SelectObject(&MiBrush);
				pDC->Rectangle(0, 0, 150, 22);
				pDC->SelectObject(pOldPen);
				pDC->SelectObject(pOldBrush);
			}
			else
			{
				CBrush RLBrush(RGB(50, 50, 50));
				CBrush MiBrush(RGB(100, 100, 100));
				CPen *pOldPen = pDC->SelectObject(&MyPen);
				CBrush *pOldBrush = pDC->SelectObject(&MiBrush);
				//���߿������
				pDC->Rectangle(0, 0, 220, 22);
				pDC->MoveTo(35, 0);
				pDC->LineTo(35, 22);
				pDC->MoveTo(185, 0);
				pDC->LineTo(185, 22);
				//ѡ�񲻴��߿�Ļ���
				pDC->SelectStockObject(NULL_PEN);
				pDC->SelectObject(&RLBrush);
				//������
				pDC->Rectangle(1, 1, 36, 22);
				//�Ҳ����
				pDC->Rectangle(186, 1, 220, 22);

				pDC->SelectObject(pOldPen);
				pDC->SelectObject(pOldBrush);
			}
		}
		break;
	case IDM_RED:
		{
			if (bHideWndSides)
			{
				CBrush MiBrush(RGB(240, 150, 150));
				CPen *pOldPen = pDC->SelectObject(&MyPen);
				CBrush *pOldBrush = pDC->SelectObject(&MiBrush);
				pDC->Rectangle(0, 0, 150, 22);
				pDC->SelectObject(pOldPen);
				pDC->SelectObject(pOldBrush);
			}
			else
			{
				CBrush RLBrush(RGB(180, 20, 20));
				CBrush MiBrush(RGB(240, 150, 150));
				CPen *pOldPen = pDC->SelectObject(&MyPen);
				CBrush *pOldBrush = pDC->SelectObject(&MiBrush);
				//���߿������
				pDC->Rectangle(0, 0, 220, 22);
				pDC->MoveTo(35, 0);
				pDC->LineTo(35, 22);
				pDC->MoveTo(185, 0);
				pDC->LineTo(185, 22);
				//ѡ�񲻴��߿�Ļ���
				pDC->SelectStockObject(NULL_PEN);
				pDC->SelectObject(&RLBrush);
				//������
				pDC->Rectangle(1, 1, 36, 22);
				//�Ҳ����
				pDC->Rectangle(186, 1, 220, 22);

				pDC->SelectObject(pOldPen);
				pDC->SelectObject(pOldBrush);
			}
		}
		break;
	case IDM_ORANGE:
		{
			if (bHideWndSides)
			{
				CBrush MiBrush(RGB(250, 230, 20));
				CPen *pOldPen = pDC->SelectObject(&MyPen);
				CBrush *pOldBrush = pDC->SelectObject(&MiBrush);
				pDC->Rectangle(0, 0, 150, 22);
				pDC->SelectObject(pOldPen);
				pDC->SelectObject(pOldBrush);
			}
			else
			{
				CBrush RLBrush(RGB(230, 100, 25));
				CBrush MiBrush(RGB(250, 230, 20));
				CPen *pOldPen = pDC->SelectObject(&MyPen);
				CBrush *pOldBrush = pDC->SelectObject(&MiBrush);
				//���߿������
				pDC->Rectangle(0, 0, 220, 22);
				pDC->MoveTo(35, 0);
				pDC->LineTo(35, 22);
				pDC->MoveTo(185, 0);
				pDC->LineTo(185, 22);
				//ѡ�񲻴��߿�Ļ���
				pDC->SelectStockObject(NULL_PEN);
				pDC->SelectObject(&RLBrush);
				//������
				pDC->Rectangle(1, 1, 36, 22);
				//�Ҳ����
				pDC->Rectangle(186, 1, 220, 22);
				pDC->SelectObject(pOldPen);
				pDC->SelectObject(pOldBrush);
			}
		}
		break;
	default:
		{
			if (bHideWndSides)
			{
				CBrush MiBrush(RGB(150, 240, 150));
				CPen *pOldPen = pDC->SelectObject(&MyPen);
				CBrush *pOldBrush = pDC->SelectObject(&MiBrush);
				pDC->Rectangle(0, 0, 150, 22);
				pDC->SelectObject(pOldPen);
				pDC->SelectObject(pOldBrush);
			}
			else
			{
				CBrush RLBrush(RGB(2, 200, 20));
				CBrush MiBrush(RGB(150, 240, 150));
				CPen *pOldPen = pDC->SelectObject(&MyPen);
				CBrush *pOldBrush = pDC->SelectObject(&MiBrush);
				//���߿������
				pDC->Rectangle(0, 0, 220, 22);
				pDC->MoveTo(35, 0);
				pDC->LineTo(35, 22);
				pDC->MoveTo(185, 0);
				pDC->LineTo(185, 22);
				//ѡ�񲻴��߿�Ļ���
				pDC->SelectStockObject(NULL_PEN);
				pDC->SelectObject(&RLBrush);
				//������
				pDC->Rectangle(1, 1, 36, 22);
				//�Ҳ����
				pDC->Rectangle(186, 1, 220, 22);
				pDC->SelectObject(pOldPen);
				pDC->SelectObject(pOldBrush);
			}
		}
		break;
	}
	//����������Բ�Ҫ,�ֲ��������������
	// 	WhitePen.DeleteObject();
	// 	GreenBrush.DeleteObject();
	// 	WhiteBrush.DeleteObject();
}

void CpermoDlg::DrawInfo(CDC* pDC)
{
	CFont font, *pOldFont;
	LOGFONT logFont;
	pDC->GetCurrentFont()->GetLogFont(&logFont);
	logFont.lfWidth = 0;
	logFont.lfHeight = nFontSize;
	logFont.lfWeight = FW_REGULAR;
	lstrcpy(logFont.lfFaceName, _T("΢���ź�"));
	font.CreateFontIndirect(&logFont);
	pOldFont = pDC->SelectObject(&font);
	COLORREF cOldTextColor;
	COLORREF cTempTextColor;
	if (IDM_GREEN == nSkin || IDM_ORANGE == nSkin)
	{
		cOldTextColor = pDC->SetTextColor(RGB(0, 0, 0));
	}
	else
	{
		cOldTextColor = pDC->SetTextColor(RGB(255, 255, 255));
	}
	int nOldBkMode = pDC->SetBkMode(TRANSPARENT);
	CRect rText;
	CRect rcIcon;
	if (bHideWndSides)
	{
		if (IDM_BLACK == nSkin || IDM_BLUE == nSkin)
		{
			pDC->SetTextColor(RGB(255, 255, 255));
		}
		else
		{
			pDC->SetTextColor(RGB(0, 0, 0));
		}
		rcIcon.left = 3;
		rcIcon.right = 15;
		rcIcon.top = 5;
		rcIcon.bottom = 17;
		DrawIconEx(pDC->GetSafeHdc(), rcIcon.left, rcIcon.top, LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_DOWN)), rcIcon.Width(), rcIcon.Height(), 0, NULL, DI_NORMAL);
		rText.top = 2;
		rText.bottom = 21;
		rText.left = 17;
		rText.right = 75;
		pDC->DrawText(strNetDown, &rText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		rcIcon.left = 77;
		rcIcon.right = 89;
		DrawIconEx(pDC->GetSafeHdc(), rcIcon.left, rcIcon.top, LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_UP)), rcIcon.Width(), rcIcon.Height(), 0, NULL, DI_NORMAL);
		rText.left = 91;
		rText.right = 149;
		pDC->DrawText(strNetUp, &rText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	}
	else
	{
		rText.left = 1;
		rText.right = 36;
		rText.top = 2;
		rText.bottom = 21;
		if (nCPU > 70)
		{
			cTempTextColor = pDC->SetTextColor(RGB(250, 180, 50));
		}
		pDC->DrawText(strCPU, &rText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		rText.left = 186;
		rText.right = 219;
		if (nMem > 70 && nCPU <= 70)
		{
			pDC->SetTextColor(RGB(250, 180, 50));
		}
		else if (nMem <= 10 && nCPU > 10)
		{
			pDC->SetTextColor(cTempTextColor);
		}
		pDC->DrawText(strMem, &rText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		if (IDM_BLACK == nSkin || IDM_BLUE == nSkin)
		{
			pDC->SetTextColor(RGB(255, 255, 255));
		}
		else
		{
			pDC->SetTextColor(RGB(0, 0, 0));
		}
		rcIcon.left = 38;
		rcIcon.right = 50;
		rcIcon.top = 5;
		rcIcon.bottom = 17;
		DrawIconEx(pDC->GetSafeHdc(), rcIcon.left, rcIcon.top, LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_DOWN)), rcIcon.Width(), rcIcon.Height(), 0, NULL, DI_NORMAL);
		rText.left = 52;
		rText.right = 110;
		pDC->DrawText(strNetDown, &rText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		rcIcon.left = 112;
		rcIcon.right = 124;
		DrawIconEx(pDC->GetSafeHdc(), rcIcon.left, rcIcon.top, LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_UP)), rcIcon.Width(), rcIcon.Height(), 0, NULL, DI_NORMAL);
		rText.left = 126;
		rText.right = 185;
		pDC->DrawText(strNetUp, &rText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	}

	pDC->SetTextColor(cOldTextColor);
	pDC->SetBkMode(nOldBkMode);
	pDC->SelectObject(pOldFont);
	font.DeleteObject();
}

void CpermoDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
	// TODO:  �ڴ������Ϣ�����������/�����Ĭ��ֵ
	//PostMessage(WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(point.x, point.y));
	SetCapture();
	mm_Timer.KillTimer();
	CDialog::OnLButtonDown(nFlags, point);
}


void CpermoDlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO:  �ڴ������Ϣ�����������/�����Ĭ��ֵ
	if (2 == nIDEvent)
	{
		if (bFullScreen)
		{
			bool bFullScreen = IsForegroundFullscreen();
			if (bFullScreen)
			{
				if (bIsWndVisable)
				{
					ShowWindow(SW_HIDE);
				}
			}
			else
			{
				if ((!IsWindowVisible()) && bIsWndVisable)
				{
					ShowWindow(SW_SHOW);
					if (bTopmost)
					{
						SetWindowPos(&wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
					}
				}
			}
		}
	}
 	if (3 == nIDEvent)
 	{
 		StopCapture();
 		StartCapture();
 	}
	CDialog::OnTimer(nIDEvent);
}


void CpermoDlg::OnTopmost()
{
	// TODO:  �ڴ���������������
	if (bTopmost)
	{
		SetWindowPos(&wndNoTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		bTopmost = FALSE;
		m_Menu.CheckMenuItem(IDM_TOPMOST, MF_BYCOMMAND | MF_UNCHECKED);
	}
	else
	{
		SetWindowPos(&wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		bTopmost = TRUE;
		m_Menu.CheckMenuItem(IDM_TOPMOST, MF_BYCOMMAND | MF_CHECKED); // ��ǰ��� 
	}
}

void CpermoDlg::OnRButtonDown(UINT nFlags, CPoint point)
{
	int nID = 0;
	// TODO:  �ڴ������Ϣ�����������/�����Ĭ��ֵ
	CPoint p;
	//���ݹ���������Ϊ����ڴ������Ͻǵ����꣬WM_CONTEXTMENU���ݹ���������Ļ����
	GetCursorPos(&p);//�������Ļ����
	m_Menu.CheckMenuItem(g_iAdapter + START_INDEX, MF_BYCOMMAND | MF_CHECKED); 
	nID = m_Menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RETURNCMD, p.x, p.y, this);
	switch (nID)
	{
	case IDM_TOPMOST:
		OnTopmost();
		break;
	case IDM_GREEN:
		OnGreen();
		break;
	case IDM_BLUE:
		OnBlue();
		break;
	case IDM_BLACK:
		OnBlack();
		break;
	case IDM_RED:
		OnRed();
		break;
	case IDM_ORANGE:
		OnOrange();
		break;
	case IDM_EXIT:
		OnExit();
		break;
	case IDM_AUTOHIDE:
		OnAutoHide();
		break;
	case IDM_SHOWBYHOVER:
		SetShowWay(0);
		break;
	case IDM_SHOWBYLDOWN:
		SetShowWay(1);
		break;
	case IDM_SHOWNEVER:
		SetShowWay(2);
		break;
	case IDM_LOCKWNDPOS:
		{
			if (bLockWndPos)
			{
				m_Menu.CheckMenuItem(IDM_LOCKWNDPOS, MF_BYCOMMAND | MF_UNCHECKED);
				bLockWndPos = false;
			}
			else
			{
				m_Menu.CheckMenuItem(IDM_LOCKWNDPOS, MF_BYCOMMAND | MF_CHECKED);
				bLockWndPos = true;
			}
		}
		break;
	case IDM_TRANS0:
		OnTrans0();
		break;
	case IDM_TRANS10:
		OnTrans10();
		break;
	case IDM_TRANS20:
		OnTrans20();
		break;
	case IDM_TRANS30:
		OnTrans30();
		break;
	case IDM_TRANS40:
		OnTrans40();
		break;
	case IDM_TRANS50:
		OnTrans50();
		break;
	case IDM_TRANS60:
		OnTrans60();
		break;
	case IDM_TRANS70:
		OnTrans70();
		break;
	case IDM_TRANS80:
		OnTrans80();
		break;
	case IDM_TRANS90:
		OnTrans90();
		break;
	case IDM_FONTSIZE12:
		SetFontSize(12);
		break;
	case IDM_FONTSIZE13:
		SetFontSize(13);
		break;
	case IDM_FONTSIZE14:
		SetFontSize(14);
		break;
	case IDM_FONTSIZE15:
		SetFontSize(15);
		break;
	case IDM_FONTSIZE16:
		SetFontSize(16);
		break;
	case IDM_FONTSIZE17:
		SetFontSize(17);
		break;
	case IDM_FONTSIZE18:
		SetFontSize(18);
		break;
	case IDM_FULLSCREEN:
		{
			if (bFullScreen)
			{
				m_Menu.CheckMenuItem(IDM_FULLSCREEN, MF_BYCOMMAND | MF_UNCHECKED);
				bFullScreen = false;
			}
			else
			{
				m_Menu.CheckMenuItem(IDM_FULLSCREEN, MF_BYCOMMAND | MF_CHECKED);
				bFullScreen = true;
			}
		}
		break;
	case IDM_AUTOSTART:
		SetAutoRun();
		break;
	case IDM_SHOWNETINFO:
		ShowNetInfo();
		break;
	case IDM_SHOWTEMPINFO:
		ShowTempInfo();
		break;
	case IDM_SHOWBAND:
		ShowBand();
		break;
	case IDM_HIDEWNDSIDES:
		{
			if (bHideWndSides)
			{
				rCurPos.right = rCurPos.left + 220;
				MoveWindow(&rCurPos, TRUE);
				m_Menu.CheckMenuItem(IDM_HIDEWNDSIDES, MF_BYCOMMAND | MF_UNCHECKED);
				bHideWndSides = false;
				MoveInfoDlg();
			}
			else
			{
				rCurPos.right = rCurPos.left + 150;
				MoveWindow(&rCurPos, TRUE);
				m_Menu.CheckMenuItem(IDM_HIDEWNDSIDES, MF_BYCOMMAND | MF_CHECKED);
				bHideWndSides = true;
				MoveInfoDlg();
			}
		}
		break;
	case IDM_SHOWONESIDEINFO:
		{
			if (bShowOneSideInfo)
			{
				m_Menu.CheckMenuItem(IDM_SHOWONESIDEINFO, MF_BYCOMMAND | MF_UNCHECKED);
				bShowOneSideInfo = false;
			}
			else
			{
				m_Menu.CheckMenuItem(IDM_SHOWONESIDEINFO, MF_BYCOMMAND | MF_CHECKED);
				bShowOneSideInfo = true;
			}
		}
		break;
	case 0:
		return;
	default:
		{m_Menu.CheckMenuItem(g_iAdapter + START_INDEX, MF_BYCOMMAND | MF_UNCHECKED);
		g_iAdapter = nID - START_INDEX; StopCapture(); StartCapture();}
		break;
	}

	CDialog::OnRButtonDown(nFlags, point);
}


void CpermoDlg::OnGreen()
{
	// TODO:  �ڴ���������������
	if (IDM_GREEN == nSkin)
	{
		return;
	}
	m_Menu.CheckMenuItem(nSkin, MF_BYCOMMAND | MF_UNCHECKED);
	nSkin = IDM_GREEN;
	m_Menu.CheckMenuItem(nSkin, MF_BYCOMMAND | MF_CHECKED);
	Invalidate(FALSE);
}


void CpermoDlg::OnBlue()
{
	// TODO:  �ڴ���������������
	if (IDM_BLUE == nSkin)
	{
		return;
	}
	m_Menu.CheckMenuItem(nSkin, MF_BYCOMMAND | MF_UNCHECKED);
	nSkin = IDM_BLUE;
	m_Menu.CheckMenuItem(nSkin, MF_BYCOMMAND | MF_CHECKED);
	Invalidate(FALSE);
}


void CpermoDlg::OnBlack()
{
	// TODO:  �ڴ���������������
	if (IDM_BLACK == nSkin)
	{
		return;
	}
	m_Menu.CheckMenuItem(nSkin, MF_BYCOMMAND | MF_UNCHECKED);
	nSkin = IDM_BLACK;
	m_Menu.CheckMenuItem(nSkin, MF_BYCOMMAND | MF_CHECKED);
	Invalidate(FALSE);
}


void CpermoDlg::OnRed()
{
	// TODO:  �ڴ���������������
	if (IDM_RED == nSkin)
	{
		return;
	}
	m_Menu.CheckMenuItem(nSkin, MF_BYCOMMAND | MF_UNCHECKED);
	nSkin = IDM_RED;
	m_Menu.CheckMenuItem(nSkin, MF_BYCOMMAND | MF_CHECKED);
	Invalidate(FALSE);
}


void CpermoDlg::OnOrange()
{
	// TODO:  �ڴ���������������
	if (IDM_ORANGE == nSkin)
	{
		return;
	}
	m_Menu.CheckMenuItem(nSkin, MF_BYCOMMAND | MF_UNCHECKED);
	nSkin = IDM_ORANGE;
	m_Menu.CheckMenuItem(nSkin, MF_BYCOMMAND | MF_CHECKED);
	Invalidate(FALSE);
}


void CpermoDlg::OnExit()
{
	// TODO:  �ڴ���������������
	mm_Timer.KillTimer();
	KillTimer(2);
	KillTimer(3);
	pInfoDlg->KillTimer(1);

	if (g_bCapture)
	{
		StopCapture();
	}
	filter.End();

	if (!SaveConfig())
	{
		AfxMessageBox(_T("��...������Ϣ����ʧ�ܣ�"));
	}
	
	Remove();
	DeleteFiles();
	OnOK();
}


void CpermoDlg::InitPopMenu(int nCount)
{
	BOOL bRet = m_Menu.CreatePopupMenu();
	ASSERT(bRet);
	bRet = m_SubMenu_Skin.CreatePopupMenu();
	ASSERT(bRet);
	bRet = m_SubMenu_Trans.CreatePopupMenu();
	ASSERT(bRet);
	bRet = m_SubMenu_ShowWay.CreatePopupMenu();
	ASSERT(bRet);
	bRet = m_SubMenu_FontSize.CreatePopupMenu();
	ASSERT(bRet);
	m_Menu.AppendMenu(MF_BYCOMMAND, IDM_TOPMOST, _T("�����������ö�"));
	m_Menu.AppendMenu(MF_BYCOMMAND, IDM_AUTOHIDE, _T("������������������"));
	m_Menu.AppendMenu(MF_BYCOMMAND, IDM_SHOWONESIDEINFO, _T("����������ʾһ����Ϣ"));
	m_Menu.AppendMenu(MF_BYPOSITION | MF_SEPARATOR);
	m_Menu.AppendMenu(MF_BYCOMMAND, IDM_SHOWNETINFO, _T("��ʾ���������Ϣ"));
	m_Menu.AppendMenu(MF_BYCOMMAND, IDM_SHOWTEMPINFO, _T("��ʾ�¶ȼ����Ϣ"));
	m_Menu.AppendMenu(MF_BYCOMMAND, IDM_SHOWBAND, _T("��ʾ���������"));
	m_Menu.AppendMenu(MF_BYCOMMAND, IDM_LOCKWNDPOS, _T("����������λ��"));
	m_Menu.AppendMenu(MF_BYPOSITION | MF_SEPARATOR);
	m_SubMenu_Skin.AppendMenu(MF_BYCOMMAND, IDM_GREEN, _T("��ɫ"));
	m_SubMenu_Skin.AppendMenu(MF_BYCOMMAND, IDM_BLUE, _T("��ɫ"));
	m_SubMenu_Skin.AppendMenu(MF_BYCOMMAND, IDM_BLACK, _T("��ɫ"));
	m_SubMenu_Skin.AppendMenu(MF_BYCOMMAND, IDM_RED, _T("��ɫ"));
	m_SubMenu_Skin.AppendMenu(MF_BYCOMMAND, IDM_ORANGE, _T("��ɫ"));
	m_Menu.AppendMenu(MF_BYPOSITION | MF_POPUP,
		(UINT)m_SubMenu_Skin.m_hMenu, _T("����Ƥ����ɫ"));
	m_SubMenu_FontSize.AppendMenu(MF_BYCOMMAND, IDM_FONTSIZE12, _T("12"));
	m_SubMenu_FontSize.AppendMenu(MF_BYCOMMAND, IDM_FONTSIZE13, _T("13"));
	m_SubMenu_FontSize.AppendMenu(MF_BYCOMMAND, IDM_FONTSIZE14, _T("14"));
	m_SubMenu_FontSize.AppendMenu(MF_BYCOMMAND, IDM_FONTSIZE15, _T("15"));
	m_SubMenu_FontSize.AppendMenu(MF_BYCOMMAND, IDM_FONTSIZE16, _T("16"));
	m_SubMenu_FontSize.AppendMenu(MF_BYCOMMAND, IDM_FONTSIZE17, _T("17"));
	m_SubMenu_FontSize.AppendMenu(MF_BYCOMMAND, IDM_FONTSIZE18, _T("18"));
	m_Menu.AppendMenu(MF_BYPOSITION | MF_POPUP,
		(UINT)m_SubMenu_FontSize.m_hMenu, _T("���������С"));
	m_SubMenu_Trans.AppendMenu(MF_BYCOMMAND, IDM_TRANS0, _T("��͸��"));
	m_SubMenu_Trans.AppendMenu(MF_BYCOMMAND, IDM_TRANS10, _T("10%"));
	m_SubMenu_Trans.AppendMenu(MF_BYCOMMAND, IDM_TRANS20, _T("20%"));
	m_SubMenu_Trans.AppendMenu(MF_BYCOMMAND, IDM_TRANS30, _T("30%"));
	m_SubMenu_Trans.AppendMenu(MF_BYCOMMAND, IDM_TRANS40, _T("40%"));
	m_SubMenu_Trans.AppendMenu(MF_BYCOMMAND, IDM_TRANS50, _T("50%"));
	m_SubMenu_Trans.AppendMenu(MF_BYCOMMAND, IDM_TRANS60, _T("60%"));
	m_SubMenu_Trans.AppendMenu(MF_BYCOMMAND, IDM_TRANS70, _T("70%"));
	m_SubMenu_Trans.AppendMenu(MF_BYCOMMAND, IDM_TRANS80, _T("80%"));
	m_SubMenu_Trans.AppendMenu(MF_BYCOMMAND, IDM_TRANS90, _T("90%"));
	m_Menu.AppendMenu(MF_BYPOSITION | MF_POPUP,
		(UINT)m_SubMenu_Trans.m_hMenu, _T("������͸��������"));
	m_Menu.AppendMenu(MF_BYCOMMAND, IDM_HIDEWNDSIDES, _T("����������������Ϣ"));
	m_Menu.AppendMenu(MF_BYPOSITION | MF_SEPARATOR);
	m_Menu.AppendMenu(MF_BYPOSITION | MF_POPUP,
		(UINT)m_SubMenu_NetPort.m_hMenu, _T("ѡ����Ҫ��ص�����ӿ�"));
	m_SubMenu_ShowWay.AppendMenu(MF_BYCOMMAND, IDM_SHOWBYHOVER, _T("��껬����������"));
	m_SubMenu_ShowWay.AppendMenu(MF_BYCOMMAND, IDM_SHOWBYLDOWN, _T("���������"));
	m_SubMenu_ShowWay.AppendMenu(MF_BYCOMMAND, IDM_SHOWNEVER, _T("�Ӳ�����"));
	m_Menu.AppendMenu(MF_BYPOSITION | MF_POPUP,
		(UINT)m_SubMenu_ShowWay.m_hMenu, _T("��ϸ���ڵ�����ʽ"));
	m_Menu.AppendMenu(MF_BYCOMMAND, IDM_FULLSCREEN, _T("ȫ�������"));
	m_Menu.AppendMenu(MF_BYCOMMAND, IDM_AUTOSTART, _T("������������"));

	m_Menu.AppendMenu(MF_BYCOMMAND, IDM_EXIT, _T("�˳�"));

	bRet = m_BandMenu.CreatePopupMenu();
	ASSERT(bRet);
	bRet = m_BandFontSizeMenu.CreatePopupMenu();
	ASSERT(bRet);
	bRet = m_BandWidthMenu.CreatePopupMenu();
	ASSERT(bRet);
	bRet = m_BandHeightMenu.CreatePopupMenu();
	ASSERT(bRet);
	m_BandMenu.AppendMenu(MF_BYCOMMAND, IDM_BANDSHOWCPU, _T("��ʾCPUռ��"));
	m_BandMenu.AppendMenu(MF_BYCOMMAND, IDM_BANDSHOWMEM, _T("��ʾ�ڴ�ռ��"));
	m_BandMenu.AppendMenu(MF_BYCOMMAND, IDM_BANDSHOWNETDOWN, _T("��ʾ�����ٶ�"));
	m_BandMenu.AppendMenu(MF_BYCOMMAND, IDM_BANDSHOWNETUP, _T("��ʾ�ϴ��ٶ�"));
	m_BandMenu.AppendMenu(MF_BYCOMMAND, IDM_BANDSHOWDISKTEM, _T("��ʾӲ���¶�"));
	if (gIsMsr)
	{
		m_BandMenu.AppendMenu(MF_BYCOMMAND, IDM_BANDSHOWCPUTEM, _T("��ʾCPU�¶�"));
	}
	m_BandMenu.AppendMenu(MF_BYPOSITION | MF_SEPARATOR);
	m_BandFontSizeMenu.AppendMenu(MF_BYCOMMAND, IDM_BANDFONTSIZE12, _T("12"));
	m_BandFontSizeMenu.AppendMenu(MF_BYCOMMAND, IDM_BANDFONTSIZE13, _T("13"));
	m_BandFontSizeMenu.AppendMenu(MF_BYCOMMAND, IDM_BANDFONTSIZE14, _T("14"));
	m_BandFontSizeMenu.AppendMenu(MF_BYCOMMAND, IDM_BANDFONTSIZE15, _T("15"));
	m_BandFontSizeMenu.AppendMenu(MF_BYCOMMAND, IDM_BANDFONTSIZE16, _T("16"));
	m_BandFontSizeMenu.AppendMenu(MF_BYCOMMAND, IDM_BANDFONTSIZE17, _T("17"));
	m_BandMenu.AppendMenu(MF_BYPOSITION | MF_POPUP,
		(UINT)m_BandFontSizeMenu.m_hMenu, _T("��������С����"));
	m_BandWidthMenu.AppendMenu(MF_BYCOMMAND, IDM_BANDWIDTH50, _T("50"));
	m_BandWidthMenu.AppendMenu(MF_BYCOMMAND, IDM_BANDWIDTH60, _T("60"));
	m_BandWidthMenu.AppendMenu(MF_BYCOMMAND, IDM_BANDWIDTH70, _T("70"));
	m_BandWidthMenu.AppendMenu(MF_BYCOMMAND, IDM_BANDWIDTH80, _T("80(Ĭ��)"));
	m_BandMenu.AppendMenu(MF_BYPOSITION | MF_POPUP,
		(UINT)m_BandWidthMenu.m_hMenu, _T("��߿������"));
	m_BandHeightMenu.AppendMenu(MF_BYCOMMAND, IDM_BANDHEIGHT20, _T("20"));
	m_BandHeightMenu.AppendMenu(MF_BYCOMMAND, IDM_BANDHEIGHT25, _T("25"));
	m_BandHeightMenu.AppendMenu(MF_BYCOMMAND, IDM_BANDHEIGHT30, _T("30(Ĭ��)"));
	m_BandHeightMenu.AppendMenu(MF_BYCOMMAND, IDM_BANDHEIGHT35, _T("35"));
	m_BandMenu.AppendMenu(MF_BYPOSITION | MF_POPUP,
		(UINT)m_BandHeightMenu.m_hMenu, _T("��߸߶�����"));
	m_BandMenu.AppendMenu(MF_BYPOSITION | MF_SEPARATOR);
	m_BandMenu.AppendMenu(MF_BYCOMMAND, IDM_SHOWHIDEWND, _T("����/��ʾ������"));
	m_BandMenu.AppendMenu(MF_BYCOMMAND, IDM_EXIT, _T("�˳�"));
}


void CpermoDlg::OpenConfig()
{
	TCHAR direc[256];
	::GetCurrentDirectory(256, direc);//��ȡ��ǰĿ¼����
	TCHAR temp[256];
	wsprintf(temp, _T("%s\\config.ini"), direc);
	int left, top, topmost, skin, autohide, trans, showway, shownetinfo, 
		showtempinfo, fontsize, showband, bandfontsize, bandwidth, bandheight,
		fullscreen, lockwndpos, hidewndsides, bandshowcpu, bandshowmem,
		bandshownetup, bandshownetdown, bandshowdisktem, bandshowcputem,
		adapter, showonesideinfo;
	left = ::GetPrivateProfileInt(_T("Main"), _T("left"), -1, temp);
	top = ::GetPrivateProfileInt(_T("Main"), _T("top"), -1, temp);
	topmost = ::GetPrivateProfileInt(_T("Main"), _T("topmost"), -1, temp);
	skin = ::GetPrivateProfileInt(_T("Main"), _T("skin"), -1, temp);
	autohide = ::GetPrivateProfileInt(_T("Main"), _T("autohide"), -1, temp);
	trans = ::GetPrivateProfileInt(_T("Main"), _T("trans"), -1, temp);
	showway = ::GetPrivateProfileInt(_T("Main"), _T("showway"), -1, temp);
	shownetinfo = ::GetPrivateProfileInt(_T("Main"), _T("shownetinfo"), -1, temp);
	showtempinfo = ::GetPrivateProfileInt(_T("Main"), _T("showtempinfo"), -1, temp);
	fontsize = ::GetPrivateProfileInt(_T("Main"), _T("fontsize"), -1, temp);
	showband = ::GetPrivateProfileInt(_T("Main"), _T("showband"), -1, temp);
	bandfontsize = ::GetPrivateProfileInt(_T("Main"), _T("bandfontsize"), -1, temp);
	bandwidth = ::GetPrivateProfileInt(_T("Main"), _T("bandwidth"), -1, temp);
	bandheight = ::GetPrivateProfileInt(_T("Main"), _T("bandheight"), -1, temp);
	fullscreen = ::GetPrivateProfileInt(_T("Main"), _T("fullscreen"), -1, temp);
	lockwndpos = ::GetPrivateProfileInt(_T("Main"), _T("lockwndpos"), -1, temp);
	hidewndsides = ::GetPrivateProfileInt(_T("Main"), _T("hidewndsides"), -1, temp);
	bandshowcpu = ::GetPrivateProfileInt(_T("Main"), _T("bandshowcpu"), -1, temp);
	bandshowmem = ::GetPrivateProfileInt(_T("Main"), _T("bandshowmem"), -1, temp);
	bandshownetup = ::GetPrivateProfileInt(_T("Main"), _T("bandshownetup"), -1, temp);
	bandshownetdown = ::GetPrivateProfileInt(_T("Main"), _T("bandshownetdown"), -1, temp);
	bandshowdisktem = ::GetPrivateProfileInt(_T("Main"), _T("bandshowdisktem"), -1, temp);
	bandshowcputem = ::GetPrivateProfileInt(_T("Main"), _T("bandshowcputem"), -1, temp);
	adapter = ::GetPrivateProfileInt(_T("Main"), _T("adapter"), -1, temp);
	showonesideinfo = ::GetPrivateProfileInt(_T("Main"), _T("showonesideinfo"), -1, temp);
	//��ȡ��������ȷ�����ݽ��и�ֵ���������Ĭ����ֵ
	if (-1 == left || left < 0)
	{
		rCurPos.left = -1;
	}
	else
	{
		rCurPos.left = left;
	}
	if (-1 == top || top < 0)
	{
		rCurPos.top = -1;
	}
	else
	{
		rCurPos.top = top;
	}
	if (0 == topmost)
	{
		bTopmost = FALSE;
	}
	else
	{
		bTopmost = TRUE;
	}
	if (skin != IDM_GREEN && skin != IDM_BLUE && skin != IDM_BLACK
		&& skin != IDM_RED && skin != IDM_ORANGE)
	{
		nSkin = IDM_BLUE;
	}
	else
	{
		nSkin = skin;
	}
	if (1 == autohide)
	{
		bAutoHide = TRUE;
	}
	else
	{
		bAutoHide = FALSE;
	}
	if (trans < 0 || trans > 255)
	{
		nTrans = 255;
	}
	else
	{
		nTrans = trans;
	}
	if (-1 == showway || showway > 2)
	{
		nShowWay = 0;
	}
	else
	{
		nShowWay = showway;
	}
	if (0 == shownetinfo)
	{
		bShowNetInfo = false;
	}
	else
	{
		bShowNetInfo = true;
	}
	if (1 == showtempinfo)
	{
		bShowTempInfo = true;
	}
	else
	{
		bShowTempInfo = false;
	}
	if (-1 == fontsize || fontsize < 12 || fontsize > 18)
	{
		nFontSize = 18;
	}
	else
	{
		nFontSize = fontsize;
	}
	if (1 == showband)
	{
		bShowBand = true;
	}
	else
	{
		bShowBand = false;
	}
	if (-1 == bandfontsize || bandfontsize < 12 || bandfontsize > 17)
	{
		nBandFontSize = 16;
	}
	else
	{
		nBandFontSize = bandfontsize;
	}
	if (-1 == bandwidth || bandwidth<50 || bandwidth>80)
	{
		nBandWidth= 80;
	}
	else
	{
		nBandWidth = bandwidth;
	}
	if (-1 == bandheight || bandheight<20 || bandheight>35)
	{
		nBandHeight= 30;
	}
	else
	{
		nBandHeight = bandheight;
	}
	if (0 == fullscreen)
	{
		bFullScreen = false;
	}
	else
	{
		bFullScreen = true;
	}
	if (1 == lockwndpos)
	{
		bLockWndPos = true;
	}
	else
	{
		bLockWndPos = false;
	}
	if (1 == hidewndsides)
	{
		bHideWndSides = true;
	}
	else
	{
		bHideWndSides = false;
	}
	if (1 == bandshowcpu)
	{
		bBandShowCpu = true;
	}
	else
	{
		bBandShowCpu = false;
	}
	if (1 == bandshowmem)
	{
		bBandShowMem = true;
	}
	else
	{
		bBandShowMem = false;
	}
	if (1 == bandshownetup)
	{
		bBandShowNetUp = true;
	}
	else
	{
		bBandShowNetUp = false;
	}
	if (1 == bandshownetdown)
	{
		bBandShowNetDown = true;
	}
	else
	{
		bBandShowNetDown = false;
	}
	if (1 == bandshowdisktem)
	{
		bBandShowDiskTem = true;
	}
	else
	{
		bBandShowDiskTem = false;
	}
	if (1 == bandshowcputem)
	{
		bBandShowCpuTem = true;
	}
	else
	{
		bBandShowCpuTem = false;
	}
	if (adapter != -1)
	{
		g_iAdapter= adapter;
	}
	else
	{
		g_iAdapter = 0;
	}
	if (1 == showonesideinfo)
	{
		bShowOneSideInfo = true;
	}
	else
	{
		bShowOneSideInfo = false;
	}
}


BOOL CpermoDlg::SaveConfig()
{
	TCHAR direc[256];
	::GetCurrentDirectory(256, direc);//��ȡ��ǰĿ¼����
	TCHAR temp[256];
	wsprintf(temp, _T("%s\\config.ini"), direc);
	TCHAR cLeft[32], cTop[32], cTopMost[32], cSkin[32], cAutoHide[32], 
		cTrans[32], cShowWay[32], cShowNetInfo[32], cShowTempInfo[32], 
		cFontSize[32], cShowBand[32], cBandFontSize[32], cBandWidth[32],
		cBandHeight[32], cFullScreen[32], cLockWndPos[32], cHideWndSides[32],
		cBandShowCpu[32], cBandShowMem[32], cBandShowNetUp[32], cBandShowNetDown[32],
		cBandShowDiskTem[32], cBandShowCpuTem[32], cAdapter[32], cShowOneSideInfo[32];
	_itow_s(rCurPos.left, cLeft, 10);
	_itow_s(rCurPos.top, cTop, 10);
	_itow_s(bTopmost, cTopMost, 10);
	_itow_s(nSkin, cSkin, 10);
	_itow_s(bAutoHide, cAutoHide, 10);
	_itow_s(nTrans, cTrans, 10);
	_itow_s(nShowWay, cShowWay, 10);
	_itow_s(bShowNetInfo, cShowNetInfo, 10);
	_itow_s(bShowTempInfo, cShowTempInfo, 10);
	_itow_s(nFontSize, cFontSize, 10);
	_itow_s(bShowBand, cShowBand, 10);
	_itow_s(nBandFontSize, cBandFontSize, 10);
	_itow_s(nBandWidth, cBandWidth, 10);
	_itow_s(nBandHeight, cBandHeight, 10);
	_itow_s(bFullScreen, cFullScreen, 10);
	_itow_s(bLockWndPos, cLockWndPos, 10);
	_itow_s(bHideWndSides, cHideWndSides, 10);
	_itow_s(bBandShowCpu, cBandShowCpu, 10);
	_itow_s(bBandShowMem, cBandShowMem, 10);
	_itow_s(bBandShowNetUp, cBandShowNetUp, 10);
	_itow_s(bBandShowNetDown, cBandShowNetDown, 10);
	_itow_s(bBandShowDiskTem, cBandShowDiskTem, 10);
	_itow_s(bBandShowCpuTem, cBandShowCpuTem, 10);
	_itow_s(g_iAdapter, cAdapter, 10);
	_itow_s(bShowOneSideInfo, cShowOneSideInfo, 10);
	::WritePrivateProfileString(_T("Main"), _T("left"), cLeft, temp);
	::WritePrivateProfileString(_T("Main"), _T("top"), cTop, temp);
	::WritePrivateProfileString(_T("Main"), _T("topmost"), cTopMost, temp);
	::WritePrivateProfileString(_T("Main"), _T("skin"), cSkin, temp);
	::WritePrivateProfileString(_T("Main"), _T("autohide"), cAutoHide, temp);
	::WritePrivateProfileString(_T("Main"), _T("trans"), cTrans, temp);
	::WritePrivateProfileString(_T("Main"), _T("showway"), cShowWay, temp);
	::WritePrivateProfileString(_T("Main"), _T("shownetinfo"), cShowNetInfo, temp);
	::WritePrivateProfileString(_T("Main"), _T("showtempinfo"), cShowTempInfo, temp);
	::WritePrivateProfileString(_T("Main"), _T("fontsize"), cFontSize, temp);
	::WritePrivateProfileString(_T("Main"), _T("showband"), cShowBand, temp);
	::WritePrivateProfileString(_T("Main"), _T("bandfontsize"), cBandFontSize, temp);
	::WritePrivateProfileString(_T("Main"), _T("bandwidth"), cBandWidth, temp);
	::WritePrivateProfileString(_T("Main"), _T("bandheight"), cBandHeight, temp);
	::WritePrivateProfileString(_T("Main"), _T("fullscreen"), cFullScreen, temp);
	::WritePrivateProfileString(_T("Main"), _T("lockwndpos"), cLockWndPos, temp);
	::WritePrivateProfileString(_T("Main"), _T("hidewndsides"), cHideWndSides, temp);
	::WritePrivateProfileString(_T("Main"), _T("bandshowcpu"), cBandShowCpu, temp);
	::WritePrivateProfileString(_T("Main"), _T("bandshowmem"), cBandShowMem, temp);
	::WritePrivateProfileString(_T("Main"), _T("bandshownetup"), cBandShowNetUp, temp);
	::WritePrivateProfileString(_T("Main"), _T("bandshownetdown"), cBandShowNetDown, temp);
	::WritePrivateProfileString(_T("Main"), _T("bandshowdisktem"), cBandShowDiskTem, temp);
	::WritePrivateProfileString(_T("Main"), _T("bandshowcputem"), cBandShowCpuTem, temp);
	::WritePrivateProfileString(_T("Main"), _T("adapter"), cAdapter, temp);
	::WritePrivateProfileString(_T("Main"), _T("showonesideinfo"), cShowOneSideInfo, temp);
	return TRUE;
}

void CpermoDlg::OnAutoHide(void)
{
	if (bAutoHide)
	{
		bAutoHide = FALSE;
		m_Menu.CheckMenuItem(IDM_AUTOHIDE, MF_BYCOMMAND | MF_UNCHECKED);
	}
	else
	{
		bAutoHide = TRUE;
		m_Menu.CheckMenuItem(IDM_AUTOHIDE, MF_BYCOMMAND | MF_CHECKED); // ��ǰ��� 
	}
}

void CpermoDlg::OnTrans0(void)
{
	m_Menu.CheckMenuItem(IDM_TRANS0+(255-nTrans)/25, MF_BYCOMMAND | MF_UNCHECKED);
	m_Menu.CheckMenuItem(IDM_TRANS0, MF_BYCOMMAND | MF_CHECKED);
	nTrans = 255;
	::SetLayeredWindowAttributes( m_hWnd, 0, nTrans, LWA_ALPHA); // 120��͸���ȣ���Χ��0��255
}

void CpermoDlg::OnTrans10(void)
{
	m_Menu.CheckMenuItem(IDM_TRANS0+(255-nTrans)/25, MF_BYCOMMAND | MF_UNCHECKED);
	m_Menu.CheckMenuItem(IDM_TRANS10, MF_BYCOMMAND | MF_CHECKED);
	nTrans = 230;
	::SetLayeredWindowAttributes( m_hWnd, 0, nTrans, LWA_ALPHA); // 120��͸���ȣ���Χ��0��255
}

void CpermoDlg::OnTrans20(void)
{
	m_Menu.CheckMenuItem(IDM_TRANS0+(255-nTrans)/25, MF_BYCOMMAND | MF_UNCHECKED);
	m_Menu.CheckMenuItem(IDM_TRANS20, MF_BYCOMMAND | MF_CHECKED);
	nTrans = 205;
	::SetLayeredWindowAttributes( m_hWnd, 0, nTrans, LWA_ALPHA); // 120��͸���ȣ���Χ��0��255
}

void CpermoDlg::OnTrans30(void)
{
	m_Menu.CheckMenuItem(IDM_TRANS0+(255-nTrans)/25, MF_BYCOMMAND | MF_UNCHECKED);
	m_Menu.CheckMenuItem(IDM_TRANS30, MF_BYCOMMAND | MF_CHECKED);
	nTrans = 180;
	::SetLayeredWindowAttributes( m_hWnd, 0, nTrans, LWA_ALPHA); // 120��͸���ȣ���Χ��0��255
}

void CpermoDlg::OnTrans40(void)
{
	m_Menu.CheckMenuItem(IDM_TRANS0+(255-nTrans)/25, MF_BYCOMMAND | MF_UNCHECKED);
	m_Menu.CheckMenuItem(IDM_TRANS40, MF_BYCOMMAND | MF_CHECKED);
	nTrans = 155;
	::SetLayeredWindowAttributes( m_hWnd, 0, nTrans, LWA_ALPHA); // 120��͸���ȣ���Χ��0��255
}

void CpermoDlg::OnTrans50(void)
{
	m_Menu.CheckMenuItem(IDM_TRANS0+(255-nTrans)/25, MF_BYCOMMAND | MF_UNCHECKED);
	m_Menu.CheckMenuItem(IDM_TRANS50, MF_BYCOMMAND | MF_CHECKED);
	nTrans = 130;
	::SetLayeredWindowAttributes( m_hWnd, 0, nTrans, LWA_ALPHA); // 120��͸���ȣ���Χ��0��255
}

void CpermoDlg::OnTrans60(void)
{
	m_Menu.CheckMenuItem(IDM_TRANS0+(255-nTrans)/25, MF_BYCOMMAND | MF_UNCHECKED);
	m_Menu.CheckMenuItem(IDM_TRANS60, MF_BYCOMMAND | MF_CHECKED);
	nTrans = 105;
	::SetLayeredWindowAttributes( m_hWnd, 0, nTrans, LWA_ALPHA); // 120��͸���ȣ���Χ��0��255
}

void CpermoDlg::OnTrans70(void)
{
	m_Menu.CheckMenuItem(IDM_TRANS0+(255-nTrans)/25, MF_BYCOMMAND | MF_UNCHECKED);
	m_Menu.CheckMenuItem(IDM_TRANS70, MF_BYCOMMAND | MF_CHECKED);
	nTrans = 80;
	::SetLayeredWindowAttributes( m_hWnd, 0, nTrans, LWA_ALPHA); // 120��͸���ȣ���Χ��0��255
}

void CpermoDlg::OnTrans80(void)
{
	m_Menu.CheckMenuItem(IDM_TRANS0+(255-nTrans)/25, MF_BYCOMMAND | MF_UNCHECKED);
	m_Menu.CheckMenuItem(IDM_TRANS80, MF_BYCOMMAND | MF_CHECKED);
	nTrans = 55;
	::SetLayeredWindowAttributes( m_hWnd, 0, nTrans, LWA_ALPHA); // 120��͸���ȣ���Χ��0��255
}

void CpermoDlg::OnTrans90(void)
{
	m_Menu.CheckMenuItem(IDM_TRANS0+(255-nTrans)/25, MF_BYCOMMAND | MF_UNCHECKED);
	m_Menu.CheckMenuItem(IDM_TRANS90, MF_BYCOMMAND | MF_CHECKED);
	nTrans = 30;
	::SetLayeredWindowAttributes( m_hWnd, 0, nTrans, LWA_ALPHA); // 120��͸���ȣ���Χ��0��255
}

//���ÿ���������
//�Ȳ��ң��ҵ���ɾ�����Ҳ����ʹ���
void CpermoDlg::SetAutoRun(void)
{
	HKEY hKey;
	CString strRegPath = _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");//�ҵ�ϵͳ��������
	if (RegOpenKeyEx(HKEY_CURRENT_USER, strRegPath, 0, KEY_QUERY_VALUE|KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
	{
		if (RegQueryValueEx(hKey, _T("permo"), NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
		{
			RegDeleteValue (hKey,_T("permo"));   
			if (bIsWindowsVistaOrGreater)
			{
				DelWin7SchTasks();
			}
			m_Menu.CheckMenuItem(IDM_AUTOSTART, MF_BYCOMMAND | MF_UNCHECKED);
		}
		else
		{
			TCHAR szModule[_MAX_PATH];
			GetModuleFileName(NULL, szModule, _MAX_PATH);//�õ������������ȫ·��
			if (bIsWindowsVistaOrGreater)
			{
				AddWin7SchTasks();
				RegSetValueEx(hKey,_T("permo"), 0, REG_SZ, (LPBYTE)szModule, 0); //���һ����Key,������ֵ
			}
			else
			{
				RegSetValueEx(hKey,_T("permo"), 0, REG_SZ, (LPBYTE)szModule, wcslen(szModule)*sizeof(TCHAR)); //���һ����Key,������ֵ
			}
			m_Menu.CheckMenuItem(IDM_AUTOSTART, MF_BYCOMMAND | MF_CHECKED);
		}
		RegCloseKey(hKey); //�ر�ע���
	}
	else
	{
		AfxMessageBox(_T("����ʧ�ܣ�����Ȩ��������߱�ɱ�����������~~"));   
	}
}

void CpermoDlg::IfAutoRun(void)
{
	HKEY hKey;
	CString strRegPath = _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");//�ҵ�ϵͳ��������
	if (RegOpenKeyEx(HKEY_CURRENT_USER, strRegPath, 0, KEY_QUERY_VALUE|KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
	{
		if (RegQueryValueEx(hKey, _T("permo"), NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
		{
			m_Menu.CheckMenuItem(IDM_AUTOSTART, MF_BYCOMMAND | MF_CHECKED);
		}
		RegCloseKey(hKey); //�ر�ע���
	}
}

void CpermoDlg::CreateInfoDlg(void)
{
	pInfoDlg = new CInfoDlg(this);
	pInfoDlg->Create(IDD_INFO_DIALOG, this);
	if (NULL == pInfoDlg)
	{
		return;
	}
	//Ĭ�ϲ���ʾ
	MoveInfoDlg();
	//pInfoDlg->ShowWindow(SW_SHOW);
}

//����ϸ��Ϣ�����ƶ����ʵ���λ��
void CpermoDlg::MoveInfoDlg(void)
{
	CRect rect = rCurPos;
	if (bHideWndSides)
	{
		rect.left -= 45;
		rect.right += 45;
	}
	else
	{
		rect.left -= 10;
		rect.right += 10;
	}
	if (rect.left < 0)
	{
		rect.left = 0;
		rect.right = rect.left + 240;
	}
	else if (rect.right > rWorkArea.right)
	{
		rect.right = rWorkArea.right;
		rect.left = rect.right - 240;
	}
	if (rCurPos.top < 365)
	{
		rect.top = rCurPos.bottom + 5;
		rect.bottom = rect.top + 360;
	}
	else
	{
		rect.bottom = rCurPos.top - 5;
		rect.top = rect.bottom - 360;
	}
	
	pInfoDlg->MoveWindow(&rect,TRUE);
}

//��������Ŀ¼����ֹ����������ȡ���ó���
BOOL CpermoDlg::SetWorkDir(void)
{
	TCHAR szPath[MAX_PATH]; 
	if( !GetModuleFileName( NULL, szPath, MAX_PATH ) )
	{
		//printf("GetModuleFileName failed (%d)\n", GetLastError()); 
		return FALSE;
	}
	PathRemoveFileSpec(szPath);
	::SetCurrentDirectory(szPath);
	return TRUE;
}

void CpermoDlg::SetShowWay(int index)
{
	m_Menu.CheckMenuItem(IDM_SHOWBYHOVER + nShowWay, MF_BYCOMMAND | MF_UNCHECKED);
	nShowWay = index;
	m_Menu.CheckMenuItem(IDM_SHOWBYHOVER + nShowWay, MF_BYCOMMAND | MF_CHECKED);
}

void CpermoDlg::OnLButtonUp(UINT nFlags, CPoint point)
{
	// TODO: �ڴ������Ϣ�����������/�����Ĭ��ֵ
	if (1 == nShowWay)
	{
		MoveInfoDlg();
		pInfoDlg->ShowWindow(SW_SHOW);
		bInfoDlgShowing = true;
	}
	ReleaseCapture();
	mm_Timer.CreateTimer((DWORD)this,1000,TimerCallbackTemp);
	CDialog::OnLButtonUp(nFlags, point);
}


void CpermoDlg::OnMouseMove(UINT nFlags, CPoint point)
{
	// TODO: �ڴ������Ϣ�����������/�����Ĭ��ֵ
	if (_bMouseTrack)    //������׷�٣���
	{
		TRACKMOUSEEVENT csTME;
		csTME.cbSize = sizeof(csTME);
		csTME.dwFlags = TME_LEAVE|TME_HOVER;
		csTME.hwndTrack = m_hWnd;//ָ��Ҫ׷�ٵĴ���
		csTME.dwHoverTime = 10;  //����ڰ�ť��ͣ������10ms������Ϊ״̬ΪHOVER
		::_TrackMouseEvent(&csTME); //����Windows��WM_MOUSELEAVE��WM_MOUSEHOVER�¼�֧��
		_bMouseTrack=FALSE;   //���Ѿ�׷�٣���ֹͣ׷��
	}
	if (bLockWndPos)
	{
		return;
	}
	static CPoint PrePoint = CPoint(0, 0);  
	if(MK_LBUTTON == nFlags)  
	{  
		if(point != PrePoint)  
		{  
			CPoint ptTemp = point - PrePoint;  
			CRect rcWindow;  
			GetWindowRect(&rcWindow);  
			rcWindow.OffsetRect(ptTemp.x, ptTemp.y);  
			if (rcWindow.bottom <= rWorkArea.bottom && rcWindow.left >= 0 && 
				rcWindow.right <= rWorkArea.right && rcWindow.top >= 0)
			{
				MoveWindow(&rcWindow); 
				rCurPos = rcWindow;
				MoveInfoDlg();
			}
			return ;  
			
		}  
	}  
	PrePoint = point; 
	CDialog::OnMouseMove(nFlags, point);
}


void CpermoDlg::OnMouseHover(UINT nFlags, CPoint point)
{
	// TODO: �ڴ������Ϣ�����������/�����Ĭ��ֵ
	if (bAutoHide && !bHideWndSides)
	{
		int tmp;
		if (bShowOneSideInfo)
		{
			tmp = 185;
		}
		else
		{
			tmp = 219;
		}
		if (rCurPos.right == rWorkArea.right + tmp)
		{
			rCurPos.right = rWorkArea.right;
			rCurPos.left = rCurPos.right - 220;
			MoveWindow(&rCurPos, TRUE);
		}
		else if (rCurPos.left == rWorkArea.left - tmp)
		{
			rCurPos.left = rWorkArea.left;
			rCurPos.right = rCurPos.left + 220;
			MoveWindow(&rCurPos, TRUE);
		}
		else if (rCurPos.bottom == rWorkArea.top + 1)
		{
			rCurPos.top = rWorkArea.top;
			rCurPos.bottom = rCurPos.top + 22;
			MoveWindow(&rCurPos, TRUE);
		}
	}
	else if (bAutoHide && bHideWndSides)
	{
		int tmp;
		tmp = 145;
		if (rCurPos.right == rWorkArea.right + tmp)
		{
			rCurPos.right = rWorkArea.right;
			rCurPos.left = rCurPos.right - 150;
			MoveWindow(&rCurPos, TRUE);
		}
		else if (rCurPos.left == rWorkArea.left - tmp)
		{
			rCurPos.left = rWorkArea.left;
			rCurPos.right = rCurPos.left + 150;
			MoveWindow(&rCurPos, TRUE);
		}
		else if (rCurPos.bottom == rWorkArea.top + 1)
		{
			rCurPos.top = rWorkArea.top;
			rCurPos.bottom = rCurPos.top + 22;
			MoveWindow(&rCurPos, TRUE);
		}
	}
	{
		//pInfoDlg->SetTimer(1, 1000, NULL);
		if (0 == nShowWay)
		{
			pInfoDlg->ShowWindow(SW_SHOW);
			bInfoDlgShowing = true;
		}
	}
	CDialog::OnMouseHover(nFlags, point);
}

void CpermoDlg::OnMouseLeave()
{
	// TODO: �ڴ������Ϣ�����������/�����Ĭ��ֵ
	_bMouseTrack=TRUE; 
	if (bAutoHide && !bHideWndSides)
	{
		int tmp;
		if (bShowOneSideInfo)
		{
			tmp = 185;
		}
		else
		{
			tmp = 219;
		}
		if (rCurPos.right >= rWorkArea.right - 5)
		{
			rCurPos.right = rWorkArea.right + tmp;
			rCurPos.left = rCurPos.right - 220;
			MoveWindow(&rCurPos, TRUE);
		}
		else if (rCurPos.left <= rWorkArea.left + 5)
		{
			rCurPos.left = rWorkArea.left - tmp;
			rCurPos.right = rCurPos.left + 220;
			MoveWindow(&rCurPos, TRUE);
		}
		else if (rCurPos.top <= rWorkArea.top + 5)
		{
			rCurPos.bottom = rWorkArea.top + 1;
			rCurPos.top = rCurPos.bottom - 22;
			MoveWindow(&rCurPos, TRUE);
		}
	}
	else if (bAutoHide && bHideWndSides)
	{
		int tmp;
		tmp = 145;
		if (rCurPos.right >= rWorkArea.right - 5)
		{
			rCurPos.right = rWorkArea.right + tmp;
			rCurPos.left = rCurPos.right - 150;
			MoveWindow(&rCurPos, TRUE);
		}
		else if (rCurPos.left <= rWorkArea.left + 5)
		{
			rCurPos.left = rWorkArea.left - tmp;
			rCurPos.right = rCurPos.left + 150;
			MoveWindow(&rCurPos, TRUE);
		}
		else if (rCurPos.top <= rWorkArea.top + 5)
		{
			rCurPos.bottom = rWorkArea.top + 1;
			rCurPos.top = rCurPos.bottom - 22;
			MoveWindow(&rCurPos, TRUE);
		}
	}
	{
		pInfoDlg->ShowWindow(SW_HIDE);
		bInfoDlgShowing = false;
	}
	SaveConfig();
	CDialog::OnMouseLeave();
}

BOOL CpermoDlg::OnEraseBkgnd(CDC* pDC)
{
	// TODO: �ڴ������Ϣ�����������/�����Ĭ��ֵ

	return TRUE;
}

void CpermoDlg::ShowNetInfo(void)
{
	if (bShowNetInfo)
	{
		bShowNetInfo = false;
		m_Menu.CheckMenuItem(IDM_SHOWNETINFO, MF_BYCOMMAND | MF_UNCHECKED);
	}
	else
	{
		bShowNetInfo = true;
		m_Menu.CheckMenuItem(IDM_SHOWNETINFO, MF_BYCOMMAND | MF_CHECKED);
	}
}

void CpermoDlg::ShowTempInfo(void)
{
	if (bShowTempInfo)
	{
		bShowTempInfo = false;
		m_Menu.CheckMenuItem(IDM_SHOWTEMPINFO, MF_BYCOMMAND | MF_UNCHECKED);
	}
	else
	{
		bShowTempInfo = true;
		m_Menu.CheckMenuItem(IDM_SHOWTEMPINFO, MF_BYCOMMAND | MF_CHECKED);
	}
}

void CpermoDlg::SetFontSize(UINT fontSize)
{
	nFontSize = fontSize;
	m_Menu.CheckMenuItem(IDM_FONTSIZE12, MF_BYCOMMAND | MF_UNCHECKED);
	m_Menu.CheckMenuItem(IDM_FONTSIZE13, MF_BYCOMMAND | MF_UNCHECKED);
	m_Menu.CheckMenuItem(IDM_FONTSIZE14, MF_BYCOMMAND | MF_UNCHECKED);
	m_Menu.CheckMenuItem(IDM_FONTSIZE15, MF_BYCOMMAND | MF_UNCHECKED);
	m_Menu.CheckMenuItem(IDM_FONTSIZE16, MF_BYCOMMAND | MF_UNCHECKED);
	m_Menu.CheckMenuItem(IDM_FONTSIZE17, MF_BYCOMMAND | MF_UNCHECKED);
	m_Menu.CheckMenuItem(IDM_FONTSIZE18, MF_BYCOMMAND | MF_UNCHECKED);
	m_Menu.CheckMenuItem(IDM_FONTSIZE12 + nFontSize - 12, MF_BYCOMMAND | MF_CHECKED);
	Invalidate(FALSE);
}

void CpermoDlg::ShowBand(void)
{
	if (NULL == pcoControl)
	{
		pcoControl = new CNProgressBar(this);
		pcoControl->SetFontSize(nBandFontSize);
		SetBandWidth(nBandWidth);
		SetBandHeight(nBandHeight);
	}
	if (bShowBand)
	{
		bShowBand = false;
		m_Menu.CheckMenuItem(IDM_SHOWBAND, MF_BYCOMMAND | MF_UNCHECKED);
	}
	else
	{
		bShowBand = true;
		m_Menu.CheckMenuItem(IDM_SHOWBAND, MF_BYCOMMAND | MF_CHECKED);
	}
	if (pcoControl->IsControlSuccessfullyCreated())
	{
		pcoControl->Show(bShowBand);
	}
}

// ��ʾ��������������������е��ã�
void CpermoDlg::ShowHideWindow(void)
{
	if (IsWindowVisible())
	{
		ShowWindow(SW_HIDE);
		bIsWndVisable = false;
	}
	else
	{
		ShowWindow(SW_SHOW);
		bIsWndVisable = true;
	}
}

LRESULT CpermoDlg::OnReconnect(WPARAM wparam,LPARAM lparam)
{
	StopCapture();
	StartCapture();
	return 0;
}

LRESULT CpermoDlg::OnBandMenu(WPARAM wparam,LPARAM lparam)
{
	SetForegroundWindow();
	CPoint p;
	//���ݹ���������Ϊ����ڴ������Ͻǵ����꣬WM_CONTEXTMENU���ݹ���������Ļ����
	GetCursorPos(&p);//�������Ļ����
	int nID = m_BandMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RETURNCMD, p.x, p.y, this);
	switch (nID)
	{
	//��ʾCPUռ����Ϣ��Ĭ�ϣ�
	case IDM_BANDSHOWCPU:
		{
			mm_Timer.KillTimer();
			if (IfExist(0))
			{
				m_BandMenu.CheckMenuItem(IDM_BANDSHOWCPU, MF_BYCOMMAND | MF_UNCHECKED); 
				RemoveBandShow(0);
				bBandShowCpu = false;
			}
			else
			{
				m_BandMenu.CheckMenuItem(IDM_BANDSHOWCPU, MF_BYCOMMAND | MF_CHECKED); 
				vBandShow.push_back(0);
				bBandShowCpu = true;
			}
			nNowBandShowIndex = 0;
			nCount = 0;
			mm_Timer.CreateTimer((DWORD)this,1000,TimerCallbackTemp);
		}
		break;
	//��ʾ�ڴ�ռ����Ϣ
	case IDM_BANDSHOWMEM:
		{
			mm_Timer.KillTimer();
			if (IfExist(1))
			{
				m_BandMenu.CheckMenuItem(IDM_BANDSHOWMEM, MF_BYCOMMAND | MF_UNCHECKED);
				RemoveBandShow(1);
				bBandShowMem =false;
			}
			else
			{
				m_BandMenu.CheckMenuItem(IDM_BANDSHOWMEM, MF_BYCOMMAND | MF_CHECKED);
				vBandShow.push_back(1);
				bBandShowMem = true;
			}
			nNowBandShowIndex = 0;
			nCount = 0;
			mm_Timer.CreateTimer((DWORD)this,1000,TimerCallbackTemp);
		}
		break;
	//��ʾ����ռ����Ϣ
	case IDM_BANDSHOWNETDOWN:
		{
			mm_Timer.KillTimer();
			if (IfExist(2))
			{
				m_BandMenu.CheckMenuItem(IDM_BANDSHOWNETDOWN, MF_BYCOMMAND | MF_UNCHECKED); 
				RemoveBandShow(2);
				bBandShowNetDown = false;
			}
			else
			{
				m_BandMenu.CheckMenuItem(IDM_BANDSHOWNETDOWN, MF_BYCOMMAND | MF_CHECKED);
				vBandShow.push_back(2);
				bBandShowNetDown = true;
			}
			nNowBandShowIndex = 0;
			nCount = 0;
			mm_Timer.CreateTimer((DWORD)this,1000,TimerCallbackTemp);
		}
		break;
	case IDM_BANDSHOWNETUP:
		{
			mm_Timer.KillTimer();
			if (IfExist(3))
			{
				m_BandMenu.CheckMenuItem(IDM_BANDSHOWNETUP, MF_BYCOMMAND | MF_UNCHECKED);
				RemoveBandShow(3);
				bBandShowNetUp = false;
			}
			else
			{
				m_BandMenu.CheckMenuItem(IDM_BANDSHOWNETUP, MF_BYCOMMAND | MF_CHECKED);
				vBandShow.push_back(3);
				bBandShowNetUp = true;
			}
			nNowBandShowIndex = 0;
			nCount = 0;
			mm_Timer.CreateTimer((DWORD)this,1000,TimerCallbackTemp);
		}
		break;
	case IDM_BANDSHOWDISKTEM:
		{
			mm_Timer.KillTimer();
			if (IfExist(4))
			{
				m_BandMenu.CheckMenuItem(IDM_BANDSHOWDISKTEM, MF_BYCOMMAND | MF_UNCHECKED);
				RemoveBandShow(4);
				bBandShowDiskTem = false;
			}
			else
			{
				m_BandMenu.CheckMenuItem(IDM_BANDSHOWDISKTEM, MF_BYCOMMAND | MF_CHECKED);
				vBandShow.push_back(4);
				bBandShowDiskTem = true;
			}
			nNowBandShowIndex = 0;
			nCount = 0;
			mm_Timer.CreateTimer((DWORD)this,1000,TimerCallbackTemp);
		}
		break;
	case IDM_BANDSHOWCPUTEM:
		{
			mm_Timer.KillTimer();
			if (IfExist(5))
			{
				m_BandMenu.CheckMenuItem(IDM_BANDSHOWCPUTEM, MF_BYCOMMAND | MF_UNCHECKED);
				RemoveBandShow(5);
				bBandShowCpuTem = false;
			}
			else
			{
				m_BandMenu.CheckMenuItem(IDM_BANDSHOWCPUTEM, MF_BYCOMMAND | MF_CHECKED);
				vBandShow.push_back(5);
				bBandShowCpuTem = true;
			}
			nNowBandShowIndex = 0;
			nCount = 0;
			mm_Timer.CreateTimer((DWORD)this,1000,TimerCallbackTemp);
		}
		break;
	case IDM_BANDFONTSIZE12:
		SetBandFontSize(12);
		break;
	case IDM_BANDFONTSIZE13:
		SetBandFontSize(13);
		break;
	case IDM_BANDFONTSIZE14:
		SetBandFontSize(14);
		break;
	case IDM_BANDFONTSIZE15:
		SetBandFontSize(15);
		break;
	case IDM_BANDFONTSIZE16:
		SetBandFontSize(16);
		break;
	case IDM_BANDFONTSIZE17:
		SetBandFontSize(17);
		break;
	case IDM_BANDWIDTH50:
		{
			SetBandWidth(50);
			pcoControl->Show(true);
		}
		break;
	case IDM_BANDWIDTH60:
		{
			SetBandWidth(60);
			pcoControl->Show(true);
		}
		break;
	case IDM_BANDWIDTH70:
		{
			SetBandWidth(70);
			pcoControl->Show(true);
		}
		break;
	case IDM_BANDWIDTH80:
		{
			SetBandWidth(80);
			pcoControl->Show(true);
		}
		break;
	case IDM_BANDHEIGHT20:
		{
			SetBandHeight(20);
			pcoControl->Show(true);
		}
		break;
	case IDM_BANDHEIGHT25:
		{
			SetBandHeight(25);
			pcoControl->Show(true);
		}
		break;
	case IDM_BANDHEIGHT30:
		{
			SetBandHeight(30);
			pcoControl->Show(true);
		}
		break;
	case IDM_BANDHEIGHT35:
		{
			SetBandHeight(35);
			pcoControl->Show(true);
		}
		break;
	//���ػ���ʾ������
	case IDM_SHOWHIDEWND:
		ShowHideWindow();
		break;
	case IDM_EXIT:
		OnExit();
		break;
	default:
		break;
	}
	SaveConfig();
	return 0;
}
void CpermoDlg::SetBandFontSize(int bandFontSize)
{
	nBandFontSize = bandFontSize;
	pcoControl->SetFontSize(bandFontSize);
	pcoControl->Invalidate(FALSE);
	m_BandMenu.CheckMenuItem(IDM_BANDFONTSIZE12, MF_BYCOMMAND | MF_UNCHECKED);
	m_BandMenu.CheckMenuItem(IDM_BANDFONTSIZE13, MF_BYCOMMAND | MF_UNCHECKED);
	m_BandMenu.CheckMenuItem(IDM_BANDFONTSIZE14, MF_BYCOMMAND | MF_UNCHECKED);
	m_BandMenu.CheckMenuItem(IDM_BANDFONTSIZE15, MF_BYCOMMAND | MF_UNCHECKED);
	m_BandMenu.CheckMenuItem(IDM_BANDFONTSIZE16, MF_BYCOMMAND | MF_UNCHECKED);
	m_BandMenu.CheckMenuItem(IDM_BANDFONTSIZE17, MF_BYCOMMAND | MF_UNCHECKED);
	m_BandMenu.CheckMenuItem(IDM_BANDFONTSIZE12 + bandFontSize - 12, MF_BYCOMMAND | MF_CHECKED);
}

void CpermoDlg::SetBandWidth(unsigned int bandwidth)
{
	pcoControl->SetWidth(bandwidth);
	m_BandMenu.CheckMenuItem(IDM_BANDWIDTH50 + (nBandWidth-50)/10, MF_BYCOMMAND | MF_UNCHECKED);
	nBandWidth = bandwidth;
	m_BandMenu.CheckMenuItem(IDM_BANDWIDTH50 + (nBandWidth-50)/10, MF_BYCOMMAND | MF_CHECKED);
}

void CpermoDlg::SetBandHeight(unsigned int bandheight)
{
	pcoControl->SetHeight(bandheight);
	m_BandMenu.CheckMenuItem(IDM_BANDHEIGHT20 + (nBandHeight-20)/5, MF_BYCOMMAND | MF_UNCHECKED);
	nBandHeight = bandheight;
	m_BandMenu.CheckMenuItem(IDM_BANDHEIGHT20 + (nBandHeight-20)/5, MF_BYCOMMAND | MF_CHECKED);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * �������ƣ�IsForegroundFullscreen
 * ����˵�����жϵ�ǰ�������û������ĵ�ǰ������Ƿ���ȫ���ġ�
 * ����˵������
 * ����˵����true���ǡ�
             false����
 * �̰߳�ȫ����
 * ����������IsForegroundFullscreen ()����ʾ�жϵ�ǰ�������û������ĵ�ǰ������Ƿ���

ȫ���ġ�
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
bool CpermoDlg::IsForegroundFullscreen(void)
{
	bool bFullscreen = false;//��ŵ�ǰ������Ƿ���ȫ���ģ�true��ʾ�ǣ�false��ʾ����
	HWND hWnd;
	RECT rcApp;
	RECT rcDesk;

	hWnd = ::GetForegroundWindow ();//��ȡ��ǰ�������û������ĵ�ǰ����ھ��

	if ((hWnd != ::GetDesktopWindow ()) && (hWnd != ::GetShellWindow ()))//�����ǰ����ڲ������洰�ڣ�Ҳ���ǿ���̨����
	{
		::GetWindowRect (hWnd, &rcApp);//��ȡ��ǰ����ڵ�����
		::GetWindowRect (::GetDesktopWindow(), &rcDesk);//�������洰�ھ������ȡ������Ļ������

		if (rcApp.left <= rcDesk.left && //�����ǰ����ڵ�������ȫ����ס���洰�ڣ��ͱ�ʾ��ǰ�������ȫ����
			rcApp.top <= rcDesk.top &&
			rcApp.right >= rcDesk.right &&
			rcApp.bottom >= rcDesk.bottom)
		{

			TCHAR szTemp[100];

			if (::GetClassName (hWnd, szTemp, sizeof (szTemp)) > 0)//�����ȡ��ǰ����ڵ������ɹ�
			{
				if (wcscmp (szTemp, _T("WorkerW")) != 0)//����������洰�ڵ�����������Ϊ��ǰ�������ȫ������
					bFullscreen = true;
			}
			else bFullscreen = true;//�����ȡʧ�ܣ�����Ϊ��ǰ�������ȫ������
		}
	}//�����ǰ����������洰�ڣ������ǿ���̨���ڣ���ֱ�ӷ��ز���ȫ��

	return bFullscreen;
}

bool CpermoDlg::IfExist(int nVal)
{
	vector<int>::iterator iter = find(vBandShow.begin(), vBandShow.end(), nVal);
	if (vBandShow.end() == iter)
	{
		return false;
	}
	return true;
}

void CpermoDlg::RemoveBandShow(int nVal)
{
	vector<int>::iterator iter;
	for(iter=vBandShow.begin();iter!=vBandShow.end();)
	{  
		if (nVal == (*iter))
		{
			iter = vBandShow.erase(iter);
		}
		else
		{
			iter++;
		}
	}
}

void CpermoDlg::GetDiskTem(void)
{
	hres = pSvc->ExecQuery(
		bstr_t("WQL"), 
		bstr_t("SELECT * FROM MSStorageDriver_ATAPISmartData"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, 
		NULL,
		&pEnumerator);

	int nTemperature = 0;
	//int nTotalTime = 0;

	IWbemClassObject *pclsObj;
	ULONG uReturn = 0;

	while (pEnumerator)
	{
		HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, 
			&pclsObj, &uReturn);

		if(0 == uReturn)
		{
			break;
		}
		VARIANT vtProp;
		VariantInit(&vtProp);

		// Get the value of the VendorSpecific property
		hr = pclsObj->Get(L"VendorSpecific", 0, &vtProp, 0, 0);
		
		if (vtProp.vt == VT_ARRAY|VT_UI1 )
		{
			SAFEARRAY *pIn;
			pIn = vtProp.parray;
			VARTYPE vt;
			UINT dim;
			SafeArrayGetVartype(pIn,&vt); //Data type
			dim = SafeArrayGetDim(pIn); //Dimension
			long LBound; //The lower bound
			long UBound; //Upper bound
			SafeArrayGetLBound(pIn,1,&LBound); //To obtain lower bounds
			SafeArrayGetUBound(pIn,1,&UBound); //To obtain upper bounds
			BYTE *pdata = new BYTE[UBound-LBound+1];
			ZeroMemory(pdata,UBound-LBound+1);

			BYTE *buf;
			SafeArrayAccessData(pIn, (void **)&buf);
			memcpy(pdata,buf,UBound-LBound+1);
			SafeArrayUnaccessData(pIn);

			BYTE* pTemp = pdata+2;
			for(int i=2;i<UBound-LBound+1;i+=12)
			{
				pTemp = pdata+i;
				if (*pTemp == 0xc2)
				{
					//Beep(1000,200);
					nTemperature = *(pTemp+5);
				}

				//�ܵ�ʹ��ʱ��
				// 				if (*pTemp == 0x09)
				// 				{
				// 					//Beep(1000,200);
				// 					nTotalTime = (*(pTemp+5)) + (*(pTemp+6)<<8);
				// 				}

			}
			delete pdata;
		}
		//Release objects not owned
		pclsObj->Release();
		nTempDisk = nTemperature;
		VariantClear(&vtProp);
	}
	// All done.
	pEnumerator->Release();
}

void CpermoDlg::StopCapture(void)
{
	g_bCapture = false;
	WaitForSingleObject(g_hCaptureThread, INFINITE);
	CloseHandle(g_hCaptureThread);
}

void CpermoDlg::StartCapture(void)
{
	g_bCapture = true;
	g_hCaptureThread = CreateThread(0, 0, CaptureThread, 0, 0, 0);
}

void CpermoDlg::DeleteFiles(void)
{
	TCHAR direc[256];
	::GetCurrentDirectory(256, direc);//��ȡ��ǰĿ¼����
	TCHAR temp[256];
	wsprintf(temp, _T("%s\\npf.sys"), direc);
	DeleteFile(temp);
	wsprintf(temp, _T("%s\\wpcap.dll"), direc);
	DeleteFile(temp);
	wsprintf(temp, _T("%s\\Packet.dll"), direc);
	DeleteFile(temp);
	wsprintf(temp, _T("%s\\WinRing0.sys"), direc);
	DeleteFile(temp);
}

// win7�Լ�����ϵͳ�����������ͨ������ƻ�
void CpermoDlg::AddWin7SchTasks(void)
{
	TCHAR Location[MAX_PATH];
	TCHAR cmd[255];

	DWORD TapLocLen = 0;

	TapLocLen = GetCurrentDirectory(sizeof(Location),Location);

	if (TapLocLen == 0) 
	{
		printf("GetCurrentDirectory failed!  Error is %d \n", GetLastError());
		return ;
	}
	
	TCHAR szModule[_MAX_PATH];
	GetModuleFileName(NULL, szModule, _MAX_PATH);//�õ������������ȫ·��

	wsprintf(cmd,_T("/C schtasks /Create /tn \"autorun permo\" /tr \"\\\"%s\\\"\" /sc onlogon /rl HIGHEST /F"), szModule);  

	SHELLEXECUTEINFO sei; 
	ZeroMemory (&sei, sizeof (SHELLEXECUTEINFO)); 
	sei.cbSize = sizeof (SHELLEXECUTEINFO); 
	sei.lpVerb = _T("OPEN");
	sei.lpFile = _T("cmd.exe");
	sei.lpParameters = cmd;
	sei.nShow = SW_HIDE;
	sei.fMask = SEE_MASK_NOCLOSEPROCESS;

	ShellExecuteEx(&sei); 
	WaitForSingleObject(sei.hProcess, INFINITE);
	return;
}

void CpermoDlg::DelWin7SchTasks(void)
{
	TCHAR Location[MAX_PATH];
	TCHAR cmd[255];

	DWORD TapLocLen = 0;

	TapLocLen = GetCurrentDirectory(sizeof(Location),Location);

	if (TapLocLen == 0) 
	{
		printf("GetCurrentDirectory failed!  Error is %d \n", GetLastError());
		return ;
	}

	wcscpy(cmd, _T("/C schtasks /delete /tn \"autorun permo\" /F"));

	SHELLEXECUTEINFO sei; 
	ZeroMemory (&sei, sizeof (SHELLEXECUTEINFO)); 
	sei.cbSize = sizeof (SHELLEXECUTEINFO); 
	sei.lpVerb = _T("OPEN");
	sei.lpFile = _T("cmd.exe");
	sei.lpParameters = cmd;
	sei.nShow = SW_HIDE;
	sei.fMask = SEE_MASK_NOCLOSEPROCESS;

	ShellExecuteEx(&sei); 
	WaitForSingleObject(sei.hProcess, INFINITE);
	return;
}

bool CpermoDlg::IsIntel(void)
{
	char    OEMString[13];
	_asm
	{       
		mov     eax,0       
		cpuid       
		mov     DWORD     PTR     OEMString,ebx       
		mov     DWORD     PTR     OEMString+4,edx       
		mov     DWORD     PTR     OEMString+8,ecx       
		mov     BYTE     PTR     OEMString+12,0       
	}       
	USES_CONVERSION; 
	CString strTmp = A2T(OEMString);
/*	MessageBox(strTmp);*/
	if (-1 == strTmp.Find(_T("Intel")))
	{
		gIsMsr = FALSE;
		return false;
	}
	gIsMsr = TRUE;
	return true;
}


DWORD CpermoDlg::GetCpuInfo(DWORD veax)
{
	DWORD deax, debx, decx, dedx;
	__asm
	{
		push eax;
		push ebx;
		push ecx;
		push edx;

		mov eax, veax;
		cpuid;
		mov deax, eax;
		mov debx, ebx;
		mov decx, ecx;
		mov dedx, edx;

		pop edx;
		pop ecx;
		pop ebx;
		pop eax;
	}
	return deax;
}

//��ȡmsr�Ĵ���
BOOL WINAPI Rdmsr(DWORD index, PDWORD eax, PDWORD edx)
{
	if(gHandle2 == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}
	if(eax == NULL || edx == NULL || gIsMsr == FALSE)
	{
		return FALSE;
	}
	DWORD	returnedLength = 0;
	BOOL	result = FALSE;
	BYTE	outBuf[8] = {0};
	result = DeviceIoControl(
		gHandle2,
		IOCTL_OLS_READ_MSR,
		&index,
		sizeof(index),
		&outBuf,
		sizeof(outBuf),
		&returnedLength,
		NULL
		);
	if(result)
	{
		memcpy(eax, outBuf, 4);
		memcpy(edx, outBuf + 4, 4);
	}
	if(result)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

void CpermoDlg::GetCpuTemp(void)
{
	bool bSupportDts = false;
	DWORD eax=0,edx=0;
	DWORD eax_in;
	DWORD eax_out;
	int activationTemperature;
	//ULONG result;
	int nMask = 1;
	int nTem=0;
	//��eax=0ִ��CPUIDָ��õ�֧�ֵ����ָ���� ���С��6һ����֧��dts
	eax_in= 0;
	eax_out= GetCpuInfo(eax_in);
	if (eax_out < 6)
	{
		bSupportDts = false;
	}
	else
	{
		//��eax=6ִ��CPUIDָ�eax ��һλ�Ƿ�Ϊ1�����Ϊ1��ʾCPU֧��DTS
		eax_in= 6;
		eax_out= GetCpuInfo(eax_in);
		if (1 == (eax_out&0x00000001))
		{
			bSupportDts = true;
		}
		else
		{
			bSupportDts = false;
		}
	}
	if (!bSupportDts)
	{
		return;
	}
	//��eax=1ִ��CPUIDָ��, ���õ�CPU Signature
	eax_in= 1;
	eax_out= GetCpuInfo(eax_in);
	// signature Ϊ 0x6f1�� 0x6f0�� CPU DTS ֱֵ�Ӵ���ǰ�¶ȶ�����Tjunction ���.
	//�� signature С�ڵ��� 0x6f4 �� Tjunction һֱΪ100
	if (0x6f1 == eax_out || 0x6f0 == eax_out)
	{
		for (int i=0; i<processor_count_; i++)
		{
			SetThreadAffinityMask(GetCurrentThread(),nMask);
			Rdmsr(0x19c,&eax,&edx);
			nTem += ((eax&0x007f0000)>>16);
			nMask *= 2;
		}
		nTem /= processor_count_;
	}
	else
	{
		if (eax_out <= 0x6f4)
		{
			activationTemperature = 100;
		}
		else
		{
			Rdmsr(0x1a2,&eax,&edx);
			activationTemperature= (eax&0x007f0000)>>16;
			if (0 == activationTemperature)
			{
				Rdmsr(0xee,&eax,&edx);
				if (1 == ((eax&0x20000000)>>30))
				{
					activationTemperature = 85;
				}
				else
				{
					activationTemperature = 100;
				}
			}
		}
		for (int i=0; i<processor_count_; i++)
		{
			SetThreadAffinityMask(GetCurrentThread(),nMask);
			Rdmsr(0x19c,&eax,&edx);
			nTem += (activationTemperature-((eax&0x007f0000)>>16));
			nMask *= 2;
		}
		nTem /= processor_count_;
	}
	nTempCpu = nTem;
}

void CpermoDlg::get_processor_number(void)
{
	SYSTEM_INFO info;  
	GetSystemInfo(&info);  
	processor_count_ =  (int)info.dwNumberOfProcessors;
}

void CpermoDlg::TimerCallbackTemp(DWORD dwUser)
{
	CpermoDlg* p = (CpermoDlg *)dwUser; 
	if (gIsMsr && bShowTempInfo)
	{
		p->GetCpuTemp();
	}

	if (!(p->bAutoHide && p->rCurPos.left < 0))
	{
		//��ȡCPUռ��
		::GetSystemTimes(&(p->idleTime), &(p->kernelTime), &(p->userTime));
		int idle = (int)CompareFileTime(p->preidleTime, p->idleTime);
		int kernel = (int)CompareFileTime(p->prekernelTime, p->kernelTime);
		int user = (int)CompareFileTime(p->preuserTime, p->userTime);
		int cpu = 0;
		//���߻��ѣ�Ϊ0���������
		if ((kernel + user) != 0)
		{
			cpu = (kernel + user - idle) * 100 / (kernel + user);
		}
		//int cpuidle = (idle)* 100 / (kernel + user);
		p->preidleTime = p->idleTime;
		p->prekernelTime = p->kernelTime;
		p->preuserTime = p->userTime;
		if (cpu < 0)
		{
			cpu = -cpu;
		}
		p->nCPU = cpu;
		p->strCPU.Format(_T("%d%%"), p->nCPU);
	}
	{
		Lock();
		// Update Rate
		for(int i = 0; i < vecProInfo.size(); i++)
		{
			CProInfo *item = vecProInfo[i];

			item->prevTxRate = item->txRate;
			item->prevRxRate = item->rxRate;

			item->txRate = 0;
			item->rxRate = 0;
		}
		Unlock();
		p->fNetDown = 0;
		p->fNetUp = 0;
		for (int i=0; i<vecProInfo.size(); i++)
		{
			p->fNetDown += vecProInfo[i]->prevRxRate;
			p->fNetUp += vecProInfo[i]->prevTxRate;
			// 		printf("puid:%d  pid:%d\n", vProInfo[i].puid, vProInfo[i].pid);
			// 		printf("down:%d.%dK/s--", vProInfo[i].prevTxRate / 1024, (vProInfo[i].prevTxRate % 1024 + 51) / 108);
			// 		printf("up:%d.%dK/s\n", vProInfo[i].prevRxRate / 1024, (vProInfo[i].prevRxRate % 1024 + 51) / 108);
		}
		p->fNetDown /= 1024;
		p->fNetUp /= 1024;
	}
	if (!(p->bAutoHide && p->rCurPos.right > p->rWorkArea.right))
	{
		//��ȡ�ڴ�ʹ����
		MEMORYSTATUSEX memStatex;
		memStatex.dwLength = sizeof(memStatex);
		::GlobalMemoryStatusEx(&memStatex);
		p->nMem = memStatex.dwMemoryLoad;
		p->strMem.Format(_T("%d%%"), p->nMem);
	}
	// 		//�ڴ�ʹ���ʡ�
	// 			printf("�ڴ�ʹ����:\t%d%%\r\n", memStatex.dwMemoryLoad);
	// 			//�ܹ������ڴ档
	// 			printf("�ܹ������ڴ�:\t%I64d\r\n", memStatex.ullTotalPhys);
	// 			//���������ڴ档
	// 			printf("���������ڴ�:\t%I64d\r\n", memStatex.ullAvailPhys);
	// 			//ȫ���ڴ档
	// 			printf("ȫ���ڴ�:\t%I64d\r\n", memStatex.ullTotalPageFile);
	// 			//ȫ�����õ��ڴ档
	// 			printf("ȫ�����õ��ڴ�:\t%I64d\r\n", memStatex.ullAvailPageFile);
	// 			//ȫ���������ڴ档
	// 			printf("ȫ�����ڴ�:\t%I64d\r\n", memStatex.ullTotalVirtual);
	if (bShowTempInfo && ((p->bShowBand && p->bBandShowDiskTem) || p->bInfoDlgShowing))
	{
		p->GetDiskTem();
	}

	CString strBandNetUp;
	CString strBandNetDown;

	if (p->fNetUp >= 1000)
	{
		p->strNetUp.Format(_T("%.2fMB/S"), p->fNetUp/1024.0);
		strBandNetUp.Format(_T("U:%.2fM/S"), p->fNetUp/1024.0);
	}
	else
	{
		if (p->fNetUp < 100)
		{
			p->strNetUp.Format(_T("%.1fKB/S"), p->fNetUp);
			strBandNetUp.Format(_T("U:%.1fK/S"), p->fNetUp);
		}
		else
		{
			p->strNetUp.Format(_T("%.0fKB/S"), p->fNetUp);
			strBandNetUp.Format(_T("U:%.0fK/S"), p->fNetUp);
		}
	}
	if (p->fNetDown >= 1000)
	{
		p->strNetDown.Format(_T("%.2fMB/S"), p->fNetDown/1024.0);
		strBandNetDown.Format(_T("D:%.2fM/S"), p->fNetDown/1024.0);
	}
	else
	{
		if (p->fNetDown < 100)
		{
			p->strNetDown.Format(_T("%.1fKB/S"), p->fNetDown);
			strBandNetDown.Format(_T("D:%.1fK/S"), p->fNetDown);
		}
		else
		{
			p->strNetDown.Format(_T("%.0fKB/S"), p->fNetDown);
			strBandNetDown.Format(_T("D:%.0fK/S"), p->fNetDown);
		}
	}
	if (p->bShowBand)
	{
		if (p->vBandShow.size() == 0)
		{
			nBandShow = 100;
			p->nCount = 0;
		}
		else
		{
			if (p->nCount%4 == 0)
			{
				nBandShow = p->vBandShow[p->nNowBandShowIndex++];
				if (p->vBandShow.size() == p->nNowBandShowIndex)
				{
					p->nNowBandShowIndex = 0;
				}
				p->nCount = 0;
			}
			p->nCount++;
		}
		if (0 == nBandShow)
		{
			p->pcoControl->SetPosEx(p->nCPU);
			p->pcoControl->ShowMyText(p->strCPU, true);
		}
		else if (1 == nBandShow)
		{
			p->pcoControl->SetPosEx(p->nMem);
			p->pcoControl->ShowMyText(p->strMem, true);
		}
		else if (2 == nBandShow)
		{
			p->pcoControl->SetPosEx(0);
			p->pcoControl->ShowMyText(strBandNetDown, true);
		}
		else if (3 == nBandShow)
		{
			p->pcoControl->SetPosEx(0);
			p->pcoControl->ShowMyText(strBandNetUp, true);
		}
		else if (4 == nBandShow)
		{
			CString strDiskTem;
			bShowTempInfo?strDiskTem.Format(_T("Ӳ��:%d��"), nTempDisk):strDiskTem.Format(_T("Ӳ��:--��"));
			p->pcoControl->SetPosEx(0);
			p->pcoControl->ShowMyText(strDiskTem, true);
		}
		else if (5 == nBandShow)
		{
			CString strCpuTem;
			bShowTempInfo?strCpuTem.Format(_T("CPU:%d��"), nTempCpu):strCpuTem.Format(_T("CPU:--��"));
			p->pcoControl->SetPosEx(0);
			p->pcoControl->ShowMyText(strCpuTem, true);
		}
		else
		{
			p->pcoControl->SetPosEx(0);
			p->pcoControl->ShowMyText(_T("δѡ��"), true);
		}
	}
	p->Invalidate(FALSE);
}

