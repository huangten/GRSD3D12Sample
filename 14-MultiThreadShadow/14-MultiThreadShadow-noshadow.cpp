#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // �� Windows ͷ���ų�����ʹ�õ�����
#include <windows.h>
#include <tchar.h>
#include <wrl.h> //���WTL֧�� ����ʹ��COM
#include <atlconv.h>
#include <atlcoll.h>  //for atl array
#include <strsafe.h>  //for StringCchxxxxx function
#include <dxgi1_6.h>
#include <d3d12.h> //for d3d12
#include <d3dcompiler.h>
#if defined(_DEBUG)
#include <dxgidebug.h>
#endif
#include <DirectXMath.h>
#include "..\WindowsCommons\d3dx12.h"
#include "..\WindowsCommons\DDSTextureLoader12.h"

using namespace std;
using namespace Microsoft;
using namespace Microsoft::WRL;
using namespace DirectX;

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

#ifndef GRS_BLOCK

#define GRS_WND_CLASS_NAME _T("GRS Game Window Class")
#define GRS_WND_TITLE	_T("GRS DirectX12 Multi Thread Shadow Sample")

#define GRS_THROW_IF_FAILED(hr) {HRESULT _hr = (hr);if (FAILED(_hr)){ throw CGRSCOMException(_hr); }}

//�¶���ĺ�������ȡ������
#define GRS_UPPER_DIV(A,B) ((UINT)(((A)+((B)-1))/(B)))
//���������ϱ߽�����㷨 �ڴ�����г��� ���ס
#define GRS_UPPER(A,B) ((UINT)(((A)+((B)-1))&~(B - 1)))

//------------------------------------------------------------------------------------------------------------
// Ϊ�˵��Լ�����������������ͺ궨�壬Ϊÿ���ӿڶ����������ƣ�����鿴�������
#if defined(_DEBUG)
inline void GRS_SetD3D12DebugName(ID3D12Object* pObject, LPCWSTR name)
{
	pObject->SetName(name);
}

inline void GRS_SetD3D12DebugNameIndexed(ID3D12Object* pObject, LPCWSTR name, UINT index)
{
	WCHAR _DebugName[MAX_PATH] = {};
	if (SUCCEEDED(StringCchPrintfW(_DebugName, _countof(_DebugName), L"%s[%u]", name, index)))
	{
		pObject->SetName(_DebugName);
	}
}
#else

inline void GRS_SetD3D12DebugName(ID3D12Object*, LPCWSTR)
{
}
inline void GRS_SetD3D12DebugNameIndexed(ID3D12Object*, LPCWSTR, UINT)
{
}

#endif

#define GRS_SET_D3D12_DEBUGNAME(x)						GRS_SetD3D12DebugName(x, L#x)
#define GRS_SET_D3D12_DEBUGNAME_INDEXED(x, n)			GRS_SetD3D12DebugNameIndexed(x[n], L#x, n)

#define GRS_SET_D3D12_DEBUGNAME_COMPTR(x)				GRS_SetD3D12DebugName(x.Get(), L#x)
#define GRS_SET_D3D12_DEBUGNAME_INDEXED_COMPTR(x, n)	GRS_SetD3D12DebugNameIndexed(x[n].Get(), L#x, n)

#if defined(_DEBUG)
inline void GRS_SetDXGIDebugName(IDXGIObject* pObject, LPCWSTR name)
{
	size_t szLen = 0;
	StringCchLengthW(name, 50, &szLen);
	pObject->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(szLen - 1), name);
}

inline void GRS_SetDXGIDebugNameIndexed(IDXGIObject* pObject, LPCWSTR name, UINT index)
{
	size_t szLen = 0;
	WCHAR _DebugName[MAX_PATH] = {};
	if (SUCCEEDED(StringCchPrintfW(_DebugName, _countof(_DebugName), L"%s[%u]", name, index)))
	{
		StringCchLengthW(_DebugName, _countof(_DebugName), &szLen);
		pObject->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(szLen), _DebugName);
	}
}
#else

inline void GRS_SetDXGIDebugName(IDXGIObject*, LPCWSTR)
{
}
inline void GRS_SetDXGIDebugNameIndexed(IDXGIObject*, LPCWSTR, UINT)
{
}

#endif

#define GRS_SET_DXGI_DEBUGNAME(x)						GRS_SetDXGIDebugName(x, L#x)
#define GRS_SET_DXGI_DEBUGNAME_INDEXED(x, n)			GRS_SetDXGIDebugNameIndexed(x[n], L#x, n)

#define GRS_SET_DXGI_DEBUGNAME_COMPTR(x)				GRS_SetDXGIDebugName(x.Get(), L#x)
#define GRS_SET_DXGI_DEBUGNAME_INDEXED_COMPTR(x, n)		GRS_SetDXGIDebugNameIndexed(x[n].Get(), L#x, n)
//------------------------------------------------------------------------------------------------------------


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
#endif // !GRS_BLOCK

// ����ṹ
struct ST_GRS_VERTEX
{
public:
	XMFLOAT4 m_v4Position;	//m_v4Position
	XMFLOAT4 m_v4Normal;	//m_v4Normal
	XMFLOAT2 m_v2Texcoord;	//Texcoord
	XMFLOAT4 m_v4Tangent;	//Tangent
public:
	ST_GRS_VERTEX() :
		m_v4Position(0.0f, 0.0f, 0.0f, 1.0f),
		m_v4Normal(0.0f, 0.0f, 0.0f, 0.0f),
		m_v4Tangent(0.0f, 0.0f, 0.0f, 0.0f),
		m_v2Texcoord(0.0f, 0.0f)
	{
	}
	ST_GRS_VERTEX(float px, float py, float pz,
		float nx, float ny, float nz,
		float tx, float ty, float tz,
		float u, float v) :
		m_v4Position(px, py, pz,1.0f)
		,m_v4Normal(nx, ny, nz,0.0f)
		,m_v4Tangent(tx, ty, tz,0.0f)
		,m_v2Texcoord(u, v)
	{
	}
	ST_GRS_VERTEX(const ST_GRS_VERTEX& stV)
		:
		m_v4Position(stV.m_v4Position)
		, m_v4Normal(stV.m_v4Normal)
		, m_v4Tangent(stV.m_v4Tangent)
		, m_v2Texcoord(stV.m_v2Texcoord )
	{
	}
public:
	ST_GRS_VERTEX& operator = (const ST_GRS_VERTEX& stV)
	{
		m_v4Position = stV.m_v4Position;
		m_v4Normal = stV.m_v4Normal;
		m_v4Tangent = stV.m_v4Tangent;
		m_v2Texcoord = stV.m_v2Texcoord;
		return *this;
	}
};

// ����������
struct ST_GRS_MVP
{
	XMFLOAT4X4 g_mxModel;
	XMFLOAT4X4 g_mxView;
	XMFLOAT4X4 g_mxProjection;
};

struct ST_GRS_LIGHT
{
	XMFLOAT4 m_v4Position;
	XMFLOAT4 m_v4Direction;
	XMFLOAT4 m_v4Color;
	XMFLOAT4 m_v4Falloff;
	XMFLOAT4X4 m_mxView;
	XMFLOAT4X4 m_mxProjection;
};

//��Դ����
#define GRS_NUM_LIGHTS 3

//#pragma push(pack)
struct ST_GRS_LIGHTBUFFER
{
	XMFLOAT4 m_v4AmbientColor;
	ST_GRS_LIGHT m_stLights[GRS_NUM_LIGHTS];
	bool m_bIsSampleShadowMap;
};

// ��Ⱦ���̲߳���
struct ST_GRS_THREAD_PARAMS
{
	UINT								m_nThreadIndex;				//���
	DWORD								m_dwThisThreadID;
	HANDLE								m_hThisThread;
	DWORD								m_dwMainThreadID;
	HANDLE								m_hMainThread;

	HANDLE								m_hRunEvent;
	HANDLE								m_hEventShadowOver;
	HANDLE								m_hEventRenderOver;
	
	UINT								m_nCurrentFrameIndex;	//��ǰ��Ⱦ�󻺳����
	ULONGLONG							m_nStartTime;			//��ǰ֡��ʼʱ��
	ULONGLONG							m_nCurrentTime;			//��ǰʱ��

	XMFLOAT4							m_v4ModelPos;
	TCHAR								m_pszDiffuseFile[MAX_PATH];
	TCHAR								m_pszNormalFile[MAX_PATH];

	ID3D12Device4*						m_pID3D12Device4;
	ID3D12CommandAllocator*				m_pICmdAlloc;
	ID3D12GraphicsCommandList*			m_pICmdList;
	ID3D12RootSignature*				m_pIRSShadowPass;
	ID3D12PipelineState*				m_pIPSOShadowPass;
	ID3D12RootSignature*				m_pIRSSecondPass;
	ID3D12PipelineState*				m_pIPSOSecondPass;

	ID3D12DescriptorHeap*				m_pISampleHeap;
	
	ID3D12Resource*						m_pITexShadowMap;
	ID3D12Resource*						m_pICBLights;
};

int g_iWndWidth = 1024;
int g_iWndHeight = 768;

CD3DX12_VIEWPORT					g_stViewPort(0.0f, 0.0f, static_cast<float>(g_iWndWidth), static_cast<float>(g_iWndHeight));
CD3DX12_RECT						g_stScissorRect(0, 0, static_cast<LONG>(g_iWndWidth), static_cast<LONG>(g_iWndHeight));

//��ʼ��Ĭ���������λ��
XMFLOAT3							g_f3EyePos = XMFLOAT3(0.0f, 5.0f, -10.0f);  //�۾�λ��
XMFLOAT3							g_f3LockAt = XMFLOAT3(0.0f, 0.0f, 0.0f);    //�۾�������λ��
XMFLOAT3							g_f3HeapUp = XMFLOAT3(0.0f, 1.0f, 0.0f);    //ͷ�����Ϸ�λ��

float								g_fYaw = 0.0f;			// ����Z�����ת��.
float								g_fPitch = 0.0f;			// ��XZƽ�����ת��

double								g_fPalstance = 3.0f * XM_PI / 180.0f;	//������ת�Ľ��ٶȣ���λ������/��

XMFLOAT4X4							g_mxWorld = {}; //World Matrix
XMFLOAT4X4							g_mxView = {}; //World Matrix
XMFLOAT4X4							g_mxProjection = {}; //World Matrix

// ȫ���̲߳���
const UINT							g_nMaxThread = 3;
const UINT							g_nThdSphere = 0;
const UINT							g_nThdCube = 1;
const UINT							g_nThdPlane = 2;
ST_GRS_THREAD_PARAMS				g_stThreadParams[g_nMaxThread] = {};

const UINT							g_nFrameBackBufCount = 3u;
UINT								g_nRTVDescriptorSize = 0U;
ComPtr<ID3D12Resource>				g_pIARenderTargets[g_nFrameBackBufCount];

ComPtr<ID3D12DescriptorHeap>		g_pIRTVHeap;
ComPtr<ID3D12DescriptorHeap>		g_pIDSVHeap;				//��Ȼ�����������

TCHAR								g_pszAppPath[MAX_PATH] = {};

typedef CAtlArray<ST_GRS_VERTEX> CGRSMeshVertex;
typedef CAtlArray<UINT32>		 CGRSMeshIndex;

UINT __stdcall RenderThread(void* pParam);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

BOOL LoadBox(FLOAT width, FLOAT height, FLOAT depth, UINT32 numSubdivisions, CGRSMeshVertex& arVertex, CGRSMeshIndex& arIndices);
BOOL LoadSphere(FLOAT radius, UINT32 sliceCount, UINT32 stackCount, CGRSMeshVertex& arVertex, CGRSMeshIndex& arIndices);
BOOL LoadGrid(float width, float depth, UINT32 m, UINT32 n, CGRSMeshVertex& arVertex, CGRSMeshIndex& arIndices);

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR    lpCmdLine, int nCmdShow)
{
	::CoInitialize(nullptr);  //for WIC & COM

	HWND								hWnd = nullptr;
	MSG									msg = {};

	UINT								nDXGIFactoryFlags = 0U;
	const float							faClearColor[] = { 0.2f, 0.5f, 1.0f, 1.0f };

	ComPtr<IDXGIFactory5>				pIDXGIFactory5;
	ComPtr<IDXGIFactory6>				pIDXGIFactory6;
	ComPtr<IDXGIAdapter1>				pIAdapter1;
	ComPtr<ID3D12Device4>				pID3D12Device4;

	ComPtr<ID3D12CommandQueue>			pIMainCmdQueue;

	ComPtr<ID3D12Fence>					pIFence;
	UINT64								n64FenceValue = 1ui64;
	HANDLE								hEventFence = nullptr;

	ComPtr<IDXGISwapChain1>				pISwapChain1;
	ComPtr<IDXGISwapChain3>				pISwapChain3;

	ComPtr<ID3D12Resource>				pIDepthShadowBuffer;	//��Ӱ�õ���Ȼ�����
	ComPtr<ID3D12Resource>				pIDepthStencilBuffer;   //������Ⱦ��������建����

	UINT								m_nCurrentFrameIndex = 0;
	DXGI_FORMAT							emRTFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT							emDSFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DXGI_FORMAT							emDepthShadowFormat = DXGI_FORMAT_D32_FLOAT;

	ComPtr<ID3D12CommandAllocator>		pICmdAllocPre;
	ComPtr<ID3D12GraphicsCommandList>	pICmdListPre;

	ComPtr<ID3D12CommandAllocator>		pICmdAllocPost;
	ComPtr<ID3D12GraphicsCommandList>	pICmdListPost;

	CAtlArray<HANDLE>					arHWaited;
	CAtlArray<HANDLE>					arHSubThread;

	ComPtr<ID3D12RootSignature>			pIRSShadowPass;
	ComPtr<ID3D12PipelineState>			pIPSOShadowPass;
	ComPtr<ID3D12RootSignature>			pIRSSecondPass;
	ComPtr<ID3D12PipelineState>			pIPSOSecondPass;
	ComPtr<ID3D12DescriptorHeap>		pISampleHeap;

	ComPtr<ID3D12Resource>			    pICBLights;
	SIZE_T								szLightBuf = GRS_UPPER(sizeof(ST_GRS_LIGHTBUFFER), 256);
	ST_GRS_LIGHTBUFFER*					pstLights = nullptr;

	try
	{
		// �õ���ǰ�Ĺ���Ŀ¼����������ʹ�����·�������ʸ�����Դ�ļ�
		{
			if (0 == ::GetModuleFileName(nullptr, g_pszAppPath, MAX_PATH))
			{
				GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
			}

			WCHAR* lastSlash = _tcsrchr(g_pszAppPath, _T('\\'));
			if (lastSlash)
			{//ɾ��Exe�ļ���
				*(lastSlash) = _T('\0');
			}

			lastSlash = _tcsrchr(g_pszAppPath, _T('\\'));
			if (lastSlash)
			{//ɾ��x64·��
				*(lastSlash) = _T('\0');
			}

			lastSlash = _tcsrchr(g_pszAppPath, _T('\\'));
			if (lastSlash)
			{//ɾ��Debug �� Release·��
				*(lastSlash + 1) = _T('\0');
			}
		}

		//1����������
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
			RECT rtWnd = { 0, 0, g_iWndWidth, g_iWndHeight };
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
		}

		//2������ʾ��ϵͳ�ĵ���֧��
		{
#if defined(_DEBUG)
			ComPtr<ID3D12Debug> debugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();
				// �򿪸��ӵĵ���֧��
				nDXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			}
#endif
		}

		//3������DXGI Factory����
		{
			GRS_THROW_IF_FAILED(CreateDXGIFactory2(nDXGIFactoryFlags, IID_PPV_ARGS(&pIDXGIFactory5)));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pIDXGIFactory5);
			// �ر�ALT+ENTER���л�ȫ���Ĺ��ܣ���Ϊ����û��ʵ��OnSize���������ȹر�
			GRS_THROW_IF_FAILED(pIDXGIFactory5->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
			//��ȡIDXGIFactory6�ӿ�
			GRS_THROW_IF_FAILED(pIDXGIFactory5.As(&pIDXGIFactory6));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pIDXGIFactory6);
		}

		//4��ö�������������豸
		{//ѡ��NUMA�ܹ��Ķ���������3D�豸����,��ʱ�Ȳ�֧�ּ����ˣ���Ȼ������޸���Щ��Ϊ

			GRS_THROW_IF_FAILED(pIDXGIFactory6->EnumAdapterByGpuPreference(0
				, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
				, IID_PPV_ARGS(&pIAdapter1)));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pIAdapter1);
			// ����D3D12.1���豸
			GRS_THROW_IF_FAILED(D3D12CreateDevice(pIAdapter1.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&pID3D12Device4)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pID3D12Device4);

	
			DXGI_ADAPTER_DESC1 stAdapterDesc = {};
			TCHAR pszWndTitle[MAX_PATH] = {};		
			GRS_THROW_IF_FAILED(pIAdapter1->GetDesc1(&stAdapterDesc));
			::GetWindowText(hWnd, pszWndTitle, MAX_PATH);
			StringCchPrintf(pszWndTitle
				, MAX_PATH
				, _T("%s(GPU[0]:%s)")
				, pszWndTitle
				, stAdapterDesc.Description);
			::SetWindowText(hWnd, pszWndTitle);
		}

		//5������ֱ���������
		{
			D3D12_COMMAND_QUEUE_DESC stQueueDesc = {};
			stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandQueue(&stQueueDesc, IID_PPV_ARGS(&pIMainCmdQueue)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIMainCmdQueue);
		}

		//6������ֱ�������б�
		{
			// Ԥ���������б�
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT
				, IID_PPV_ARGS(&pICmdAllocPre)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICmdAllocPre);
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT
				, pICmdAllocPre.Get(), nullptr, IID_PPV_ARGS(&pICmdListPre)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICmdListPre);

			//���������б�
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT
				, IID_PPV_ARGS(&pICmdAllocPost)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICmdAllocPost);
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT
				, pICmdAllocPost.Get(), nullptr, IID_PPV_ARGS(&pICmdListPost)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICmdListPost);
		}

		//7������Χ������
		{
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pIFence)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIFence);

			//����һ��Eventͬ���������ڵȴ�Χ���¼�֪ͨ
			hEventFence = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (hEventFence == nullptr)
			{
				GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
			}
		}

		//8������������
		{
			DXGI_SWAP_CHAIN_DESC1 stSwapChainDesc = {};
			stSwapChainDesc.BufferCount = g_nFrameBackBufCount;
			stSwapChainDesc.Width = g_iWndWidth;
			stSwapChainDesc.Height = g_iWndHeight;
			stSwapChainDesc.Format = emRTFormat;
			stSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			stSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			stSwapChainDesc.SampleDesc.Count = 1;

			GRS_THROW_IF_FAILED(pIDXGIFactory5->CreateSwapChainForHwnd(
				pIMainCmdQueue.Get(),		// Swap chain needs the queue so that it can force a flush on it.
				hWnd,
				&stSwapChainDesc,
				nullptr,
				nullptr,
				&pISwapChain1
			));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pISwapChain1);

			//ע��˴�ʹ���˸߰汾��SwapChain�ӿڵĺ���
			GRS_THROW_IF_FAILED(pISwapChain1.As(&pISwapChain3));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pISwapChain3);

			// ��ȡ��ǰ��һ�������Ƶĺ󻺳����
			m_nCurrentFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

			//����RTV(��ȾĿ����ͼ)��������(����ѵĺ���Ӧ�����Ϊ������߹̶���СԪ�صĹ̶���С�Դ��)
			D3D12_DESCRIPTOR_HEAP_DESC stRTVHeapDesc = {};
			stRTVHeapDesc.NumDescriptors = g_nFrameBackBufCount;
			stRTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			stRTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(&stRTVHeapDesc, IID_PPV_ARGS(&g_pIRTVHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(g_pIRTVHeap);

			//�õ�ÿ��������Ԫ�صĴ�С
			g_nRTVDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

			//---------------------------------------------------------------------------------------------
			CD3DX12_CPU_DESCRIPTOR_HANDLE stRTVHandle(g_pIRTVHeap->GetCPUDescriptorHandleForHeapStart());
			for (UINT i = 0; i < g_nFrameBackBufCount; i++)
			{//���ѭ����©����������ʵ�����Ǹ�����ı���
				GRS_THROW_IF_FAILED(pISwapChain3->GetBuffer(i, IID_PPV_ARGS(&g_pIARenderTargets[i])));
				GRS_SET_D3D12_DEBUGNAME_INDEXED_COMPTR(g_pIARenderTargets, i);
				pID3D12Device4->CreateRenderTargetView(g_pIARenderTargets[i].Get(), nullptr, stRTVHandle);
				stRTVHandle.Offset(1, g_nRTVDescriptorSize);
			}
		}

		//9��������Ȼ��弰��Ȼ�����������
		{
			D3D12_CLEAR_VALUE stDepthOptimizedClearValue = {};
			stDepthOptimizedClearValue.Format = emDepthShadowFormat;
			stDepthOptimizedClearValue.DepthStencil.Depth = 1.0f;
			stDepthOptimizedClearValue.DepthStencil.Stencil = 0;

			//����������Ӱ��Ⱦ����Ȼ�����
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT)
				, D3D12_HEAP_FLAG_NONE
				, &CD3DX12_RESOURCE_DESC::Tex2D(emDepthShadowFormat
					, g_iWndWidth
					, g_iWndHeight
					, 1
					, 0
					, 1
					, 0
					, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
				, D3D12_RESOURCE_STATE_DEPTH_WRITE
				, &stDepthOptimizedClearValue
				, IID_PPV_ARGS(&pIDepthShadowBuffer)
			));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDepthShadowBuffer);

			stDepthOptimizedClearValue.Format = emDSFormat;

			// �����ڶ���������Ⱦ�õ�������建����
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT)
				, D3D12_HEAP_FLAG_NONE
				, &CD3DX12_RESOURCE_DESC::Tex2D(emDSFormat
					, g_iWndWidth
					, g_iWndHeight
					, 1
					, 0
					, 1
					, 0
					, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)
				, D3D12_RESOURCE_STATE_DEPTH_WRITE
				, &stDepthOptimizedClearValue
				, IID_PPV_ARGS(&pIDepthStencilBuffer)
			));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDepthStencilBuffer);


			D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
			dsvHeapDesc.NumDescriptors = 2;		// 1 Shadow Depth View + 1 Depth Stencil View
			dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
			dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&g_pIDSVHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(g_pIDSVHeap);

			D3D12_DEPTH_STENCIL_VIEW_DESC stDepthStencilDesc = {};
			stDepthStencilDesc.Format = emDepthShadowFormat;
			stDepthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			stDepthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

			CD3DX12_CPU_DESCRIPTOR_HANDLE stDSVHandle(g_pIDSVHeap->GetCPUDescriptorHandleForHeapStart());
			
			pID3D12Device4->CreateDepthStencilView(pIDepthShadowBuffer.Get()
				, &stDepthStencilDesc
				, stDSVHandle);

			stDSVHandle.Offset(1, pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV));

			stDepthStencilDesc.Format = emDSFormat;

			pID3D12Device4->CreateDepthStencilView(pIDepthStencilBuffer.Get()
				, &stDepthStencilDesc
				, stDSVHandle);

		}

		//10��������ǩ��
		{//��������У���������ʹ����ͬ�ĸ�ǩ������Ϊ��Ⱦ��������Ҫ�Ĳ�����һ����
			D3D12_FEATURE_DATA_ROOT_SIGNATURE stFeatureData = {};
			// ����Ƿ�֧��V1.1�汾�ĸ�ǩ��
			stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
			if (FAILED(pID3D12Device4->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &stFeatureData, sizeof(stFeatureData))))
			{
				stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
			}

			CD3DX12_DESCRIPTOR_RANGE1 stDSPRanges[3];
			stDSPRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 2, 0);
			stDSPRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0);
			stDSPRanges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 2, 0);

			CD3DX12_ROOT_PARAMETER1 stRootParameters[3];
			stRootParameters[0].InitAsDescriptorTable(1, &stDSPRanges[0], D3D12_SHADER_VISIBILITY_ALL); //CBV������Shader�ɼ�
			stRootParameters[1].InitAsDescriptorTable(1, &stDSPRanges[1], D3D12_SHADER_VISIBILITY_PIXEL);//SRV��PS�ɼ�
			stRootParameters[2].InitAsDescriptorTable(1, &stDSPRanges[2], D3D12_SHADER_VISIBILITY_PIXEL);//SAMPLE��PS�ɼ�

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC stRootSignatureDesc;
			stRootSignatureDesc.Init_1_1(_countof(stRootParameters)
				, stRootParameters
				, 0
				, nullptr
				, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

			ComPtr<ID3DBlob> pISignatureBlob;
			ComPtr<ID3DBlob> pIErrorBlob;
			GRS_THROW_IF_FAILED(D3DX12SerializeVersionedRootSignature(&stRootSignatureDesc
				, stFeatureData.HighestVersion
				, &pISignatureBlob
				, &pIErrorBlob));

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateRootSignature(0
				, pISignatureBlob->GetBufferPointer()
				, pISignatureBlob->GetBufferSize()
				, IID_PPV_ARGS(&pIRSSecondPass)));

			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRSSecondPass);

			//----------------------------------------------------------------------------------------------------------------------------
			CD3DX12_DESCRIPTOR_RANGE1 stShadowSRVRanges[1];
			stShadowSRVRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

			CD3DX12_ROOT_PARAMETER1 stRSShadowParams[1];
			stRSShadowParams[0].InitAsDescriptorTable(1, &stShadowSRVRanges[0], D3D12_SHADER_VISIBILITY_VERTEX);
			
			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC stRSShasowDesc;
			stRSShasowDesc.Init_1_1(_countof(stRSShadowParams)
				, stRSShadowParams
				, 0
				, nullptr
				, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

			pISignatureBlob.Reset();
			pIErrorBlob.Reset();
			GRS_THROW_IF_FAILED(D3DX12SerializeVersionedRootSignature(&stRSShasowDesc
				, stFeatureData.HighestVersion
				, &pISignatureBlob
				, &pIErrorBlob));

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateRootSignature(0
				, pISignatureBlob->GetBufferPointer()
				, pISignatureBlob->GetBufferSize()
				, IID_PPV_ARGS(&pIRSShadowPass)));

			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRSShadowPass);
		}

		//11������Shader������Ⱦ����״̬����
		{
#if defined(_DEBUG)
			// Enable better shader debugging with the graphics debugging tools.
			UINT nShaderCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
			UINT nShaderCompileFlags = 0;
#endif
			//����Ϊ�о�����ʽ	   
			nShaderCompileFlags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

			TCHAR pszShaderFileName[MAX_PATH] = {};
			CHAR pszErrMsg[MAX_PATH] = {};
			ComPtr<ID3DBlob> pIVSCode;
			ComPtr<ID3DBlob> pIPSCode;
			ComPtr<ID3DBlob> pIErrMsg;
			StringCchPrintf(pszShaderFileName, MAX_PATH, _T("%s14-MultiThreadShadow\\Shader\\VSMain.hlsl"), g_pszAppPath);

			HRESULT hr = D3DCompileFromFile(pszShaderFileName, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE
				, "VSMain", "vs_5_0", nShaderCompileFlags, 0, &pIVSCode, &pIErrMsg);
			if (FAILED(hr))
			{
				StringCchPrintfA(pszErrMsg
					, MAX_PATH
					, "\n%s\n"
					, (CHAR*)pIErrMsg->GetBufferPointer());
				::OutputDebugStringA(pszErrMsg);
				throw CGRSCOMException(hr);
			}

			pIErrMsg.Reset();
			StringCchPrintf(pszShaderFileName, MAX_PATH, _T("%s14-MultiThreadShadow\\Shader\\PSMain.hlsl"), g_pszAppPath);

			hr = D3DCompileFromFile(pszShaderFileName, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE
				, "PSMain", "ps_5_0", nShaderCompileFlags, 0, &pIPSCode, &pIErrMsg);
			if (FAILED(hr))
			{
				StringCchPrintfA(pszErrMsg
					, MAX_PATH
					, "\n%s\n"
					, (CHAR*)pIErrMsg->GetBufferPointer());
				::OutputDebugStringA(pszErrMsg);
				throw CGRSCOMException(hr);
			}

			// ���Ƕ������һ�����ߵĶ��壬��ĿǰShader�����ǲ�û��ʹ��
			D3D12_INPUT_ELEMENT_DESC stIALayoutSphere[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "NORMAL",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,  0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
			};

			// ���� graphics pipeline state object (PSO)����
			D3D12_GRAPHICS_PIPELINE_STATE_DESC stPSODesc = {};
			stPSODesc.InputLayout = { stIALayoutSphere, _countof(stIALayoutSphere) };
			stPSODesc.pRootSignature = pIRSSecondPass.Get();
			stPSODesc.VS = CD3DX12_SHADER_BYTECODE(pIVSCode.Get());
			stPSODesc.PS = CD3DX12_SHADER_BYTECODE(pIPSCode.Get());
			stPSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			stPSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			stPSODesc.SampleMask = UINT_MAX;
			stPSODesc.SampleDesc.Count = 1;
			stPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			stPSODesc.NumRenderTargets = 1;
			stPSODesc.RTVFormats[0] = emRTFormat;
			stPSODesc.DSVFormat = emDSFormat;
			stPSODesc.DepthStencilState.StencilEnable = FALSE;
			stPSODesc.DepthStencilState.DepthEnable = TRUE;			//����Ȼ���				
			stPSODesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;//������Ȼ���д�빦��
			stPSODesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;     //��Ȳ��Ժ�������ֵΪ��ͨ����Ȳ��ԣ�����Сֵд�룩

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateGraphicsPipelineState(&stPSODesc
				, IID_PPV_ARGS(&pIPSOSecondPass)));

			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIPSOSecondPass);

			//---------------------------------------------------------------------------------------------
			// Shadow Pass Pipeline State Object
			stPSODesc.pRootSignature = pIRSShadowPass.Get();
			stPSODesc.PS = CD3DX12_SHADER_BYTECODE(0, 0);
			stPSODesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
			stPSODesc.DSVFormat = emDepthShadowFormat;
			stPSODesc.NumRenderTargets = 0;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateGraphicsPipelineState(&stPSODesc, IID_PPV_ARGS(&pIPSOShadowPass)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIPSOShadowPass);

		}

		//12������Sample
		{
			D3D12_DESCRIPTOR_HEAP_DESC stSamplerHeapDesc = {};
			stSamplerHeapDesc.NumDescriptors = 2;  // 1 Wrap Sample + 1 Clamp Sample
			stSamplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			stSamplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(&stSamplerHeapDesc, IID_PPV_ARGS(&pISampleHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pISampleHeap);

			const UINT nSamplerDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
			CD3DX12_CPU_DESCRIPTOR_HANDLE stSamplerHandle(pISampleHeap->GetCPUDescriptorHandleForHeapStart());

			D3D12_SAMPLER_DESC stWrapSamplerDesc = {};
			stWrapSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			stWrapSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			stWrapSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			stWrapSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			stWrapSamplerDesc.MinLOD = 0;
			stWrapSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
			stWrapSamplerDesc.MipLODBias = 0.0f;
			stWrapSamplerDesc.MaxAnisotropy = 1;
			stWrapSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			stWrapSamplerDesc.BorderColor[0] 
				= stWrapSamplerDesc.BorderColor[1] 
				= stWrapSamplerDesc.BorderColor[2] 
				= stWrapSamplerDesc.BorderColor[3] = 0;
			pID3D12Device4->CreateSampler(&stWrapSamplerDesc, stSamplerHandle);

			stSamplerHandle.Offset(nSamplerDescriptorSize);

			D3D12_SAMPLER_DESC stClampSamplerDesc = {};
			stClampSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
			stClampSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			stClampSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			stClampSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			stClampSamplerDesc.MipLODBias = 0.0f;
			stClampSamplerDesc.MaxAnisotropy = 1;
			stClampSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			stClampSamplerDesc.BorderColor[0] 
				= stClampSamplerDesc.BorderColor[1] 
				= stClampSamplerDesc.BorderColor[2] 
				= stClampSamplerDesc.BorderColor[3] = 0;
			stClampSamplerDesc.MinLOD = 0;
			stClampSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
			pID3D12Device4->CreateSampler(&stClampSamplerDesc, stSamplerHandle);
		}

		//13������ȫ�ֵƹ�
		{
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
				, D3D12_HEAP_FLAG_NONE
				, &CD3DX12_RESOURCE_DESC::Buffer(szLightBuf) //ע�⻺��ߴ�����Ϊ256�߽�����С
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pICBLights)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICBLights);

			// Map ֮��Ͳ���Unmap�� ֱ�Ӹ������ݽ�ȥ ����ÿ֡������map-copy-unmap�˷�ʱ����
			GRS_THROW_IF_FAILED(pICBLights->Map(0, nullptr, reinterpret_cast<void**>(&pstLights)));

			pstLights->m_bIsSampleShadowMap = false; 
			pstLights->m_v4AmbientColor = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);

			for (int i = 0; i < GRS_NUM_LIGHTS; i++)
			{
				pstLights->m_stLights[i].m_v4Position = { 0.0f, 50.0f, -30.0f, 1.0f };
				pstLights->m_stLights[i].m_v4Direction = { 0.0, 0.0f, 1.0f, 0.0f };
				pstLights->m_stLights[i].m_v4Falloff = { 800.0f, 1.0f, 0.0f, 1.0f };
				pstLights->m_stLights[i].m_v4Color = { 0.7f, 0.7f, 0.7f, 1.0f };

				XMVECTOR eye = XMLoadFloat4(&pstLights->m_stLights[i].m_v4Position);
				XMVECTOR at = XMVectorAdd(eye, XMLoadFloat4(&pstLights->m_stLights[i].m_v4Direction));
				XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

				XMStoreFloat4x4(&pstLights->m_stLights[i].m_mxView
					, XMMatrixLookAtLH(eye, at, up));

				XMStoreFloat4x4(&pstLights->m_stLights[i].m_mxProjection
					, XMMatrixPerspectiveFovLH(XM_PIDIV4, (FLOAT)g_iWndWidth / (FLOAT)g_iWndHeight, 0.1f, 1000.0f));		
			}
		}

		//14��׼�����������������Ⱦ�߳�
		{
			USES_CONVERSION;
			// ������Բ���
			StringCchPrintf(g_stThreadParams[g_nThdSphere].m_pszDiffuseFile, MAX_PATH, _T("%sAssets\\Earth_512.dds"), g_pszAppPath);
			StringCchPrintf(g_stThreadParams[g_nThdSphere].m_pszNormalFile, MAX_PATH, _T("%sAssets\\Earth_512_Normal.dds"), g_pszAppPath);
			g_stThreadParams[g_nThdSphere].m_v4ModelPos = XMFLOAT4(2.0f, 2.0f, 0.0f, 1.0f);

			// ��������Բ���
			StringCchPrintf(g_stThreadParams[g_nThdCube].m_pszDiffuseFile, MAX_PATH, _T("%sAssets\\Cube.dds"), g_pszAppPath);
			StringCchPrintf(g_stThreadParams[g_nThdCube].m_pszNormalFile, MAX_PATH, _T("%sAssets\\Cube_NRM.dds"), g_pszAppPath);
			g_stThreadParams[g_nThdCube].m_v4ModelPos = XMFLOAT4(-2.0f, 2.0f, 0.0f, 1.0f);

			// ƽ����Բ���
			StringCchPrintf(g_stThreadParams[g_nThdPlane].m_pszDiffuseFile, MAX_PATH, _T("%sAssets\\Plane.dds"), g_pszAppPath);
			StringCchPrintf(g_stThreadParams[g_nThdPlane].m_pszNormalFile, MAX_PATH, _T("%sAssets\\Plane_NRM.dds"), g_pszAppPath);
			g_stThreadParams[g_nThdPlane].m_v4ModelPos = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);


			// ����Ĺ��Բ�����Ҳ���Ǹ��̵߳Ĺ��Բ���
			for (int i = 0; i < g_nMaxThread; i++)
			{
				g_stThreadParams[i].m_nThreadIndex = i;		//��¼���

				//����ÿ���߳���Ҫ�������б�͸����������
				GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT
					, IID_PPV_ARGS(&g_stThreadParams[i].m_pICmdAlloc)));
				GRS_SetD3D12DebugNameIndexed(g_stThreadParams[i].m_pICmdAlloc, _T("pIThreadCmdAlloc"), i);

				GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT
					, g_stThreadParams[i].m_pICmdAlloc, nullptr, IID_PPV_ARGS(&g_stThreadParams[i].m_pICmdList)));
				GRS_SetD3D12DebugNameIndexed(g_stThreadParams[i].m_pICmdList, _T("pIThreadCmdList"), i);

				g_stThreadParams[i].m_dwMainThreadID = ::GetCurrentThreadId();
				g_stThreadParams[i].m_hMainThread = ::GetCurrentThread();
				g_stThreadParams[i].m_hRunEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
				g_stThreadParams[i].m_hEventShadowOver = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
				g_stThreadParams[i].m_hEventRenderOver = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
				g_stThreadParams[i].m_pID3D12Device4 = pID3D12Device4.Get();

				g_stThreadParams[i].m_pIRSShadowPass = pIRSShadowPass.Get();
				g_stThreadParams[i].m_pIPSOShadowPass = pIPSOShadowPass.Get();
				g_stThreadParams[i].m_pIRSSecondPass = pIRSSecondPass.Get();
				g_stThreadParams[i].m_pIPSOSecondPass = pIPSOSecondPass.Get();
				g_stThreadParams[i].m_pISampleHeap = pISampleHeap.Get();
				g_stThreadParams[i].m_pITexShadowMap = pIDepthShadowBuffer.Get();
				g_stThreadParams[i].m_pICBLights = pICBLights.Get();

				arHWaited.Add(g_stThreadParams[i].m_hEventRenderOver); //��ӵ����ȴ�������

				//����ͣ��ʽ�����߳�
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

		//����Ϣѭ��״̬����:0����ʾ��Ҫ���GPU�ϵĵڶ���Copy���� 1����ʾ֪ͨ���߳�ȥ��Ⱦ 2����ʾ���߳���Ⱦ�����¼���������ύִ����
		UINT nStates = 0; //��ʼ״̬Ϊ0
		DWORD dwRet = 0;
		DWORD dwWaitCnt = 0;
		UINT64 n64fence = 0;

		CAtlArray<ID3D12CommandList*> arCmdList;

		BOOL bExit = FALSE;

		ULONGLONG n64tmFrameStart = ::GetTickCount64();
		ULONGLONG n64tmCurrent = n64tmFrameStart;
		//������ת�Ƕ���Ҫ�ı���
		double dModelRotationYAngle = 0.0f;

		//��ʼ��ʱ��ر�һ�����������б���Ϊ�����ڿ�ʼ��Ⱦ��ʱ����Ҫ��reset���ǣ�Ϊ�˷�ֹ�������Close
		GRS_THROW_IF_FAILED(pICmdListPre->Close());
		GRS_THROW_IF_FAILED(pICmdListPost->Close());

		SetTimer(hWnd, WM_USER + 100, 1, nullptr); //���Ϊ�˱�֤MsgWaitForMultipleObjects �� wait for all ΪTrueʱ �ܹ���ʱ����

		// ����͸�Ӿ���
		XMStoreFloat4x4(&g_mxProjection, XMMatrixPerspectiveFovLH(XM_PIDIV4 / 2, (FLOAT)g_iWndWidth / (FLOAT)g_iWndHeight, 0.1f, 1000.0f));

		// ��ʼ����������ʾ����
		ShowWindow(hWnd, nCmdShow);
		UpdateWindow(hWnd);

		//15����ʼ����Ϣѭ�����������в�����Ⱦ
		while (!bExit)
		{
			//���߳̽���ȴ�
			dwWaitCnt = static_cast<DWORD>(arHWaited.GetCount());
			dwRet = ::MsgWaitForMultipleObjects(dwWaitCnt, arHWaited.GetData(), TRUE, INFINITE, QS_ALLINPUT);
			dwRet -= WAIT_OBJECT_0;

			if (0 == dwRet)
			{
				switch (nStates)
				{
				case 0://״̬0����ʾ�ȵ������̼߳�����Դ��ϣ���ʱִ��һ�������б���ɸ����߳�Ҫ�����Դ�ϴ��ĵڶ���Copy����
				{

					arCmdList.RemoveAll();
					//ִ�������б�
					arCmdList.Add(g_stThreadParams[g_nThdSphere].m_pICmdList);
					arCmdList.Add(g_stThreadParams[g_nThdCube].m_pICmdList);
					arCmdList.Add(g_stThreadParams[g_nThdPlane].m_pICmdList);

					pIMainCmdQueue->ExecuteCommandLists(static_cast<UINT>(arCmdList.GetCount()), arCmdList.GetData());

					//---------------------------------------------------------------------------------------------
					// ��ʼͬ��GPU��CPU��ִ�У��ȼ�¼Χ�����ֵ
					n64fence = n64FenceValue;
					GRS_THROW_IF_FAILED(pIMainCmdQueue->Signal(pIFence.Get(), n64fence));
					n64FenceValue++;
					GRS_THROW_IF_FAILED(pIFence->SetEventOnCompletion(n64fence, hEventFence));

					nStates = 1;

					arHWaited.RemoveAll();
					arHWaited.Add(hEventFence);
				}
				break;
				case 1:// ״̬1 �ȵ��������ִ�н�����Ҳ��CPU�ȵ�GPUִ�н���������ʼ��һ����Ⱦ
				{
					//OnUpdate()
					{
						//����ʱ��Ļ������㶼���������߳���
						//��ʵ�����������н���ʱ��ֵҲ��Ϊһ��ÿ֡���µĲ��������̻߳�ȡ�����������߳�
						n64tmCurrent = ::GetTickCount();
						//������ת�ĽǶȣ���ת�Ƕ�(����) = ʱ��(��) * ���ٶ�(����/��)
						//�����������൱�ھ�����Ϸ��Ϣѭ���е�OnUpdate��������Ҫ��������
						dModelRotationYAngle += ((n64tmCurrent - n64tmFrameStart) / 1000.0f) * g_fPalstance;

						//��ת�Ƕ���2PI���ڵı�����ȥ����������ֻ�������0���ȿ�ʼ��С��2PI�Ļ��ȼ���
						if (dModelRotationYAngle > XM_2PI)
						{
							dModelRotationYAngle = fmod(dModelRotationYAngle, XM_2PI);
						}

						//���� World ���� �����Ǹ���ת����
						XMStoreFloat4x4(&g_mxWorld, XMMatrixRotationY(static_cast<float>(dModelRotationYAngle)));
						//���� �Ӿ��� view
						XMStoreFloat4x4(&g_mxView
							, XMMatrixLookAtLH(
								  XMLoadFloat3(&g_f3EyePos)
								, XMLoadFloat3(&g_f3LockAt)
								, XMLoadFloat3(&g_f3HeapUp)));

						// �任��Դ
						for ( int i = 0; i < GRS_NUM_LIGHTS; i++ )
						{
							float fRotLight = g_fPalstance * pow(-1.0f, i);
							XMStoreFloat4(&pstLights->m_stLights[i].m_v4Position
								, XMVector4Transform(XMLoadFloat4(&pstLights->m_stLights[i].m_v4Position)
									, XMMatrixRotationY(fRotLight)));

							XMVECTOR eye = XMLoadFloat4(&pstLights->m_stLights[i].m_v4Position);
							XMVECTOR at = XMVectorSet(0.0f, 8.0f, 0.0f, 0.0f);
							XMStoreFloat4(&pstLights->m_stLights[i].m_v4Direction, XMVector3Normalize(XMVectorSubtract(at, eye)));
							XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

							XMStoreFloat4x4(&pstLights->m_stLights[i].m_mxView
								, XMMatrixLookAtLH(eye, at, up));

							XMStoreFloat4x4(&pstLights->m_stLights[i].m_mxProjection
								, XMMatrixPerspectiveFovLH(XM_PIDIV4, (FLOAT)g_iWndWidth / (FLOAT)g_iWndHeight, 0.1f, 1000.0f));
						}

					}

					GRS_THROW_IF_FAILED(pICmdAllocPre->Reset());
					GRS_THROW_IF_FAILED(pICmdListPre->Reset(pICmdAllocPre.Get(), pIPSOSecondPass.Get()));

					GRS_THROW_IF_FAILED(pICmdAllocPost->Reset());
					GRS_THROW_IF_FAILED(pICmdListPost->Reset(pICmdAllocPost.Get(), pIPSOSecondPass.Get()));

					m_nCurrentFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

					//��Ⱦǰ����
					{
						pICmdListPre->ResourceBarrier(1
							, &CD3DX12_RESOURCE_BARRIER::Transition(
								g_pIARenderTargets[m_nCurrentFrameIndex].Get()
								, D3D12_RESOURCE_STATE_PRESENT
								, D3D12_RESOURCE_STATE_RENDER_TARGET)
						);

						//ƫ��������ָ�뵽ָ��֡������ͼλ��
						CD3DX12_CPU_DESCRIPTOR_HANDLE stRTVHandle(g_pIRTVHeap->GetCPUDescriptorHandleForHeapStart()
							, m_nCurrentFrameIndex, g_nRTVDescriptorSize);
						CD3DX12_CPU_DESCRIPTOR_HANDLE stDSVHandle(g_pIDSVHeap->GetCPUDescriptorHandleForHeapStart()
							,1
							, pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV));
						//������ȾĿ��
						pICmdListPre->OMSetRenderTargets(1, &stRTVHandle, FALSE, &stDSVHandle);

						pICmdListPre->RSSetViewports(1, &g_stViewPort);
						pICmdListPre->RSSetScissorRects(1, &g_stScissorRect);

						pICmdListPre->ClearRenderTargetView(stRTVHandle, faClearColor, 0, nullptr);
						pICmdListPre->ClearDepthStencilView(stDSVHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
					}


					pICmdListPre->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pIDepthShadowBuffer.Get()
						, D3D12_RESOURCE_STATE_DEPTH_WRITE
						, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

				
					nStates = 2;

					arHWaited.RemoveAll();
					arHWaited.Add(g_stThreadParams[g_nThdSphere].m_hEventRenderOver);
					arHWaited.Add(g_stThreadParams[g_nThdCube].m_hEventRenderOver);
					arHWaited.Add(g_stThreadParams[g_nThdPlane].m_hEventRenderOver);

					//֪ͨ���߳̿�ʼ��Ⱦ
					for (int i = 0; i < g_nMaxThread; i++)
					{
						g_stThreadParams[i].m_nCurrentFrameIndex = m_nCurrentFrameIndex;
						g_stThreadParams[i].m_nStartTime = n64tmFrameStart;
						g_stThreadParams[i].m_nCurrentTime = n64tmCurrent;

						SetEvent(g_stThreadParams[i].m_hRunEvent);
					}


				}
				break;
				case 2:// ״̬2 ��ʾ���е���Ⱦ�����б���¼����ˣ���ʼ�����ִ�������б�
				{
					pICmdListPost->ResourceBarrier(1
						, &CD3DX12_RESOURCE_BARRIER::Transition(
							g_pIARenderTargets[m_nCurrentFrameIndex].Get()
							, D3D12_RESOURCE_STATE_RENDER_TARGET
							, D3D12_RESOURCE_STATE_PRESENT));

					pICmdListPost->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pIDepthShadowBuffer.Get()
						, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
						, D3D12_RESOURCE_STATE_DEPTH_WRITE));

					//�ر������б�����ȥִ����
					GRS_THROW_IF_FAILED(pICmdListPre->Close());
					GRS_THROW_IF_FAILED(pICmdListPost->Close());

					arCmdList.RemoveAll();
					//ִ�������б�ע�������б��Ŷ���ϵķ�ʽ��
					arCmdList.Add(pICmdListPre.Get());
					arCmdList.Add(g_stThreadParams[g_nThdSphere].m_pICmdList);
					arCmdList.Add(g_stThreadParams[g_nThdCube].m_pICmdList);
					arCmdList.Add(g_stThreadParams[g_nThdPlane].m_pICmdList);
					arCmdList.Add(pICmdListPost.Get());

					pIMainCmdQueue->ExecuteCommandLists(static_cast<UINT>(arCmdList.GetCount()), arCmdList.GetData());

					//�ύ����
					GRS_THROW_IF_FAILED(pISwapChain3->Present(1, 0));

					//��ʼͬ��GPU��CPU��ִ�У��ȼ�¼Χ�����ֵ
					n64fence = n64FenceValue;
					GRS_THROW_IF_FAILED(pIMainCmdQueue->Signal(pIFence.Get(), n64fence));
					n64FenceValue++;
					GRS_THROW_IF_FAILED(pIFence->SetEventOnCompletion(n64fence, hEventFence));

					nStates = 1;

					arHWaited.RemoveAll();
					arHWaited.Add(hEventFence);

					//������֡��ʼʱ��Ϊ��һ֡��ʼʱ��
					n64tmFrameStart = n64tmCurrent;
				}
				break;
				default:// �����ܵ��������Ϊ�˱�������ı��뾯�����������������default
				{
					bExit = TRUE;
				}
				break;
				}

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
			else
			{


			}

			//---------------------------------------------------------------------------------------------
			//���һ���̵߳Ļ�����������߳��Ѿ��˳��ˣ����˳�ѭ��
			dwRet = WaitForMultipleObjects(static_cast<DWORD>(arHSubThread.GetCount()), arHSubThread.GetData(), FALSE, 0);
			dwRet -= WAIT_OBJECT_0;
			if (dwRet >= 0 && dwRet < g_nMaxThread)
			{
				bExit = TRUE;
			}
		}

		KillTimer(hWnd, WM_USER + 100);

	}
	catch (CGRSCOMException & e)
	{//������COM�쳣
		e;
	}

	try
	{
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
			::CloseHandle(g_stThreadParams[i].m_hThisThread);
			::CloseHandle(g_stThreadParams[i].m_hEventRenderOver);
			g_stThreadParams[i].m_pICmdList->Release();
			g_stThreadParams[i].m_pICmdAlloc->Release();
		}

		//::CoUninitialize();
	}
	catch (CGRSCOMException & e)
	{//������COM�쳣
		e;
	}
	::CoUninitialize();
	return 0;
}

UINT __stdcall RenderThread(void* pParam)
{
	ST_GRS_THREAD_PARAMS* pThdPms = static_cast<ST_GRS_THREAD_PARAMS*>(pParam);
	try
	{
		if ( nullptr == pThdPms )
		{//�����쳣�����쳣��ֹ�߳�
			throw CGRSCOMException(E_INVALIDARG);
		}

		ComPtr<ID3D12Resource>				pITexDiffuse;
		ComPtr<ID3D12Resource>				pITexDiffuseUpload;
		ComPtr<ID3D12Resource>				pITexNormal;
		ComPtr<ID3D12Resource>				pITexNormalUpload;
		ComPtr<ID3D12Resource>				pITexShadow;
		ComPtr<ID3D12Resource>				pITexShadowUpload;
		
		ComPtr<ID3D12Resource>				pIVB;
		ComPtr<ID3D12Resource>				pIIB;

		ComPtr<ID3D12Resource>			    pICBWVP;
		ComPtr<ID3D12DescriptorHeap>		pICBVSRVHeap;

		UINT								nSRVSize 
			= pThdPms->m_pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		ST_GRS_MVP*							pMVPBufModule = nullptr;
		SIZE_T								szMVPBuf = GRS_UPPER(sizeof(ST_GRS_MVP), 256);

		D3D12_VERTEX_BUFFER_VIEW			stVBV = {};
		D3D12_INDEX_BUFFER_VIEW				stIBV = {};
		
		XMMATRIX							mxPosModule = XMMatrixTranslationFromVector(XMLoadFloat4(&pThdPms->m_v4ModelPos));  //��ǰ��Ⱦ�����λ��
		// Mesh Value
		UINT								nIndexCnt = 0;
		UINT								nVertexCnt = 0;

		// DDS Value
		std::unique_ptr<uint8_t[]>			pbDDSData;
		std::vector<D3D12_SUBRESOURCE_DATA> stArSubResources;
		DDS_ALPHA_MODE						emAlphaMode = DDS_ALPHA_MODE_UNKNOWN;
		bool								bIsCube = false;

		//1������DDS����
		{
			//-------------------------------------------------------------------------------------------------------------------------------
			// Diffuse Texture
			GRS_THROW_IF_FAILED(LoadDDSTextureFromFile(pThdPms->m_pID3D12Device4, pThdPms->m_pszDiffuseFile, pITexDiffuse.GetAddressOf()
				, pbDDSData, stArSubResources, SIZE_MAX, &emAlphaMode, &bIsCube));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pITexDiffuse);

			UINT64 n64szUpSphere = GetRequiredIntermediateSize(pITexDiffuse.Get(), 0, static_cast<UINT>(stArSubResources.size()));
			D3D12_RESOURCE_DESC stTXDesc = pITexDiffuse->GetDesc();

			GRS_THROW_IF_FAILED(pThdPms->m_pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
				, D3D12_HEAP_FLAG_NONE
				, &CD3DX12_RESOURCE_DESC::Buffer(n64szUpSphere)
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pITexDiffuseUpload)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pITexDiffuseUpload);

			UpdateSubresources(pThdPms->m_pICmdList
				, pITexDiffuse.Get()
				, pITexDiffuseUpload.Get()
				, 0
				, 0
				, static_cast<UINT>(stArSubResources.size())
				, stArSubResources.data());

			//ͬ��
			pThdPms->m_pICmdList->ResourceBarrier(1
				, &CD3DX12_RESOURCE_BARRIER::Transition(pITexDiffuse.Get()
					, D3D12_RESOURCE_STATE_COPY_DEST
					, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

			//-------------------------------------------------------------------------------------------------------------------------------
			// Normal Texture
			GRS_THROW_IF_FAILED(LoadDDSTextureFromFile(pThdPms->m_pID3D12Device4, pThdPms->m_pszNormalFile, pITexNormal.GetAddressOf()
				, pbDDSData, stArSubResources, SIZE_MAX, &emAlphaMode, &bIsCube));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pITexNormal);

			n64szUpSphere = GetRequiredIntermediateSize(pITexNormal.Get(), 0, static_cast<UINT>(stArSubResources.size()));
			stTXDesc = pITexNormal->GetDesc();

			GRS_THROW_IF_FAILED(pThdPms->m_pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
				, D3D12_HEAP_FLAG_NONE
				, &CD3DX12_RESOURCE_DESC::Buffer(n64szUpSphere)
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pITexNormalUpload)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pITexNormalUpload);

			UpdateSubresources(pThdPms->m_pICmdList
				, pITexNormal.Get()
				, pITexNormalUpload.Get()
				, 0
				, 0
				, static_cast<UINT>(stArSubResources.size())
				, stArSubResources.data());

			//ͬ��
			pThdPms->m_pICmdList->ResourceBarrier(1
				, &CD3DX12_RESOURCE_BARRIER::Transition(pITexNormal.Get()
					, D3D12_RESOURCE_STATE_COPY_DEST
					, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
		}

		//4�������������ȵĳ������� 
		{
			GRS_THROW_IF_FAILED(pThdPms->m_pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
				, D3D12_HEAP_FLAG_NONE
				, &CD3DX12_RESOURCE_DESC::Buffer(szMVPBuf) //ע�⻺��ߴ�����Ϊ256�߽�����С
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pICBWVP)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICBWVP);

			// Map ֮��Ͳ���Unmap�� ֱ�Ӹ������ݽ�ȥ ����ÿ֡������map-copy-unmap�˷�ʱ����
			GRS_THROW_IF_FAILED(pICBWVP->Map(0, nullptr, reinterpret_cast<void**>(&pMVPBufModule)));

		}

		// ����SRV�� CBV SRV
		{
			D3D12_DESCRIPTOR_HEAP_DESC stSRVCBVHPDesc = {};
			stSRVCBVHPDesc.NumDescriptors = 5; // 2 CBV + 3 SRV
			stSRVCBVHPDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			stSRVCBVHPDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			GRS_THROW_IF_FAILED(pThdPms->m_pID3D12Device4->CreateDescriptorHeap(&stSRVCBVHPDesc, IID_PPV_ARGS(&pICBVSRVHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICBVSRVHeap);

			CD3DX12_CPU_DESCRIPTOR_HANDLE stDSHeapHandle(pICBVSRVHeap->GetCPUDescriptorHandleForHeapStart());
			
			// ������һ��CBV ������� �Ӿ��� ͸�Ӿ��� 
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
			cbvDesc.BufferLocation = pICBWVP->GetGPUVirtualAddress();
			cbvDesc.SizeInBytes = static_cast<UINT>(szMVPBuf);
			
			pThdPms->m_pID3D12Device4->CreateConstantBufferView(&cbvDesc, stDSHeapHandle);

			stDSHeapHandle.Offset(1, nSRVSize);

			// �����ڶ���CBV ȫ�ֹ�Դ��Ϣ
			cbvDesc.BufferLocation = pThdPms->m_pICBLights->GetGPUVirtualAddress();
			cbvDesc.SizeInBytes = GRS_UPPER(sizeof(ST_GRS_LIGHTBUFFER), 256);

			pThdPms->m_pID3D12Device4->CreateConstantBufferView(&cbvDesc, stDSHeapHandle);
			
			stDSHeapHandle.Offset(1, nSRVSize);

			//���� Diffuse Texture SRV
			D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
			stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			stSRVDesc.Format = pITexDiffuse->GetDesc().Format;
			stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			stSRVDesc.Texture2D.MipLevels = 1;

			pThdPms->m_pID3D12Device4->CreateShaderResourceView(pITexDiffuse.Get(), &stSRVDesc, stDSHeapHandle);

			stDSHeapHandle.Offset(1, nSRVSize);

			//���� Normal Texture SRV
			stSRVDesc.Format = pITexNormal->GetDesc().Format;
			pThdPms->m_pID3D12Device4->CreateShaderResourceView(pITexNormal.Get(), &stSRVDesc, stDSHeapHandle);

			stDSHeapHandle.Offset(1, nSRVSize);

			//���� Shadow Texture SRV
			stSRVDesc.Format = DXGI_FORMAT_R32_FLOAT;//pThdPms->m_pITexShadowMap->GetDesc().Format;
			pThdPms->m_pID3D12Device4->CreateShaderResourceView(pThdPms->m_pITexShadowMap, &stSRVDesc, stDSHeapHandle);
		}

		//3��������������
		{
			CGRSMeshVertex arVertex;
			CGRSMeshIndex arIndex;
			BOOL bRet = TRUE;
			switch (pThdPms->m_nThreadIndex)
			{
			case g_nThdSphere:
			{
				bRet = LoadSphere(1.0f, 20, 20, arVertex, arIndex);
			}
			break;
			case g_nThdCube:
			{
				bRet = LoadBox(2.0f, 2.0f, 2.0f, 3, arVertex, arIndex);
			}
			break;
			case g_nThdPlane:
			{
				bRet = LoadGrid(10.0f, 10.0f, 60, 40, arVertex, arIndex);
			}
			break;
			default:
				bRet = FALSE;
				break;
			}
			
			if (!bRet)
			{
				throw CGRSCOMException(E_INVALIDARG);
			}

			nVertexCnt = (UINT)arVertex.GetCount();
			nIndexCnt = (UINT)arIndex.GetCount();

			//���� ST_GRS_VERTEX Buffer ��ʹ��Upload��ʽ��
			GRS_THROW_IF_FAILED(pThdPms->m_pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
				, D3D12_HEAP_FLAG_NONE
				, &CD3DX12_RESOURCE_DESC::Buffer(nVertexCnt * sizeof(ST_GRS_VERTEX))
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pIVB)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIVB);

			//ʹ��map-memcpy-unmap�󷨽����ݴ������㻺�����
			UINT8* pVertexDataBegin = nullptr;
			CD3DX12_RANGE stReadRange(0, 0);		// We do not intend to read from this resource on the CPU.

			GRS_THROW_IF_FAILED(pIVB->Map(0, &stReadRange, reinterpret_cast<void**>(&pVertexDataBegin)));
			memcpy(pVertexDataBegin, arVertex.GetData(), nVertexCnt * sizeof(ST_GRS_VERTEX));
			pIVB->Unmap(0, nullptr);

			//���� Index Buffer ��ʹ��Upload��ʽ��
			GRS_THROW_IF_FAILED(pThdPms->m_pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
				, D3D12_HEAP_FLAG_NONE
				, &CD3DX12_RESOURCE_DESC::Buffer(nIndexCnt * sizeof(UINT))
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pIIB)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIIB);

			UINT8* pIndexDataBegin = nullptr;
			GRS_THROW_IF_FAILED(pIIB->Map(0, &stReadRange, reinterpret_cast<void**>(&pIndexDataBegin)));
			memcpy(pIndexDataBegin, arIndex.GetData(), nIndexCnt * sizeof(UINT));
			pIIB->Unmap(0, nullptr);

			//����ST_GRS_VERTEX Buffer View
			stVBV.BufferLocation = pIVB->GetGPUVirtualAddress();
			stVBV.StrideInBytes = sizeof(ST_GRS_VERTEX);
			stVBV.SizeInBytes = nVertexCnt * sizeof(ST_GRS_VERTEX);

			//����Index Buffer View
			stIBV.BufferLocation = pIIB->GetGPUVirtualAddress();
			stIBV.Format = DXGI_FORMAT_R32_UINT;
			stIBV.SizeInBytes = nIndexCnt * sizeof(UINT);
		}



		//5�������¼����� ֪ͨ���л����߳� �����Դ�ĵڶ���Copy����
		{
			GRS_THROW_IF_FAILED(pThdPms->m_pICmdList->Close());
			//��һ��֪ͨ���̱߳��̼߳�����Դ���
			::SetEvent(pThdPms->m_hEventRenderOver); // �����źţ�֪ͨ���̱߳��߳���Դ�������
		}

		// ��������������������Ч�ı���
		float fJmpSpeed = 0.001f; //�����ٶ�
		float fUp = 1.0f;
		float fRawYPos = pThdPms->m_v4ModelPos.y;

		DWORD dwRet = 0;
		BOOL  bQuit = FALSE;
		MSG   msg = {};

		//6����Ⱦѭ��
		while (!bQuit)
		{
			// �ȴ����߳�֪ͨ��ʼ��Ⱦ��ͬʱ���������߳�Post��������Ϣ��Ŀǰ����Ϊ�˵ȴ�WM_QUIT��Ϣ
			dwRet = ::MsgWaitForMultipleObjects(1, &pThdPms->m_hRunEvent, FALSE, INFINITE, QS_ALLPOSTMESSAGE);
			switch (dwRet - WAIT_OBJECT_0)
			{
			case 0:
			{
				//�����������Resetһ�£��ղ��Ѿ�ִ�й���һ���������������
				GRS_THROW_IF_FAILED(pThdPms->m_pICmdAlloc->Reset());
				//Reset�����б�������ָ�������������PSO����
				GRS_THROW_IF_FAILED(pThdPms->m_pICmdList->Reset(pThdPms->m_pICmdAlloc, pThdPms->m_pIPSOSecondPass));

				// OnUpdate()
				{
					//==============================================================================
					//ʹ�����̸߳��µ�ͳһ֡ʱ����������������״̬�ȣ��������ʾ������������������
					//����ʽ������˵�����ö��߳���Ⱦʱ������任�����ڲ�ͬ���߳��У������ڲ�ͬ��CPU�ϲ��е�ִ��
					
					if ( g_nThdSphere == pThdPms->m_nThreadIndex )
					{
						if (pThdPms->m_v4ModelPos.y >= 2.0f * fRawYPos)
						{
							fUp = -1.0f;
							pThdPms->m_v4ModelPos.y = 2.0f * fRawYPos;
						}

						if (pThdPms->m_v4ModelPos.y <= fRawYPos)
						{
							fUp = 1.0f;
							pThdPms->m_v4ModelPos.y = fRawYPos;
						}

						pThdPms->m_v4ModelPos.y += fUp * fJmpSpeed * static_cast<float>(pThdPms->m_nCurrentTime - pThdPms->m_nStartTime);

						mxPosModule = XMMatrixTranslationFromVector(XMLoadFloat4(&pThdPms->m_v4ModelPos));
					}
					//==============================================================================

					// Module * World
					XMMATRIX xmMW = XMMatrixMultiply(mxPosModule, XMLoadFloat4x4(&g_mxWorld));

					// (Module * World) * View * Projection
					xmMW = XMMatrixMultiply(xmMW, XMLoadFloat4x4(&g_mxWorld));

					XMStoreFloat4x4(&pMVPBufModule->g_mxModel, xmMW);

					pMVPBufModule->g_mxView = g_mxView;
					pMVPBufModule->g_mxProjection = g_mxProjection;
				}

				// OnThreadRender()
				{
					//---------------------------------------------------------------------------------------------
//���ö�Ӧ����ȾĿ����Ӳü���(������Ⱦ���̱߳���Ҫ���Ĳ��裬����Ҳ������ν���߳���Ⱦ�ĺ�������������)
					{
						CD3DX12_CPU_DESCRIPTOR_HANDLE stRTVHandle(g_pIRTVHeap->GetCPUDescriptorHandleForHeapStart()
							, pThdPms->m_nCurrentFrameIndex
							, g_nRTVDescriptorSize);
						CD3DX12_CPU_DESCRIPTOR_HANDLE stDSVHandle(g_pIDSVHeap->GetCPUDescriptorHandleForHeapStart()
							, 1
							, pThdPms->m_pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV));
						//������ȾĿ��
						pThdPms->m_pICmdList->OMSetRenderTargets(1, &stRTVHandle, FALSE, &stDSVHandle);
						pThdPms->m_pICmdList->RSSetViewports(1, &g_stViewPort);
						pThdPms->m_pICmdList->RSSetScissorRects(1, &g_stScissorRect);
					}
					//---------------------------------------------------------------------------------------------

					//��Ⱦ��ʵ�ʾ��Ǽ�¼��Ⱦ�����б�
					{
						pThdPms->m_pICmdList->SetGraphicsRootSignature(pThdPms->m_pIRSSecondPass);
						pThdPms->m_pICmdList->SetPipelineState(pThdPms->m_pIPSOSecondPass);
						ID3D12DescriptorHeap* ppHeapsSphere[] = { pICBVSRVHeap.Get(),pThdPms->m_pISampleHeap };
						pThdPms->m_pICmdList->SetDescriptorHeaps(_countof(ppHeapsSphere), ppHeapsSphere);

						CD3DX12_GPU_DESCRIPTOR_HANDLE stGPUSRVHandle(pICBVSRVHeap->GetGPUDescriptorHandleForHeapStart());
						//����CBV
						pThdPms->m_pICmdList->SetGraphicsRootDescriptorTable(0, stGPUSRVHandle);

						stGPUSRVHandle.Offset(2, nSRVSize);
						//����SRV
						pThdPms->m_pICmdList->SetGraphicsRootDescriptorTable(1, stGPUSRVHandle);

						//����Sample
						pThdPms->m_pICmdList->SetGraphicsRootDescriptorTable(2, pThdPms->m_pISampleHeap->GetGPUDescriptorHandleForHeapStart());

						//ע������ʹ�õ���Ⱦ�ַ����������б�Ҳ����ͨ����Mesh����
						pThdPms->m_pICmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
						pThdPms->m_pICmdList->IASetVertexBuffers(0, 1, &stVBV);
						pThdPms->m_pICmdList->IASetIndexBuffer(&stIBV);

						//Draw Call������
						pThdPms->m_pICmdList->DrawIndexedInstanced(nIndexCnt, 1, 0, 0, 0);
					}

					//�����Ⱦ�����ر������б�������ͬ������֪ͨ���߳̿�ʼִ�У�
					{
						GRS_THROW_IF_FAILED(pThdPms->m_pICmdList->Close());
						::SetEvent(pThdPms->m_hEventRenderOver); // �����źţ�֪ͨ���̱߳��߳���Ⱦ���

					}
				}
			}
			break;
			case 1:
			{//������Ϣ
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
		USHORT n16KeyCode = (wParam & 0xFF);
		if (VK_SPACE == n16KeyCode)
		{//���ո���л���ͬ�Ĳ�������Ч����������ÿ�ֲ���������ĺ���
			//UINT g_nCurrentSamplerNO = 0; //��ǰʹ�õĲ���������
			//UINT g_nSampleMaxCnt = 5;		//����������͵Ĳ�����
			//++g_nCurrentSamplerNO;
			//g_nCurrentSamplerNO %= g_nSampleMaxCnt;

			//=================================================================================================
			//������������������
			//pICmdListSphere->Reset(pICmdAllocSphere.Get(), pIPSOSecondPass.Get());
			//pICmdListSphere->SetGraphicsRootSignature(pIRSSecondPass.Get());
			//pICmdListSphere->SetPipelineState(pIPSOSecondPass.Get());
			//ID3D12DescriptorHeap* ppHeapsSphere[] = { pICBVSRVHeap.Get(),pISampleHp.Get() };
			//pICmdListSphere->SetDescriptorHeaps(_countof(ppHeapsSphere), ppHeapsSphere);
			////����SRV
			//pICmdListSphere->SetGraphicsRootDescriptorTable(0, pICBVSRVHeap->GetGPUDescriptorHandleForHeapStart());

			//CD3DX12_GPU_DESCRIPTOR_HANDLE stGPUSRVHandle(pICBVSRVHeap->GetGPUDescriptorHandleForHeapStart()
			//	, 1
			//	, m_pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
			////����CBV
			//pICmdListSphere->SetGraphicsRootDescriptorTable(1, stGPUSRVHandle);
			//CD3DX12_GPU_DESCRIPTOR_HANDLE hGPUSamplerSphere(pISampleHp->GetGPUDescriptorHandleForHeapStart()
			//	, g_nCurrentSamplerNO
			//	, nSamplerDescriptorSize);
			////����Sample
			//pICmdListSphere->SetGraphicsRootDescriptorTable(2, hGPUSamplerSphere);
			////ע������ʹ�õ���Ⱦ�ַ����������б�Ҳ����ͨ����Mesh����
			//pICmdListSphere->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			//pICmdListSphere->IASetVertexBuffers(0, 1, &stVBV);
			//pICmdListSphere->IASetIndexBuffer(&stIBV);

			////Draw Call������
			//pICmdListSphere->DrawIndexedInstanced(nIndexCnt, 1, 0, 0, 0);
			//pICmdListSphere->Close();
			//=================================================================================================
		}
		if (VK_ADD == n16KeyCode || VK_OEM_PLUS == n16KeyCode)
		{
			//double g_fPalstance = 10.0f * XM_PI / 180.0f;	//������ת�Ľ��ٶȣ���λ������/��
			g_fPalstance += 10 * XM_PI / 180.0f;
			if (g_fPalstance > XM_PI)
			{
				g_fPalstance = XM_PI;
			}
			//XMMatrixOrthographicOffCenterLH()
		}

		if (VK_SUBTRACT == n16KeyCode || VK_OEM_MINUS == n16KeyCode)
		{
			g_fPalstance -= 10 * XM_PI / 180.0f;
			if (g_fPalstance < 0.0f)
			{
				g_fPalstance = XM_PI / 180.0f;
			}
		}

		//�����û�����任
		//XMVECTOR g_f3EyePos = XMVectorSet(0.0f, 5.0f, -10.0f, 0.0f); //�۾�λ��
		//XMVECTOR g_f3LockAt = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);  //�۾�������λ��
		//XMVECTOR g_f3HeapUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);  //ͷ�����Ϸ�λ��
		XMFLOAT3 move(0, 0, 0);
		float fMoveSpeed = 2.0f;
		float fTurnSpeed = XM_PIDIV2 * 0.005f;

		if ('w' == n16KeyCode || 'W' == n16KeyCode)
		{
			move.z -= 1.0f;
		}

		if ('s' == n16KeyCode || 'S' == n16KeyCode)
		{
			move.z += 1.0f;
		}

		if ('d' == n16KeyCode || 'D' == n16KeyCode)
		{
			move.x += 1.0f;
		}

		if ('a' == n16KeyCode || 'A' == n16KeyCode)
		{
			move.x -= 1.0f;
		}

		if (fabs(move.x) > 0.1f && fabs(move.z) > 0.1f)
		{
			XMVECTOR vector = XMVector3Normalize(XMLoadFloat3(&move));
			move.x = XMVectorGetX(vector);
			move.z = XMVectorGetZ(vector);
		}

		if (VK_UP == n16KeyCode)
		{
			g_fPitch += fTurnSpeed;
		}

		if (VK_DOWN == n16KeyCode)
		{
			g_fPitch -= fTurnSpeed;
		}

		if (VK_RIGHT == n16KeyCode)
		{
			g_fYaw -= fTurnSpeed;
		}

		if (VK_LEFT == n16KeyCode)
		{
			g_fYaw += fTurnSpeed;
		}

		// Prevent looking too far up or down.
		g_fPitch = min(g_fPitch, XM_PIDIV4);
		g_fPitch = max(-XM_PIDIV4, g_fPitch);

		// Move the camera in model space.
		float x = move.x * -cosf(g_fYaw) - move.z * sinf(g_fYaw);
		float z = move.x * sinf(g_fYaw) - move.z * cosf(g_fYaw);
		g_f3EyePos.x += x * fMoveSpeed;
		g_f3EyePos.z += z * fMoveSpeed;

		// Determine the look direction.
		float r = cosf(g_fPitch);
		g_f3LockAt.x = r * sinf(g_fYaw);
		g_f3LockAt.y = sinf(g_fPitch);
		g_f3LockAt.z = r * cosf(g_fYaw);

		if (VK_TAB == n16KeyCode)
		{//��Tab����ԭ�����λ��
			g_f3EyePos = XMFLOAT3(0.0f, 0.0f, -10.0f); //�۾�λ��
			g_f3LockAt = XMFLOAT3(0.0f, 0.0f, 0.0f);    //�۾�������λ��
			g_f3HeapUp = XMFLOAT3(0.0f, 1.0f, 0.0f);    //ͷ�����Ϸ�λ��
		}

	}

	break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

ST_GRS_VERTEX MidPoint(const ST_GRS_VERTEX& v0, const ST_GRS_VERTEX& v1)
{
	XMVECTOR p0 = XMLoadFloat4(&v0.m_v4Position);
	XMVECTOR p1 = XMLoadFloat4(&v1.m_v4Position);

	XMVECTOR n0 = XMLoadFloat4(&v0.m_v4Normal);
	XMVECTOR n1 = XMLoadFloat4(&v1.m_v4Normal);

	XMVECTOR tan0 = XMLoadFloat4(&v0.m_v4Tangent);
	XMVECTOR tan1 = XMLoadFloat4(&v1.m_v4Tangent);

	XMVECTOR tex0 = XMLoadFloat2(&v0.m_v2Texcoord);
	XMVECTOR tex1 = XMLoadFloat2(&v1.m_v2Texcoord);

	// Compute the midpoints of all the attributes.  Vectors need to be normalized
	// since linear interpolating can make them not unit length.  
	XMVECTOR pos = 0.5f * (p0 + p1);
	XMVECTOR normal = XMVector4Normalize(0.5f * (n0 + n1));
	XMVECTOR tangent = XMVector4Normalize(0.5f * (tan0 + tan1));
	XMVECTOR tex = 0.5f * (tex0 + tex1);

	ST_GRS_VERTEX stMidPoint;
	XMStoreFloat4(&stMidPoint.m_v4Position, pos);
	XMStoreFloat4(&stMidPoint.m_v4Normal, normal);
	XMStoreFloat4(&stMidPoint.m_v4Tangent, tangent);
	XMStoreFloat2(&stMidPoint.m_v2Texcoord, tex);

	return stMidPoint;
}

void Subdivide(CGRSMeshVertex& arVertex, CGRSMeshIndex& arIndices)
{
	// Save a copy of the input geometry.
	CGRSMeshVertex tmpV;
	CGRSMeshIndex tmpI;

	tmpV.Append(arVertex);
	tmpI.Append(arIndices);

	arVertex.RemoveAll();
	arIndices.RemoveAll();

	//       v1
	//       *
	//      / \
	//     /   \
	//  m0*-----*m1
	//   / \   / \
	//  /   \ /   \
	// *-----*-----*
	// v0    m2     v2

	UINT32 numTris = (UINT32)tmpI.GetCount() / 3;
	for (UINT32 i = 0; i < numTris; ++i)
	{
		ST_GRS_VERTEX v0 = tmpV[tmpI[i * 3 + 0]];
		ST_GRS_VERTEX v1 = tmpV[tmpI[i * 3 + 1]];
		ST_GRS_VERTEX v2 = tmpV[tmpI[i * 3 + 2]];

		//
		// Generate the midpoints.
		//

		ST_GRS_VERTEX m0 = MidPoint(v0, v1);
		ST_GRS_VERTEX m1 = MidPoint(v1, v2);
		ST_GRS_VERTEX m2 = MidPoint(v0, v2);

		//
		// Add new geometry.
		//

		arVertex.Add(v0); // 0
		arVertex.Add(v1); // 1
		arVertex.Add(v2); // 2
		arVertex.Add(m0); // 3
		arVertex.Add(m1); // 4
		arVertex.Add(m2); // 5

		arIndices.Add(i * 6 + 0);
		arIndices.Add(i * 6 + 3);
		arIndices.Add(i * 6 + 5);

		arIndices.Add(i * 6 + 3);
		arIndices.Add(i * 6 + 4);
		arIndices.Add(i * 6 + 5);

		arIndices.Add(i * 6 + 5);
		arIndices.Add(i * 6 + 4);
		arIndices.Add(i * 6 + 2);

		arIndices.Add(i * 6 + 3);
		arIndices.Add(i * 6 + 1);
		arIndices.Add(i * 6 + 4);
	}
}

BOOL LoadBox(FLOAT width
	, FLOAT height
	, FLOAT depth
	, UINT32 numSubdivisions
	, CGRSMeshVertex& arVertex
	, CGRSMeshIndex& arIndices)
{
	float w2 = 0.5f * width;
	float h2 = 0.5f * height;
	float d2 = 0.5f * depth;

	arVertex.SetCount(24);
	// Fill in the front face vertex data.
	arVertex[0] = ST_GRS_VERTEX(-w2, -h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	arVertex[1] = ST_GRS_VERTEX(-w2, +h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	arVertex[2] = ST_GRS_VERTEX(+w2, +h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
	arVertex[3] = ST_GRS_VERTEX(+w2, -h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

	// Fill in the back face vertex data.
	arVertex[4] = ST_GRS_VERTEX(-w2, -h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
	arVertex[5] = ST_GRS_VERTEX(+w2, -h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	arVertex[6] = ST_GRS_VERTEX(+w2, +h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	arVertex[7] = ST_GRS_VERTEX(-w2, +h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

	// Fill in the top face vertex data.
	arVertex[8] = ST_GRS_VERTEX(-w2, +h2, -d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	arVertex[9] = ST_GRS_VERTEX(-w2, +h2, +d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	arVertex[10] = ST_GRS_VERTEX(+w2, +h2, +d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
	arVertex[11] = ST_GRS_VERTEX(+w2, +h2, -d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

	// Fill in the bottom face vertex data.
	arVertex[12] = ST_GRS_VERTEX(-w2, -h2, -d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
	arVertex[13] = ST_GRS_VERTEX(+w2, -h2, -d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	arVertex[14] = ST_GRS_VERTEX(+w2, -h2, +d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	arVertex[15] = ST_GRS_VERTEX(-w2, -h2, +d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

	// Fill in the left face vertex data.
	arVertex[16] = ST_GRS_VERTEX(-w2, -h2, +d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f);
	arVertex[17] = ST_GRS_VERTEX(-w2, +h2, +d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f);
	arVertex[18] = ST_GRS_VERTEX(-w2, +h2, -d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f);
	arVertex[19] = ST_GRS_VERTEX(-w2, -h2, -d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f);

	// Fill in the right face vertex data.
	arVertex[20] = ST_GRS_VERTEX(+w2, -h2, -d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
	arVertex[21] = ST_GRS_VERTEX(+w2, +h2, -d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
	arVertex[22] = ST_GRS_VERTEX(+w2, +h2, +d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
	arVertex[23] = ST_GRS_VERTEX(+w2, -h2, +d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);

	//
	// Create the indices.
	//
	arIndices.SetCount(36);

	// Fill in the front face index data
	arIndices[0] = 0; arIndices[1] = 1; arIndices[2] = 2;
	arIndices[3] = 0; arIndices[4] = 2; arIndices[5] = 3;

	// Fill in the back face index data
	arIndices[6] = 4; arIndices[7] = 5; arIndices[8] = 6;
	arIndices[9] = 4; arIndices[10] = 6; arIndices[11] = 7;

	// Fill in the top face index data
	arIndices[12] = 8; arIndices[13] = 9; arIndices[14] = 10;
	arIndices[15] = 8; arIndices[16] = 10; arIndices[17] = 11;

	// Fill in the bottom face index data
	arIndices[18] = 12; arIndices[19] = 13; arIndices[20] = 14;
	arIndices[21] = 12; arIndices[22] = 14; arIndices[23] = 15;

	// Fill in the left face index data
	arIndices[24] = 16; arIndices[25] = 17; arIndices[26] = 18;
	arIndices[27] = 16; arIndices[28] = 18; arIndices[29] = 19;

	// Fill in the right face index data
	arIndices[30] = 20; arIndices[31] = 21; arIndices[32] = 22;
	arIndices[33] = 20; arIndices[34] = 22; arIndices[35] = 23;

	// Put a cap on the number of subdivisions.
	numSubdivisions = numSubdivisions < 6u ? numSubdivisions : 6u ;

	for (UINT32 i = 0; i < numSubdivisions; ++i)
	{
		Subdivide(arVertex, arIndices);
	}	
	return arVertex.GetCount() > 0 && arIndices.GetCount() > 0;
}
BOOL LoadSphere(FLOAT radius
	, UINT32 sliceCount
	, UINT32 stackCount
	, CGRSMeshVertex& arVertex
	, CGRSMeshIndex& arIndices)
{
	ST_GRS_VERTEX topVertex(0.0f, +radius, 0.0f, 0.0f, +1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	ST_GRS_VERTEX bottomVertex(0.0f, -radius, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);

	arVertex.Add(topVertex);

	float phiStep = XM_PI / stackCount;
	float thetaStep = 2.0f * XM_PI / sliceCount;
	float phi = 0.0f;
	float theta = 0.0f;
	ST_GRS_VERTEX v;

	// Compute vertices for each stack ring (do not count the poles as rings).
	for (UINT32 i = 1; i <= stackCount - 1; ++i)
	{
		phi = i * phiStep;
		// Vertices of ring.
		for (UINT32 j = 0; j <= sliceCount; ++j)
		{
			theta = j * thetaStep;
			
			// spherical to cartesian
			v.m_v4Position.x = radius * sinf(phi) * cosf(theta);
			v.m_v4Position.y = radius * cosf(phi);
			v.m_v4Position.z = radius * sinf(phi) * sinf(theta);
			v.m_v4Position.w = 1.0f;

			// Partial derivative of P with respect to theta
			v.m_v4Tangent.x = -radius * sinf(phi) * sinf(theta);
			v.m_v4Tangent.y = 0.0f;
			v.m_v4Tangent.z = +radius * sinf(phi) * cosf(theta);
			v.m_v4Tangent.w = 0.0f;

			XMVECTOR T = XMLoadFloat4(&v.m_v4Tangent);
			XMStoreFloat4(&v.m_v4Tangent, XMVector4Normalize(T));

			XMVECTOR p = XMLoadFloat4(&v.m_v4Position);
			XMStoreFloat4(&v.m_v4Normal, XMVector4Normalize(p));

			v.m_v2Texcoord.x = theta / XM_2PI;
			v.m_v2Texcoord.y = phi / XM_PI;

			arVertex.Add(v);
		}
	}

	arVertex.Add(bottomVertex);

	//
	// Compute indices for top stack.  The top stack was written first to the vertex buffer
	// and connects the top pole to the first ring.
	//

	for (UINT32 i = 1; i <= sliceCount; ++i)
	{
		arIndices.Add(0);
		arIndices.Add(i + 1);
		arIndices.Add(i);
	}

	//
	// Compute indices for inner stacks (not connected to poles).
	//

	// Offset the indices to the index of the first vertex in the first ring.
	// This is just skipping the top pole vertex.
	UINT32 baseIndex = 1;
	UINT32 ringVertexCount = sliceCount + 1;
	for (UINT32 i = 0; i < stackCount - 2; ++i)
	{
		for (UINT32 j = 0; j < sliceCount; ++j)
		{
			arIndices.Add(baseIndex + i * ringVertexCount + j);
			arIndices.Add(baseIndex + i * ringVertexCount + j + 1);
			arIndices.Add(baseIndex + (i + 1) * ringVertexCount + j);

			arIndices.Add(baseIndex + (i + 1) * ringVertexCount + j);
			arIndices.Add(baseIndex + i * ringVertexCount + j + 1);
			arIndices.Add(baseIndex + (i + 1) * ringVertexCount + j + 1);
		}
	}

	//
	// Compute indices for bottom stack.  The bottom stack was written last to the vertex buffer
	// and connects the bottom pole to the bottom ring.
	//

	// South pole vertex was added last.
	UINT32 southPoleIndex = (UINT32)arVertex.GetCount() - 1;

	// Offset the indices to the index of the first vertex in the last ring.
	baseIndex = southPoleIndex - ringVertexCount;

	for (UINT32 i = 0; i < sliceCount; ++i)
	{
		arIndices.Add(southPoleIndex);
		arIndices.Add(baseIndex + i);
		arIndices.Add(baseIndex + i + 1);
	}

	return arVertex.GetCount() > 0 && arIndices.GetCount() > 0;
}

BOOL LoadGrid(float width
	, float depth
	, UINT32 m
	, UINT32 n
	, CGRSMeshVertex& arVertex
	, CGRSMeshIndex& arIndices)
{
	UINT32 vertexCount = m * n;
	UINT32 faceCount = (m - 1) * (n - 1) * 2;

	//
	// Create the vertices.
	//

	float halfWidth = 0.5f * width;
	float halfDepth = 0.5f * depth;

	float dx = width / (n - 1);
	float dz = depth / (m - 1);

	float du = 1.0f / (n - 1);
	float dv = 1.0f / (m - 1);

	arVertex.SetCount(vertexCount);

	for (UINT32 i = 0; i < m; ++i)
	{
		float z = halfDepth - i * dz;
		for (UINT32 j = 0; j < n; ++j)
		{
			float x = -halfWidth + j * dx;
			
			arVertex[i * n + j].m_v4Position = XMFLOAT4(x, 0.0f, z,1.0f);
			arVertex[i * n + j].m_v4Normal = XMFLOAT4(0.0f, 1.0f, 0.0f,0.0f);
			arVertex[i * n + j].m_v4Tangent = XMFLOAT4(1.0f, 0.0f, 0.0f,0.0f);

			// Stretch texture over grid.
			arVertex[i * n + j].m_v2Texcoord.x = j * du;
			arVertex[i * n + j].m_v2Texcoord.y = i * dv;
		}
	}

	//
	// Create the indices.
	//
	arIndices.SetCount(faceCount * 3);
	// Iterate over each quad and compute indices.
	UINT32 k = 0;
	for (UINT32 i = 0; i < m - 1; ++i)
	{
		for (UINT32 j = 0; j < n - 1; ++j)
		{
			arIndices[k] = i * n + j;
			arIndices[k + 1] = i * n + j + 1;
			arIndices[k + 2] = (i + 1) * n + j;

			arIndices[k + 3] = (i + 1) * n + j;
			arIndices[k + 4] = i * n + j + 1;
			arIndices[k + 5] = (i + 1) * n + j + 1;

			k += 6; // next quad
		}
	}

	return arVertex.GetCount() > 0 && arIndices.GetCount() > 0;
}

