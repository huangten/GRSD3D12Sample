#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // �� Windows ͷ���ų�����ʹ�õ�����
#include <windows.h>
#include <tchar.h>
#include <atlconv.h>
#include <atlcoll.h>  //for atl array
#include <strsafe.h>  //for StringCchxxxxx function
#include <time.h>	// for time

#define GRS_WND_CLASS_NAME _T("GRS Game Window Class")
#define GRS_WND_TITLE	_T("GRS DirectX12 Render Thread Pool Based On Barrier Kernel Objects Sample")

//����̨�������
#define GRS_INIT_OUTPUT() 	if (!::AllocConsole()){throw CGRSCOMException(HRESULT_FROM_WIN32(::GetLastError()));}\
		SetConsoleTitle(GRS_WND_TITLE);
#define GRS_FREE_OUTPUT()	::_tsystem(_T("PAUSE"));\
							::FreeConsole();

#define GRS_USEPRINTF() TCHAR pBuf[1024] = {};TCHAR pszOutput[1024] = {};
#define GRS_PRINTF(...) \
    StringCchPrintf(pBuf,1024,__VA_ARGS__);\
	StringCchPrintf(pszOutput,1024,_T("��ID:% 8u����%s"),::GetCurrentThreadId(),pBuf);\
    WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE),pszOutput,lstrlen(pszOutput),nullptr,nullptr);

//���õȴ��߳̾���Լ���ȷ��ʱһ���ĺ�����,�����������ȷ��ϵͳ����Sleep
#define GRS_SLEEP(dwMilliseconds) WaitForSingleObject(GetCurrentThread(),dwMilliseconds)

class CGRSCOMException
{
public:
	CGRSCOMException(HRESULT hr) : m_hrError(hr)
	{
	}
	HRESULT Error() const
	{
		return m_hrError;
	}
private:
	const HRESULT m_hrError;
};

// ��Ⱦ���̲߳���
struct ST_GRS_THREAD_PARAMS
{
	UINT								m_nThreadIndex;				//���
	DWORD								m_dwThisThreadID;
	HANDLE								m_hThisThread;
	DWORD								m_dwMainThreadID;
	HANDLE								m_hMainThread;
	//��ǰ��ȾPass����ţ���Ȼ��������⣬��ȫ�����ø��������String������Pass����ţ�ʹ��map������Pass���ƺͶ�Ӧ����Դ
	UINT								m_nPassNO;					
 };

// ����������Ҫg_nMaxThread����Ⱦ���̣߳�����Ҫ��̬��������Ҫ����Ⱦ���̲߳����ö�̬�����װ���磺CAtlArray��stl::vector��
const UINT								g_nMaxThread = 64;
ST_GRS_THREAD_PARAMS					g_stThreadParams[g_nMaxThread] = {};

// �̳߳ع����ں�ͬ������
HANDLE									g_hEventPassOver;
HANDLE									g_hSemaphore;
SYNCHRONIZATION_BARRIER					g_stCPUThdBarrier = {};


UINT __stdcall RenderThread(void* pParam);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR    lpCmdLine, int nCmdShow)
{
	int						iWndWidth = 1024;
	int						iWndHeight = 768;
	HWND					hWnd = nullptr;
	MSG						msg = {};

	UINT					nMaxPass = 5; //����һ֡��Ⱦ��Ҫ����5����Ⱦ��5 Pass�������Ը��������Ⱦ�ַ���technology������̬����
	UINT					nPassNO = 0;    //��ǰ��ȾPass�����
	UINT					nFrameCnt = 0; //����Ⱦ֡��

	CAtlArray<HANDLE>		arHWaited;
	CAtlArray<HANDLE>		arHSubThread;
	GRS_USEPRINTF();
	try
	{
		
		GRS_INIT_OUTPUT();
		// ��������
		{
			WNDCLASSEX wcex = {};
			wcex.cbSize = sizeof(WNDCLASSEX);
			wcex.style = CS_GLOBALCLASS;
			wcex.lpfnWndProc = WndProc;
			wcex.cbClsExtra = 0;
			wcex.cbWndExtra = 0;
			wcex.hInstance = hInstance;
			wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
			wcex.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);		//��ֹ���ĵı����ػ�
			wcex.lpszClassName = GRS_WND_CLASS_NAME;
			RegisterClassEx(&wcex);

			DWORD dwWndStyle = WS_OVERLAPPED | WS_SYSMENU;
			RECT rtWnd = { 0, 0, iWndWidth, iWndHeight };
			AdjustWindowRect(&rtWnd, dwWndStyle, FALSE);

			// ���㴰�ھ��е���Ļ����
			INT posX = (GetSystemMetrics(SM_CXSCREEN) - rtWnd.right - rtWnd.left) / 2;
			INT posY = (GetSystemMetrics(SM_CYSCREEN) - rtWnd.bottom - rtWnd.top) / 2;

			hWnd = CreateWindowW(GRS_WND_CLASS_NAME, GRS_WND_TITLE, dwWndStyle
				, posX, posY, rtWnd.right - rtWnd.left, rtWnd.bottom - rtWnd.top
				, nullptr, nullptr, hInstance, nullptr);

			if (!hWnd)
			{
				throw CGRSCOMException(HRESULT_FROM_WIN32(GetLastError()));
			}

			// ��������д���
			ShowWindow(GetConsoleWindow(), SW_MAXIMIZE);    //�������
		}

		// ��������CPU����Ⱦ�̳߳ص�ͬ������
		{
			// ֪ͨ�̳߳�������Ⱦ���ű�������ʼ�ű����Ϊ0������ű�������̳߳��̸߳���
			g_hSemaphore = ::CreateSemaphore(nullptr, 0, g_nMaxThread, nullptr);
			if (!g_hSemaphore)
			{
				throw CGRSCOMException(HRESULT_FROM_WIN32(GetLastError()));
			}
			
			// �̳߳�Pass��Ⱦ�����¼����
			g_hEventPassOver = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (!g_hEventPassOver)
			{
				throw CGRSCOMException(HRESULT_FROM_WIN32(GetLastError()));
			}

			// ����CPU�߳����϶������������߳������̳߳��̸߳���
			if (!InitializeSynchronizationBarrier(&g_stCPUThdBarrier, g_nMaxThread, -1))
			{
				throw CGRSCOMException(HRESULT_FROM_WIN32(GetLastError()));
			}
		}

		// ���������Ⱦ�߳�
		{
			GRS_PRINTF(_T("���߳�������%d�������߳�\n"),g_nMaxThread);
			// ���ø��̵߳Ĺ��Բ���
			for (int i = 0; i < g_nMaxThread; i++)
			{
				//�����߳���ţ�ʹ��CPU�̳߳ؿ���ȥ���GPU�ϵ�Computer Engine��ÿ���̶߳������
				//��Ȼ�������ˣ���Ҳ���Խ�һ��ģ�±���ΪGPU Thread Box��3D���
				g_stThreadParams[i].m_nThreadIndex = i;		
				//��ʼ����Ⱦ���������Ϊ0��ֻҪ����Ⱦ���ٶ�Ӧ����һ����Ⱦ����Ȼ������Ⱦ�̳߳ظ��
				g_stThreadParams[i].m_nPassNO = 0;			

				//����ͣ��ʽ�����̣߳��̵߳���ͣ������ʽ��һ�����ŵķ�ʽ�����̳���������Ϊɶ��
				g_stThreadParams[i].m_hThisThread = (HANDLE)_beginthreadex(nullptr,
					0, RenderThread, (void*)&g_stThreadParams[i],
					CREATE_SUSPENDED, (UINT*)&g_stThreadParams[i].m_dwThisThreadID);

				//Ȼ���ж��̴߳����Ƿ�ɹ�
				if (nullptr == g_stThreadParams[i].m_hThisThread
					|| reinterpret_cast<HANDLE>(-1) == g_stThreadParams[i].m_hThisThread)
				{
					throw CGRSCOMException(HRESULT_FROM_WIN32(GetLastError()));
				}

				arHSubThread.Add(g_stThreadParams[i].m_hThisThread);
			}

			//��һ�����߳�
			for (int i = 0; i < g_nMaxThread; i++)
			{
				::ResumeThread(g_stThreadParams[i].m_hThisThread);
			}
		}

		DWORD dwRet = 0;
		BOOL bExit = FALSE;

		// ����һ����ʱ������ģ��GPU��Ⱦʱ����ʱ
		HANDLE hTimer = CreateWaitableTimer(NULL, FALSE, NULL);
		LARGE_INTEGER liDueTime = {};
		liDueTime.QuadPart = -1i64;//1���ʼ��ʱ
		SetWaitableTimer(hTimer, &liDueTime, 10, NULL, NULL, 0);

		//������Ӷ�ʱ���������˼��ʾģ��GPU���еڶ���Copy��������ʱ
		arHWaited.Add(hTimer);

		// ��ʼ����������ʾ����
		ShowWindow(hWnd, nCmdShow);
		UpdateWindow(hWnd);

		//���������������������
		srand((unsigned)time(NULL));

		GRS_PRINTF(_T("���߳̿�ʼ��Ϣѭ��:\n"));
		
		//15����ʼ����Ϣѭ�����������в�����Ⱦ
		while (!bExit)
		{
			//���߳̽���ȴ�
			// ע������ fWaitAll = FALSE�ˣ�
			// ������ʵ�������ǿ�����arHWaited�������������������Ҫ�ȴ��Ķ�����
			// ������ӵľ�������Ե�����ʹMsgWaitForMultipleObjects����
			// �Ӷ��������÷���ֵ��ȷ��Ч�Ĵ����Ӧ�����ʾ���¼�
			// �磺Input��Music��Sound��Net��Video�ȵ�
			dwRet = ::MsgWaitForMultipleObjects(static_cast<DWORD>(arHWaited.GetCount()), arHWaited.GetData(), FALSE, INFINITE, QS_ALLINPUT);
			dwRet -= WAIT_OBJECT_0;
			switch (dwRet)
			{
			case 0:
			{
				if ( nPassNO < nMaxPass )
				{//
					arHWaited.RemoveAll();
					// ׼���ȴ���n����Ⱦ����
					arHWaited.Add(g_hEventPassOver);

					GRS_PRINTF(_T("���߳�֪ͨ���߳̿�ʼ�ڡ�%d������Ⱦ\n"), nPassNO);

					//֪ͨ���߳̿�ʼ��Ⱦ
					for (int i = 0; i < g_nMaxThread; i++)
					{
						//�趨���߳���ȾPass���
						g_stThreadParams[i].m_nPassNO = nPassNO;
					}
					// �ͷ��ű�����������߳������൱��֪ͨ�������߳̿�ʼ��Ⱦ����
					ReleaseSemaphore(g_hSemaphore, g_nMaxThread, nullptr);

					GRS_PRINTF(_T("  ���߳̽��еڡ�%d������Ⱦ\n"), nPassNO);
					// Sleep(20ms) ģ�����̵߳ĵ�nPassNo����Ⱦ
					GRS_SLEEP(20);

					++nPassNO; //��������һ����Ⱦ
				}
				else
				{// һ֡��Ⱦ�����ˣ�����ExecuteCommandLists��
					arHWaited.RemoveAll();
					// ģ��ȴ�GPUͬ��
					arHWaited.Add(hTimer);
					
					// ��������Զ�̬�ĵ�����һ֡��Ҫ��Pass�����������������ø�5���ڵ������ģ���������
					nMaxPass = rand() % 5 + 1;
					nPassNO = 0;

					GRS_PRINTF(_T("���߳���ɵڡ�%d��֡��Ⱦ����һ֡��Ҫ��%d������Ⱦ\n"), nFrameCnt,nMaxPass);
					++nFrameCnt;
				}
			}
			break;
			case 1:
			{
				GRS_PRINTF(_T("���̣߳��ڡ�%d������Ⱦ�У���������Ϣ\n"), nPassNO);
				//������Ϣ
				while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE | PM_NOYIELD))
				{
					if (WM_QUIT != msg.message)
					{
						::TranslateMessage(&msg);
						::DispatchMessage(&msg);
					}
					else
					{
						bExit = TRUE;
					}
				}
			}
			break;
			default:
			{

			}
			break;
			}

			//---------------------------------------------------------------------------------------------
			// ���һ���̵߳Ļ�����������߳��Ѿ��˳��ˣ����˳����߳�ѭ��
			// ��Ȼ��������Ը�����һ��ÿ�ζ���⣬Ϊ��ʾ������Ը�����������������n��ѭ������һ�Σ���һ֡��������һ�εȵ�
			// ����Ҳ�����������Ż����и��ӵ��̳߳��������������߳������������ȣ���̬������һ��ѭ��������һ��������Ҫ���̳߳��߳�������
			dwRet = WaitForMultipleObjects(static_cast<DWORD>(arHSubThread.GetCount()), arHSubThread.GetData(), FALSE, 0);
			dwRet -= WAIT_OBJECT_0;
			if (dwRet >= 0 && dwRet < g_nMaxThread)
			{
				bExit = TRUE;
			}
		}

	}
	catch (CGRSCOMException & e)
	{//������COM�쳣
		e;
	}

	try
	{
		GRS_PRINTF(_T("���߳�֪ͨ�����߳��˳�\n"));
		// ֪ͨ���߳��˳�
		for (int i = 0; i < g_nMaxThread; i++)
		{
			::PostThreadMessage(g_stThreadParams[i].m_dwThisThreadID, WM_QUIT, 0, 0);
		}

		// �ȴ��������߳��˳�
		DWORD dwRet = WaitForMultipleObjects(static_cast<DWORD>(arHSubThread.GetCount()), arHSubThread.GetData(), TRUE, INFINITE);

		// �����������߳���Դ
		for (int i = 0; i < g_nMaxThread; i++)
		{
			// �ر����߳��ں˶�����
			::CloseHandle(g_stThreadParams[i].m_hThisThread);
		}
		// �ر��ű���
		::CloseHandle(g_hSemaphore);
		// ɾ��CPU�߳�����
		DeleteSynchronizationBarrier(&g_stCPUThdBarrier);
		GRS_PRINTF(_T("�����߳�ȫ���˳������߳�������Դ���˳�\n"));
	}
	catch (CGRSCOMException & e)
	{//������COM�쳣
		e;
	}
	::CoUninitialize();
	GRS_FREE_OUTPUT();
	return 0;
}

UINT __stdcall RenderThread(void* pParam)
{
	ST_GRS_THREAD_PARAMS* pThdPms = static_cast<ST_GRS_THREAD_PARAMS*>(pParam);
	DWORD dwRet = 0;
	BOOL  bQuit = FALSE;
	MSG   msg = {};

	//���߳�����������ո�
	TCHAR pszPreSpace[] = _T("  ");
	
	GRS_USEPRINTF();
	
	try
	{
		if (nullptr == pThdPms)
		{//�����쳣�����쳣��ֹ�߳�
			throw CGRSCOMException(E_INVALIDARG);
		}

		GRS_PRINTF(_T("%s���̡߳�%d����ʼ������Ⱦѭ��\n"), pszPreSpace,pThdPms->m_nThreadIndex);
		//6����Ⱦѭ��
		while (!bQuit)
		{
			// �ȴ����߳�֪ͨ��ʼ��Ⱦ��ͬʱ���������߳�Post��������Ϣ��Ŀǰ����Ϊ�˵ȴ�WM_QUIT��Ϣ
			dwRet = ::MsgWaitForMultipleObjects(1, &g_hSemaphore, FALSE, INFINITE, QS_ALLPOSTMESSAGE);
			switch (dwRet - WAIT_OBJECT_0)
			{
			case 0:
			{
				// OnUpdate()
				{
					GRS_PRINTF(_T("%s���̡߳�%d��ִ�еڡ�%d������Ⱦ��OnUpdate()\n"), pszPreSpace, pThdPms->m_nThreadIndex, pThdPms->m_nPassNO);
				}

				// OnThreadRender()
				{
					GRS_PRINTF(_T("%s���̡߳�%d��ִ�еڡ�%d������Ⱦ��OnThreadRender()\n"), pszPreSpace, pThdPms->m_nThreadIndex, pThdPms->m_nPassNO);
				}

				//�ӳ�20���룬ģ�����߳���Ⱦ����
				GRS_SLEEP(20);

				//�����Ⱦ�����ر������б�������ͬ������֪ͨ���߳̿�ʼִ�У�

				if (EnterSynchronizationBarrier(&g_stCPUThdBarrier, 0))
				{
					GRS_PRINTF(_T("%s���̡߳�%d����֪ͨ���߳���ɵڡ�%d������Ⱦ\n"), pszPreSpace, pThdPms->m_nThreadIndex, pThdPms->m_nPassNO);

					SetEvent(g_hEventPassOver);
				}
			}
			break;
			case 1:
			{//������Ϣ
				GRS_PRINTF(_T("%s���̡߳�%d����������Ϣ\n"), pszPreSpace, pThdPms->m_nThreadIndex);
				while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
				{//����ֻ�����Ǳ���̷߳���������Ϣ�����ڸ����ӵĳ���
					if (WM_QUIT != msg.message)
					{
						::TranslateMessage(&msg);
						::DispatchMessage(&msg);
					}
					else
					{
						bQuit = TRUE;
					}
				}
			}
			break;
			case WAIT_TIMEOUT:
				break;
			default:
				break;
			}
		}

		GRS_PRINTF(_T("%s���̡߳�%d���˳�\n"), pszPreSpace, pThdPms->m_nThreadIndex);
	}
	catch (CGRSCOMException&)
	{

	}

	return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_KEYDOWN:
	{

	}
	break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
