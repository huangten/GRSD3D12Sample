#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // �� Windows ͷ���ų�����ʹ�õ�����
#include <windows.h>
#include <tchar.h>
#include <fstream>  //for ifstream
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
#define GRS_WND_TITLE	_T("GRS DirectX12 MultiThread & MultiAdapter Sample")

#define GRS_THROW_IF_FAILED(hr) { HRESULT _hr = (hr);if (FAILED(_hr)){ throw CGRSCOMException(_hr); } }
#define GRS_SAFE_RELEASE(p) if(nullptr != (p)){(p)->Release();(p)=nullptr;}

//�¶���ĺ�������ȡ������
#define GRS_UPPER_DIV(A,B) ((UINT)(((A)+((B)-1))/(B)))
//���������ϱ߽�����㷨 �ڴ�����г��� ���ס
#define GRS_UPPER(A,B) ((UINT)(((A)+((B)-1))&~(B - 1)))

// �ڴ����ĺ궨��
#define GRS_ALLOC(sz)		::HeapAlloc(GetProcessHeap(),0,(sz))
#define GRS_CALLOC(sz)		::HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(sz))
#define GRS_CREALLOC(p,sz)	::HeapReAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(p),(sz))
#define GRS_SAFE_FREE(p)	if( nullptr != (p) ){ ::HeapFree( ::GetProcessHeap(),0,(p) ); (p) = nullptr; }

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

#endif

// Ĭ�Ϻ󻺳�����
const UINT								g_nFrameBackBufCount = 3u;

// �Կ���������
struct ST_GRS_GPU_PARAMS
{
	UINT								m_nGPUIndex;

	UINT								m_nRTVDescriptorSize;
	UINT								m_nSRVDescriptorSize;

	ComPtr<ID3D12Device4>				m_pID3D12Device4;
	ComPtr<ID3D12CommandQueue>			m_pICMDQueue;

	ComPtr<ID3D12Fence>					m_pIFence;
	ComPtr<ID3D12Fence>					m_pISharedFence;

	HANDLE								m_hEventFence;

	ComPtr<ID3D12Resource>				m_pIRTRes[g_nFrameBackBufCount];
	ComPtr<ID3D12DescriptorHeap>		m_pIRTVHeap;
	ComPtr<ID3D12Resource>				m_pIDSRes;
	ComPtr<ID3D12DescriptorHeap>		m_pIDSVHeap;

	ComPtr<ID3D12DescriptorHeap>		m_pISRVHeap;
	ComPtr<ID3D12DescriptorHeap>		m_pISampleHeap;

	ComPtr<ID3D12Heap>					m_pICrossAdapterHeap;
	ComPtr<ID3D12Resource>				m_pICrossAdapterResPerFrame[g_nFrameBackBufCount];

	ComPtr<ID3D12CommandAllocator>		m_pICMDAlloc;
	ComPtr<ID3D12GraphicsCommandList>	m_pICMDList;

	ComPtr<ID3D12RootSignature>			m_pIRS;
	ComPtr<ID3D12PipelineState>			m_pIPSO;

};

// ��Ⱦ���̲߳���
struct ST_GRS_THREAD_PARAMS
{
	UINT								m_nThreadIndex;				//���
	DWORD								m_dwThisThreadID;
	HANDLE								m_hThisThread;
	DWORD								m_dwMainThreadID;
	HANDLE								m_hMainThread;

	HANDLE								m_hEventRun;
	HANDLE								m_hEventRenderOver;

	UINT								m_nCurrentFrameIndex;//��ǰ��Ⱦ�󻺳����
	ULONGLONG							m_nStartTime;		 //��ǰ֡��ʼʱ��
	ULONGLONG							m_nCurrentTime;	     //��ǰʱ��

	XMFLOAT4							m_v4ModelPos;
	TCHAR								m_pszDDSFile[MAX_PATH];
	CHAR								m_pszMeshFile[MAX_PATH];

	ID3D12Device4* m_pID3D12Device4;

	ID3D12DescriptorHeap* m_pIRTVHeap;
	ID3D12DescriptorHeap* m_pIDSVHeap;

	ID3D12CommandAllocator* m_pICMDAlloc;
	ID3D12GraphicsCommandList* m_pICMDList;

	ID3D12RootSignature* m_pIRS;
	ID3D12PipelineState* m_pIPSO;
};

// ����ṹ
struct ST_GRS_VERTEX
{
	XMFLOAT4 m_v4Position;		//Position
	XMFLOAT2 m_vTex;		//Texcoord
	XMFLOAT3 m_vNor;		//Normal
};

//���ں�����Ⱦ�ľ��ζ���ṹ
struct ST_GRS_VERTEX_QUAD
{
	XMFLOAT4 m_v4Position;		//Position
	XMFLOAT2 m_vTex;		//Texcoord
};

// ����������
struct ST_GRS_MVP
{
	XMFLOAT4X4 m_MVP;			//�����Model-view-projection(MVP)����.
};

struct ST_GRS_PEROBJECT_CB
{
	UINT		m_nFun;
	float		m_fQuatLevel;   //����bit����ȡֵ2-6
	float		m_fWaterPower;  //��ʾˮ����չ���ȣ���λΪ����
};

int g_iWndWidth = 1024;
int g_iWndHeight = 768;

D3D12_VIEWPORT					g_stViewPort = { 0.0f, 0.0f, static_cast<float>(g_iWndWidth), static_cast<float>(g_iWndHeight) , D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
D3D12_RECT							g_stScissorRect = { 0, 0, static_cast<LONG>(g_iWndWidth), static_cast<LONG>(g_iWndHeight) };

//��ʼ��Ĭ���������λ��
XMFLOAT3								g_f3EyePos = XMFLOAT3(0.0f, 5.0f, -10.0f);  //�۾�λ��
XMFLOAT3								g_f3LockAt = XMFLOAT3(0.0f, 0.0f, 0.0f);    //�۾�������λ��
XMFLOAT3								g_f3HeapUp = XMFLOAT3(0.0f, 1.0f, 0.0f);    //ͷ�����Ϸ�λ��

float											g_fYaw = 0.0f;			// ����Z�����ת��.
float											g_fPitch = 0.0f;			// ��XZƽ�����ת��

double										g_fPalstance = 5.0f * XM_PI / 180.0f;	//������ת�Ľ��ٶȣ���λ������/��

XMFLOAT4X4							g_mxWorld = {}; //World Matrix
XMFLOAT4X4							g_mxVP = {};    //View Projection Matrix

// ȫ���̲߳���
const UINT								g_nMaxThread = 3;
const UINT								g_nThdSphere = 0;
const UINT								g_nThdCube = 1;
const UINT								g_nThdPlane = 2;
ST_GRS_THREAD_PARAMS		g_stThread[g_nMaxThread] = {};

TCHAR										g_pszAppPath[MAX_PATH] = {};

//������Ҫ�Ĳ���������Ϊȫ�ֱ��������ڴ��ڹ����и��ݰ����������
UINT										g_nFunNO = 1;		//��ǰʹ��Ч����������ţ����ո��ѭ���л���
UINT										g_nMaxFunNO = 2;    //�ܵ�Ч����������
float											g_fQuatLevel = 2.0f;    //����bit����ȡֵ2-6
float											g_fWaterPower = 40.0f;  //��ʾˮ����չ���ȣ���λΪ����

//ʹ���ĸ��汾PS������ģ������0 ԭ����� 1 ������ʽ��˹ģ��PS 2 ˫������Ż���ĸ�˹ģ��PS������΢��ٷ�ʾ����
UINT										g_nUsePSID = 1;

//��Ⱦ���̺߳���
UINT __stdcall RenderThread(void* pParam);
//�����ڹ���
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
//��������ĺ���
BOOL LoadMeshVertex(const CHAR* pszMeshFileName, UINT& nVertexCnt, ST_GRS_VERTEX*& ppVertex, UINT*& ppIndices);

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR    lpCmdLine, int nCmdShow)
{
	HWND								hWnd = nullptr;
	MSG									msg = {};


	const float							faClearColor[] = { 0.2f, 0.5f, 1.0f, 1.0f };

	ComPtr<IDXGIFactory5>				pIDXGIFactory5;
	ComPtr<IDXGIFactory6>				pIDXGIFactory6;
	ComPtr<IDXGIAdapter1>				pIAdapter1;
	ComPtr<IDXGISwapChain1>				pISwapChain1;
	ComPtr<IDXGISwapChain3>				pISwapChain3;

	const UINT							c_nMaxGPUCnt = 2;			//֧������GPU������Ⱦ
	const UINT							c_nMainGPU = 0;
	const UINT							c_nSecondGPU = 1;
	ST_GRS_GPU_PARAMS					stGPU[c_nMaxGPUCnt] = {};

	// ���ڸ�����Ⱦ����ĸ���������С�����������������б����
	ComPtr<ID3D12CommandQueue>			pICmdQueueCopy;
	ComPtr<ID3D12CommandAllocator>		pICMDAllocCopy;
	ComPtr<ID3D12GraphicsCommandList>	pICMDListCopy;

	//������ֱ�ӹ�����Դʱ�����ڸ����Կ��ϴ�����������Դ�����ڸ������Կ���Ⱦ�������
	ComPtr<ID3D12Resource>				pISecondaryAdapterTexutrePerFrame[g_nFrameBackBufCount];
	BOOL								bCrossAdapterTextureSupport = FALSE;
	D3D12_RESOURCE_DESC					stRenderTargetDesc = {};
	const float							v4ClearColor[4] = { 0.2f, 0.5f, 1.0f, 1.0f };

	UINT								nCurrentFrameIndex = 0;

	DXGI_FORMAT							emRTFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT							emDSFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	UINT64								n64FenceValue = 1ui64;
	UINT64								n64CurrentFenceValue = 0;

	ComPtr<ID3D12CommandAllocator>		pICMDAllocBefore;
	ComPtr<ID3D12GraphicsCommandList>	pICMDListBefore;

	ComPtr<ID3D12CommandAllocator>		pICMDAllocLater;
	ComPtr<ID3D12GraphicsCommandList>	pICMDListLater;

	CAtlArray<HANDLE>					arHWaited;
	CAtlArray<HANDLE>					arHSubThread;

	//�����Կ��Ϻ�����Ҫ����Դ������Second Pass��
	const UINT							c_nPostPassCnt = 2;										//��������
	const UINT							c_nPostPass0 = 0;
	const UINT							c_nPostPass1 = 1;
	ComPtr<ID3D12Resource>				pIOffLineRTRes[c_nPostPassCnt][g_nFrameBackBufCount];	//������Ⱦ����ȾĿ����Դ
	ComPtr<ID3D12DescriptorHeap>		pIRTVOffLine[c_nPostPassCnt];							//������ȾRTV

	// Second Pass Values
	ComPtr<ID3D12Resource>				pIVBQuad;
	ComPtr<ID3D12Resource>				pIVBQuadUpload;
	D3D12_VERTEX_BUFFER_VIEW			pstVBVQuad;
	SIZE_T								szSecondPassCB = GRS_UPPER(sizeof(ST_GRS_PEROBJECT_CB), 256);
	ST_GRS_PEROBJECT_CB* pstCBSecondPass = nullptr;
	ComPtr<ID3D12Resource>				pICBResSecondPass;
	ComPtr<ID3D12Resource>				pINoiseTexture;
	ComPtr<ID3D12Resource>				pINoiseTextureUpload;

	// Third Pass Values
	ComPtr<ID3D12CommandAllocator>		pICMDAllocPostPass;
	ComPtr<ID3D12GraphicsCommandList>	pICMDListPostPass;
	ComPtr<ID3D12RootSignature>			pIRSPostPass;
	ComPtr<ID3D12PipelineState>			pIPSOPostPass[c_nPostPassCnt];
	ComPtr<ID3D12DescriptorHeap>		pISRVHeapPostPass[c_nPostPassCnt];
	ComPtr<ID3D12DescriptorHeap>		pISampleHeapPostPass;

	D3D12_HEAP_PROPERTIES stDefautHeapProps = {};
	stDefautHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
	stDefautHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	stDefautHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	stDefautHeapProps.CreationNodeMask = 0;
	stDefautHeapProps.VisibleNodeMask = 0;

	D3D12_HEAP_PROPERTIES stUploadHeapProps = {};
	stUploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
	stUploadHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	stUploadHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	stUploadHeapProps.CreationNodeMask = 0;
	stUploadHeapProps.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC stBufferResSesc = {};
	stBufferResSesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	stBufferResSesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	stBufferResSesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	stBufferResSesc.Format = DXGI_FORMAT_UNKNOWN;
	stBufferResSesc.Width = 0;
	stBufferResSesc.Height = 1;
	stBufferResSesc.DepthOrArraySize = 1;
	stBufferResSesc.MipLevels = 1;
	stBufferResSesc.SampleDesc.Count = 1;
	stBufferResSesc.SampleDesc.Quality = 0;

	D3D12_RESOURCE_BARRIER stResStateTransBarrier = {};
	stResStateTransBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	stResStateTransBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	stResStateTransBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	try
	{
		GRS_THROW_IF_FAILED(::CoInitialize(nullptr));

		// 0���õ���ǰ�Ĺ���Ŀ¼����������ʹ�����·�������ʸ�����Դ�ļ�
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

		// 1����������
		{
			//---------------------------------------------------------------------------------------------
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

		// 2������DXGI Factory����
		{
			UINT nDXGIFactoryFlags = 0U;
#if defined(_DEBUG)
			// ����ʾ��ϵͳ�ĵ���֧��
			ComPtr<ID3D12Debug> debugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();
				// �򿪸��ӵĵ���֧��
				nDXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			}
#endif
			GRS_THROW_IF_FAILED(CreateDXGIFactory2(nDXGIFactoryFlags, IID_PPV_ARGS(&pIDXGIFactory5)));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pIDXGIFactory5);
			//��ȡIDXGIFactory6�ӿ�
			GRS_THROW_IF_FAILED(pIDXGIFactory5.As(&pIDXGIFactory6));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pIDXGIFactory6);
		}

		// 3���������ܴӸߵ���ö�������������豸
		{
			DXGI_ADAPTER_DESC1 stAdapterDesc[c_nMaxGPUCnt] = {};
			D3D_FEATURE_LEVEL emFeatureLevel = D3D_FEATURE_LEVEL_12_1;
			D3D12_COMMAND_QUEUE_DESC stQueueDesc = {};
			stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			HRESULT hr = S_OK;
			UINT i = 0;

			for (i = 0;
				(i < c_nMaxGPUCnt) &&
				SUCCEEDED(pIDXGIFactory6->EnumAdapterByGpuPreference(
					i
					, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
					, IID_PPV_ARGS(&pIAdapter1)));
				++i)
			{
				GRS_THROW_IF_FAILED(pIAdapter1->GetDesc1(&stAdapterDesc[i]));

				if (stAdapterDesc[i].Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				{//������������������豸��
				 //ע�����if�жϣ�����ʹ��һ����ʵGPU��һ������������������˫GPU��ʾ��
					continue;
				}

				HRESULT hr = D3D12CreateDevice(pIAdapter1.Get()
					, emFeatureLevel
					, IID_PPV_ARGS(stGPU[i].m_pID3D12Device4.GetAddressOf()));

				if (!SUCCEEDED(hr))
				{
					::MessageBox(hWnd
						, _T("�ǳ���Ǹ��֪ͨ����\r\n��ϵͳ����NB���Կ�Ҳ����֧��DX12������û���������У�\r\n�����˳���")
						, GRS_WND_TITLE
						, MB_OK | MB_ICONINFORMATION);

					return -1;
				}

				GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPU[i].m_pID3D12Device4);

				GRS_THROW_IF_FAILED(stGPU[i].m_pID3D12Device4->CreateCommandQueue(
					&stQueueDesc
					, IID_PPV_ARGS(&stGPU[i].m_pICMDQueue)));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPU[i].m_pICMDQueue);

				stGPU[i].m_nGPUIndex = i;
				stGPU[i].m_nRTVDescriptorSize
					= stGPU[i].m_pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
				stGPU[i].m_nSRVDescriptorSize
					= stGPU[i].m_pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				pIAdapter1.Reset();
			}

			if (nullptr == stGPU[c_nMainGPU].m_pID3D12Device4.Get()
				|| nullptr == stGPU[c_nSecondGPU].m_pID3D12Device4.Get())
			{// ������û�����������Կ� �˳����� 
				::MessageBox(hWnd
					, _T("�ǳ���Ǹ��֪ͨ����\r\n�������Կ���������������ʾ�������˳���")
					, GRS_WND_TITLE
					, MB_OK | MB_ICONINFORMATION);
				throw CGRSCOMException(E_FAIL);
			}

			//����GPU�ϴ�������������У���ν���߶���
			stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
			GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateCommandQueue(
				&stQueueDesc
				, IID_PPV_ARGS(&pICmdQueueCopy)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICmdQueueCopy);

			//����ʹ�õ��豸������ʾ�����ڱ�����
			TCHAR pszWndTitle[MAX_PATH] = {};
			::GetWindowText(hWnd, pszWndTitle, MAX_PATH);
			StringCchPrintf(pszWndTitle
				, MAX_PATH
				, _T("%s(GPU[0]:%s & GPU[1]:%s)")
				, pszWndTitle
				, stAdapterDesc[c_nMainGPU].Description
				, stAdapterDesc[c_nSecondGPU].Description);
			::SetWindowText(hWnd, pszWndTitle);
		}

		// 4�����������б�
		{
			WCHAR pszDebugName[MAX_PATH] = {};

			for (int i = 0; i < c_nMaxGPUCnt; i++)
			{
				GRS_THROW_IF_FAILED(stGPU[i].m_pID3D12Device4->CreateCommandAllocator(
					D3D12_COMMAND_LIST_TYPE_DIRECT
					, IID_PPV_ARGS(&stGPU[i].m_pICMDAlloc)));

				if (SUCCEEDED(StringCchPrintfW(pszDebugName, MAX_PATH, L"stGPU[%u].m_pICMDAlloc", i)))
				{
					stGPU[i].m_pICMDAlloc->SetName(pszDebugName);
				}

				GRS_THROW_IF_FAILED(stGPU[i].m_pID3D12Device4->CreateCommandList(
					0, D3D12_COMMAND_LIST_TYPE_DIRECT
					, stGPU[i].m_pICMDAlloc.Get(), nullptr, IID_PPV_ARGS(&stGPU[i].m_pICMDList)));
				if (SUCCEEDED(StringCchPrintfW(pszDebugName, MAX_PATH, L"stGPU[%u].m_pICMDList", i)))
				{
					stGPU[i].m_pICMDList->SetName(pszDebugName);
				}
			}

			// ���Կ��ϵ������б�
			//ǰ���������б�
			GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT
				, IID_PPV_ARGS(&pICMDAllocBefore)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICMDAllocBefore);

			GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateCommandList(
				0, D3D12_COMMAND_LIST_TYPE_DIRECT
				, pICMDAllocBefore.Get(), nullptr, IID_PPV_ARGS(&pICMDListBefore)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICMDListBefore);

			//���������б�
			GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT
				, IID_PPV_ARGS(&pICMDAllocLater)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICMDAllocLater);
			GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateCommandList(
				0, D3D12_COMMAND_LIST_TYPE_DIRECT
				, pICMDAllocLater.Get(), nullptr, IID_PPV_ARGS(&pICMDListLater)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICMDListLater);

			//���������б�
			GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_COPY
				, IID_PPV_ARGS(&pICMDAllocCopy)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICMDAllocCopy);
			GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateCommandList(
				0, D3D12_COMMAND_LIST_TYPE_COPY
				, pICMDAllocCopy.Get(), nullptr, IID_PPV_ARGS(&pICMDListCopy)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICMDListCopy);

			// �����Կ��ϵ������б�
			// Third Pass �����б������Կ������������ڸ����Կ��ϣ�
			GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pID3D12Device4->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT
				, IID_PPV_ARGS(&pICMDAllocPostPass)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICMDAllocPostPass);
			GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pID3D12Device4->CreateCommandList(
				0, D3D12_COMMAND_LIST_TYPE_DIRECT
				, pICMDAllocPostPass.Get(), nullptr, IID_PPV_ARGS(&pICMDListPostPass)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICMDListPostPass);
		}

		// 5������Χ�������Լ���GPUͬ��Χ������
		{
			for (int i = 0; i < c_nMaxGPUCnt; i++)
			{
				GRS_THROW_IF_FAILED(
					stGPU[i].m_pID3D12Device4->CreateFence(
						0
						, D3D12_FENCE_FLAG_NONE
						, IID_PPV_ARGS(&stGPU[i].m_pIFence)));

				stGPU[i].m_hEventFence = CreateEvent(nullptr, FALSE, FALSE, nullptr);
				if (stGPU[i].m_hEventFence == nullptr)
				{
					GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
				}
			}

			// �����Կ��ϴ���һ���ɹ����Χ������
			GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateFence(0
				, D3D12_FENCE_FLAG_SHARED | D3D12_FENCE_FLAG_SHARED_CROSS_ADAPTER
				, IID_PPV_ARGS(&stGPU[c_nMainGPU].m_pISharedFence)));

			// �������Χ����ͨ�������ʽ
			HANDLE hFenceShared = nullptr;
			GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateSharedHandle(
				stGPU[c_nMainGPU].m_pISharedFence.Get(),
				nullptr,
				GENERIC_ALL,
				nullptr,
				&hFenceShared));

			// �ڸ����Կ��ϴ����Χ��������ɹ���
			HRESULT hrOpenSharedHandleResult
				= stGPU[c_nSecondGPU].m_pID3D12Device4->OpenSharedHandle(
					hFenceShared
					, IID_PPV_ARGS(&stGPU[c_nSecondGPU].m_pISharedFence));

			// �ȹرվ�������ж��Ƿ���ɹ�
			::CloseHandle(hFenceShared);
			GRS_THROW_IF_FAILED(hrOpenSharedHandleResult);
		}

		// 6�����������������������
		{
			DXGI_SWAP_CHAIN_DESC1 stSwapChainDesc = {};
			stSwapChainDesc.BufferCount = g_nFrameBackBufCount;
			stSwapChainDesc.Width = g_iWndWidth;
			stSwapChainDesc.Height = g_iWndHeight;
			stSwapChainDesc.Format = emRTFormat;
			stSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			stSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			stSwapChainDesc.SampleDesc.Count = 1;

			//�ڵڶ����Կ���Present
			GRS_THROW_IF_FAILED(pIDXGIFactory5->CreateSwapChainForHwnd(
				stGPU[c_nSecondGPU].m_pICMDQueue.Get(),		// Swap chain needs the queue so that it can force a flush on it.
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
			nCurrentFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

			//����RTV(��ȾĿ����ͼ)��������(����ѵĺ���Ӧ�����Ϊ������߹̶���СԪ�صĹ̶���С�Դ��)
			D3D12_DESCRIPTOR_HEAP_DESC stRTVHeapDesc = {};
			stRTVHeapDesc.NumDescriptors = g_nFrameBackBufCount;
			stRTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			stRTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

			for (int i = 0; i < c_nMaxGPUCnt; i++)
			{
				GRS_THROW_IF_FAILED(stGPU[i].m_pID3D12Device4->CreateDescriptorHeap(&stRTVHeapDesc, IID_PPV_ARGS(&stGPU[i].m_pIRTVHeap)));
				GRS_SetD3D12DebugNameIndexed(stGPU[i].m_pIRTVHeap.Get(), _T("m_pIRTVHeap"), i);
			}

			for (int i = 0; i < c_nPostPassCnt; i++)
			{
				//Off-Line Render Target View
				GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pID3D12Device4->CreateDescriptorHeap(
					&stRTVHeapDesc, IID_PPV_ARGS(&pIRTVOffLine[i])));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRTVOffLine[i]);

			}

			//������ȾĿ�����������Լ����Կ�����ȾĿ����Դ
			D3D12_CLEAR_VALUE   stClearValue = {};
			stClearValue.Format = stSwapChainDesc.Format;
			stClearValue.Color[0] = v4ClearColor[0];
			stClearValue.Color[1] = v4ClearColor[1];
			stClearValue.Color[2] = v4ClearColor[2];
			stClearValue.Color[3] = v4ClearColor[3];

			stRenderTargetDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			stRenderTargetDesc.Alignment = 0;
			stRenderTargetDesc.Format = stSwapChainDesc.Format;
			stRenderTargetDesc.Width = stSwapChainDesc.Width;
			stRenderTargetDesc.Height = stSwapChainDesc.Height;
			stRenderTargetDesc.DepthOrArraySize = 1;
			stRenderTargetDesc.MipLevels = 1;
			stRenderTargetDesc.SampleDesc.Count = stSwapChainDesc.SampleDesc.Count;
			stRenderTargetDesc.SampleDesc.Quality = stSwapChainDesc.SampleDesc.Quality;
			stRenderTargetDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			stRenderTargetDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			WCHAR pszDebugName[MAX_PATH] = {};

			D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandleMainGPU = stGPU[c_nMainGPU].m_pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
			D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandleSecondGPU = stGPU[c_nSecondGPU].m_pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
			for (UINT j = 0; j < g_nFrameBackBufCount; j++)
			{
				//�ڸ����Կ������
				GRS_THROW_IF_FAILED(pISwapChain3->GetBuffer(j, IID_PPV_ARGS(&stGPU[c_nSecondGPU].m_pIRTRes[j])));
				//�����Կ��ϴ�����ȾĿ��������Դ
				GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateCommittedResource(
					&stDefautHeapProps
					, D3D12_HEAP_FLAG_NONE
					, &stRenderTargetDesc
					, D3D12_RESOURCE_STATE_COMMON
					, &stClearValue
					, IID_PPV_ARGS(&stGPU[c_nMainGPU].m_pIRTRes[j])));

				stGPU[c_nMainGPU].m_pID3D12Device4->CreateRenderTargetView(stGPU[c_nMainGPU].m_pIRTRes[j].Get(), nullptr, stRTVHandleMainGPU);
				stRTVHandleMainGPU.ptr += stGPU[c_nMainGPU].m_nRTVDescriptorSize;

				stGPU[c_nSecondGPU].m_pID3D12Device4->CreateRenderTargetView(stGPU[c_nSecondGPU].m_pIRTRes[j].Get(), nullptr, stRTVHandleSecondGPU);
				stRTVHandleSecondGPU.ptr += stGPU[c_nSecondGPU].m_nRTVDescriptorSize;
			}

			for (int i = 0; i < c_nMaxGPUCnt; i++)
			{
				// ����ÿ���Կ��ϵ�������建����
				D3D12_CLEAR_VALUE stDepthOptimizedClearValue = {};
				stDepthOptimizedClearValue.Format = emDSFormat;
				stDepthOptimizedClearValue.DepthStencil.Depth = 1.0f;
				stDepthOptimizedClearValue.DepthStencil.Stencil = 0;

				//ʹ����ʽĬ�϶Ѵ���һ��������建������
				//��Ϊ��������Ȼ�������һֱ��ʹ�ã����õ����岻��
				//����ֱ��ʹ����ʽ�ѣ�ͼ����
				D3D12_RESOURCE_DESC stDSResDesc = {};
				stDSResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
				stDSResDesc.Alignment = 0;
				stDSResDesc.Format = emDSFormat;
				stDSResDesc.Width = g_iWndWidth;
				stDSResDesc.Height = g_iWndHeight;
				stDSResDesc.DepthOrArraySize = 1;
				stDSResDesc.MipLevels = 0;
				stDSResDesc.SampleDesc.Count = 1;
				stDSResDesc.SampleDesc.Quality = 0;
				stDSResDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
				stDSResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

				GRS_THROW_IF_FAILED(stGPU[i].m_pID3D12Device4->CreateCommittedResource(
					&stDefautHeapProps
					, D3D12_HEAP_FLAG_NONE
					, &stDSResDesc
					, D3D12_RESOURCE_STATE_DEPTH_WRITE
					, &stDepthOptimizedClearValue
					, IID_PPV_ARGS(&stGPU[i].m_pIDSRes)
				));
				GRS_SetD3D12DebugNameIndexed(stGPU[i].m_pIDSRes.Get(), _T("m_pIDSRes"), i);

				// Create DSV Heap
				D3D12_DESCRIPTOR_HEAP_DESC stDSVHeapDesc = {};
				stDSVHeapDesc.NumDescriptors = 1;
				stDSVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
				stDSVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
				GRS_THROW_IF_FAILED(stGPU[i].m_pID3D12Device4->CreateDescriptorHeap(&stDSVHeapDesc, IID_PPV_ARGS(&stGPU[i].m_pIDSVHeap)));
				GRS_SetD3D12DebugNameIndexed(stGPU[i].m_pIDSVHeap.Get(), _T("m_pIDSVHeap"), i);

				// Create DSV
				D3D12_DEPTH_STENCIL_VIEW_DESC stDepthStencilDesc = {};
				stDepthStencilDesc.Format = emDSFormat;
				stDepthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
				stDepthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

				stGPU[i].m_pID3D12Device4->CreateDepthStencilView(stGPU[i].m_pIDSRes.Get()
					, &stDepthStencilDesc
					, stGPU[i].m_pIDSVHeap->GetCPUDescriptorHandleForHeapStart());
			}

			for (UINT k = 0; k < c_nPostPassCnt; k++)
			{
				D3D12_CPU_DESCRIPTOR_HANDLE stHRTVOffLine = pIRTVOffLine[k]->GetCPUDescriptorHandleForHeapStart();
				for (int i = 0; i < g_nFrameBackBufCount; i++)
				{
					// Create Second Adapter Off-Line Render Target
					GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pID3D12Device4->CreateCommittedResource(
						&stDefautHeapProps
						, D3D12_HEAP_FLAG_NONE
						, &stRenderTargetDesc
						, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
						, &stClearValue
						, IID_PPV_ARGS(&pIOffLineRTRes[k][i])));

					GRS_SET_D3D12_DEBUGNAME_INDEXED_COMPTR(pIOffLineRTRes[k], i);
					// Off-Line Render Target View
					stGPU[c_nSecondGPU].m_pID3D12Device4->CreateRenderTargetView(pIOffLineRTRes[k][i].Get(), nullptr, stHRTVOffLine);
					stHRTVOffLine.ptr += stGPU[c_nSecondGPU].m_nRTVDescriptorSize;
				}
			}


			// �������������Ĺ�����Դ��
			{
				D3D12_FEATURE_DATA_D3D12_OPTIONS stOptions = {};
				// ͨ����������ʾ������Կ��Ƿ�֧�ֿ��Կ���Դ���������Կ�����Դ��δ���
				GRS_THROW_IF_FAILED(
					stGPU[c_nSecondGPU].m_pID3D12Device4->CheckFeatureSupport(
						D3D12_FEATURE_D3D12_OPTIONS
						, reinterpret_cast<void*>(&stOptions)
						, sizeof(stOptions)));

				bCrossAdapterTextureSupport = stOptions.CrossAdapterRowMajorTextureSupported;

				UINT64 n64szTexture = 0;
				D3D12_RESOURCE_DESC stCrossAdapterResDesc = {};

				if (bCrossAdapterTextureSupport)
				{
					::OutputDebugString(_T("\n�����Կ�ֱ��֧�ֿ��Կ���������!\n"));
					// ���֧����ôֱ�Ӵ������Կ��������͵���Դ��
					// ������ʽ�����ʱ������������������洢��
					stCrossAdapterResDesc = stRenderTargetDesc;
					stCrossAdapterResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
					stCrossAdapterResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

					D3D12_RESOURCE_ALLOCATION_INFO stTextureInfo
						= stGPU[c_nMainGPU].m_pID3D12Device4->GetResourceAllocationInfo(
							0, 1, &stCrossAdapterResDesc);
					n64szTexture = stTextureInfo.SizeInBytes;
				}
				else
				{
					::OutputDebugString(_T("\n�����Կ���֧�ֿ��Կ������������Ŵ�����������\n"));
					// �����֧���������͵���Դ�ѹ�����ô�ʹ���Buffer���͵Ĺ�����Դ��
					D3D12_PLACED_SUBRESOURCE_FOOTPRINT stResLayout = {};
					stGPU[c_nMainGPU].m_pID3D12Device4->GetCopyableFootprints(
						&stRenderTargetDesc, 0, 1, 0, &stResLayout, nullptr, nullptr, nullptr);
					//��ͨ��Դ�ֽڳߴ��СҪ64k�߽����
					n64szTexture = GRS_UPPER(stResLayout.Footprint.RowPitch * stResLayout.Footprint.Height
						, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);

					stCrossAdapterResDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
					stCrossAdapterResDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
					stCrossAdapterResDesc.Width = n64szTexture;
					stCrossAdapterResDesc.Height = 1;
					stCrossAdapterResDesc.DepthOrArraySize = 1;
					stCrossAdapterResDesc.MipLevels = 1;
					stCrossAdapterResDesc.Format = DXGI_FORMAT_UNKNOWN;
					stCrossAdapterResDesc.SampleDesc.Count = 1;
					stCrossAdapterResDesc.SampleDesc.Quality = 0;
					stCrossAdapterResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
					stCrossAdapterResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
				}

				// �������Կ��������Դ��
				// ������Ҫע��ľ��ǣ������������������������Կ������ٶ�֧��Buffer���͵���Դ����
				// ����������Ϊ�����ڴ����ǿ����������Կ��乲��
				// ֻ������ͨ�ķ�ʽ����Buffer��ʽ����GPU���ܵ�������������
				D3D12_HEAP_DESC stCrossHeapDesc = {};
				stCrossHeapDesc.SizeInBytes = n64szTexture * g_nFrameBackBufCount;
				stCrossHeapDesc.Alignment = 0;
				stCrossHeapDesc.Flags = D3D12_HEAP_FLAG_SHARED | D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER;
				stCrossHeapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
				stCrossHeapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
				stCrossHeapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
				stCrossHeapDesc.Properties.CreationNodeMask = 0;
				stCrossHeapDesc.Properties.VisibleNodeMask = 0;

				GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateHeap(&stCrossHeapDesc
					, IID_PPV_ARGS(&stGPU[c_nMainGPU].m_pICrossAdapterHeap)));

				HANDLE hHeap = nullptr;
				GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateSharedHandle(
					stGPU[c_nMainGPU].m_pICrossAdapterHeap.Get(),
					nullptr,
					GENERIC_ALL,
					nullptr,
					&hHeap));

				HRESULT hrOpenSharedHandle = stGPU[c_nSecondGPU].m_pID3D12Device4->OpenSharedHandle(
					hHeap
					, IID_PPV_ARGS(&stGPU[c_nSecondGPU].m_pICrossAdapterHeap));

				// �ȹرվ�������ж��Ƿ���ɹ�
				::CloseHandle(hHeap);

				GRS_THROW_IF_FAILED(hrOpenSharedHandle);

				// �Զ�λ��ʽ�ڹ�����ϴ���ÿ���Կ��ϵ���Դ
				for (UINT i = 0; i < g_nFrameBackBufCount; i++)
				{
					GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreatePlacedResource(
						stGPU[c_nMainGPU].m_pICrossAdapterHeap.Get(),
						n64szTexture * i,
						&stCrossAdapterResDesc,
						D3D12_RESOURCE_STATE_COPY_DEST,
						nullptr,
						IID_PPV_ARGS(&stGPU[c_nMainGPU].m_pICrossAdapterResPerFrame[i])));

					GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pID3D12Device4->CreatePlacedResource(
						stGPU[c_nSecondGPU].m_pICrossAdapterHeap.Get(),
						n64szTexture * i,
						&stCrossAdapterResDesc,
						bCrossAdapterTextureSupport ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COPY_SOURCE,
						nullptr,
						IID_PPV_ARGS(&stGPU[c_nSecondGPU].m_pICrossAdapterResPerFrame[i])));

					if (!bCrossAdapterTextureSupport)
					{
						// ���������Դ���ܹ���Buffer��ʽ����Դ����ô���ڵڶ����Կ��ϴ�����Ӧ��������ʽ����Դ
						// ���ҽ����Կ����������Buffer�����Ⱦ����������Ƶ��ڶ����Կ���������ʽ����Դ��
						GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pID3D12Device4->CreateCommittedResource(
							&stDefautHeapProps
							, D3D12_HEAP_FLAG_NONE
							, &stRenderTargetDesc
							, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
							, nullptr
							, IID_PPV_ARGS(&pISecondaryAdapterTexutrePerFrame[i])));
					}
				}

			}

			// �ر�ALT+ENTER���л�ȫ���Ĺ��ܣ���Ϊ����û��ʵ��OnSize���������ȹر�
			GRS_THROW_IF_FAILED(pIDXGIFactory5->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
		}

		// 7��������ǩ��
		{ {
				//First Pass Root Signature
				//��������У���һ����Ⱦ�����Կ���ɣ���������ʹ����ͬ�ĸ�ǩ������Ϊ��Ⱦ��������Ҫ�Ĳ�����һ����
				D3D12_FEATURE_DATA_ROOT_SIGNATURE stFeatureData = {};
				//����Ƿ�֧��V1.1�汾�ĸ�ǩ��
				stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

				if (FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CheckFeatureSupport(
					D3D12_FEATURE_ROOT_SIGNATURE
					, &stFeatureData
					, sizeof(stFeatureData))))
				{// 1.0�� ֱ�Ӷ��쳣�˳���
					GRS_THROW_IF_FAILED(E_NOTIMPL);
				}

				D3D12_DESCRIPTOR_RANGE1 stDSPRanges[3] = {};
				stDSPRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
				stDSPRanges[0].NumDescriptors = 1; //1 Const Buffer View
				stDSPRanges[0].BaseShaderRegister = 0;
				stDSPRanges[0].RegisterSpace = 0;
				stDSPRanges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
				stDSPRanges[0].OffsetInDescriptorsFromTableStart = 0;

				stDSPRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				stDSPRanges[1].NumDescriptors = 1; //1 Texture View
				stDSPRanges[1].BaseShaderRegister = 0;
				stDSPRanges[1].RegisterSpace = 0;
				stDSPRanges[1].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
				stDSPRanges[1].OffsetInDescriptorsFromTableStart = 0;

				stDSPRanges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
				stDSPRanges[2].NumDescriptors = 1; // 1 Sampler
				stDSPRanges[2].BaseShaderRegister = 0;
				stDSPRanges[2].RegisterSpace = 0;
				stDSPRanges[2].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
				stDSPRanges[2].OffsetInDescriptorsFromTableStart = 0;

				D3D12_ROOT_PARAMETER1 stRootParameters[3] = {};

				stRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				stRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;//CBV������Shader�ɼ�
				stRootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
				stRootParameters[0].DescriptorTable.pDescriptorRanges = &stDSPRanges[0];

				stRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				stRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//SRV��PS�ɼ�
				stRootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
				stRootParameters[1].DescriptorTable.pDescriptorRanges = &stDSPRanges[1];

				stRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				stRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//SAMPLE��PS�ɼ�
				stRootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
				stRootParameters[2].DescriptorTable.pDescriptorRanges = &stDSPRanges[2];

				D3D12_VERSIONED_ROOT_SIGNATURE_DESC stRootSignatureDesc = {};
				stRootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
				stRootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
				stRootSignatureDesc.Desc_1_1.NumParameters = _countof(stRootParameters);
				stRootSignatureDesc.Desc_1_1.pParameters = stRootParameters;
				stRootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
				stRootSignatureDesc.Desc_1_1.pStaticSamplers = nullptr;

				ComPtr<ID3DBlob> pISignatureBlob;
				ComPtr<ID3DBlob> pIErrorBlob;

				GRS_THROW_IF_FAILED(D3D12SerializeVersionedRootSignature(&stRootSignatureDesc
					, &pISignatureBlob
					, &pIErrorBlob));

				GRS_THROW_IF_FAILED(
					stGPU[c_nMainGPU].m_pID3D12Device4->CreateRootSignature(0
						, pISignatureBlob->GetBufferPointer()
						, pISignatureBlob->GetBufferSize()
						, IID_PPV_ARGS(&stGPU[c_nMainGPU].m_pIRS)));

				GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPU[c_nMainGPU].m_pIRS);

				//Second Pass Root Signature
				//������ȾQuad�ĸ�ǩ������ע�������ǩ���ڸ����Կ�����
				// ��Ϊ���ǰ����е���ȾĿ�������SRV����������һ��SRV�ѵ�����λ���ϣ�ʵ��ֻʹ��һ��������Ҫ��Noise Texture����һ��
				D3D12_DESCRIPTOR_RANGE1 stDRQuad[4] = {};

				stDRQuad[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
				stDRQuad[0].NumDescriptors = 1; //1 Const Buffer View
				stDRQuad[0].BaseShaderRegister = 0;
				stDRQuad[0].RegisterSpace = 0;
				stDRQuad[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
				stDRQuad[0].OffsetInDescriptorsFromTableStart = 0;

				stDRQuad[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				stDRQuad[1].NumDescriptors = 1; //1 Texture View
				stDRQuad[1].BaseShaderRegister = 0;
				stDRQuad[1].RegisterSpace = 0;
				stDRQuad[1].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
				stDRQuad[1].OffsetInDescriptorsFromTableStart = 0;

				stDRQuad[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				stDRQuad[2].NumDescriptors = 1; // 1 Noise Texture 
				stDRQuad[2].BaseShaderRegister = 1;
				stDRQuad[2].RegisterSpace = 0;
				stDRQuad[2].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
				stDRQuad[2].OffsetInDescriptorsFromTableStart = 0;

				stDRQuad[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
				stDRQuad[3].NumDescriptors = 1; // 1 Sampler
				stDRQuad[3].BaseShaderRegister = 0;
				stDRQuad[3].RegisterSpace = 0;
				stDRQuad[3].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
				stDRQuad[3].OffsetInDescriptorsFromTableStart = 0;

				//������ʵ����ͼ����������Դ���趨Pixel Shader�ɼ�
				D3D12_ROOT_PARAMETER1 stRPQuad[4] = {};
				stRPQuad[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				stRPQuad[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//CBV��PS�ɼ�
				stRPQuad[0].DescriptorTable.NumDescriptorRanges = 1;
				stRPQuad[0].DescriptorTable.pDescriptorRanges = &stDRQuad[0];

				stRPQuad[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				stRPQuad[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//SRV��PS�ɼ�
				stRPQuad[1].DescriptorTable.NumDescriptorRanges = 1;
				stRPQuad[1].DescriptorTable.pDescriptorRanges = &stDRQuad[1];

				stRPQuad[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				stRPQuad[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//SRV��PS�ɼ�
				stRPQuad[2].DescriptorTable.NumDescriptorRanges = 1;
				stRPQuad[2].DescriptorTable.pDescriptorRanges = &stDRQuad[2];


				stRPQuad[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				stRPQuad[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//SAMPLE��PS�ɼ�
				stRPQuad[3].DescriptorTable.NumDescriptorRanges = 1;
				stRPQuad[3].DescriptorTable.pDescriptorRanges = &stDRQuad[3];

				D3D12_VERSIONED_ROOT_SIGNATURE_DESC stRSQuadDesc = {};
				stRSQuadDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
				stRSQuadDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
				stRSQuadDesc.Desc_1_1.NumParameters = _countof(stRPQuad);
				stRSQuadDesc.Desc_1_1.pParameters = stRPQuad;
				stRSQuadDesc.Desc_1_1.NumStaticSamplers = 0;
				stRSQuadDesc.Desc_1_1.pStaticSamplers = nullptr;

				pISignatureBlob.Reset();
				pIErrorBlob.Reset();

				GRS_THROW_IF_FAILED(D3D12SerializeVersionedRootSignature(&stRSQuadDesc
					, &pISignatureBlob
					, &pIErrorBlob));

				GRS_THROW_IF_FAILED(
					stGPU[c_nSecondGPU].m_pID3D12Device4->CreateRootSignature(0
						, pISignatureBlob->GetBufferPointer()
						, pISignatureBlob->GetBufferSize()
						, IID_PPV_ARGS(&stGPU[c_nSecondGPU].m_pIRS)));

				GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPU[c_nSecondGPU].m_pIRS);

				//-----------------------------------------------------------------------------------------------------
				// Create Third Pass Root Signature
				D3D12_DESCRIPTOR_RANGE1 stDRThridPass[2] = {};
				stDRThridPass[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				stDRThridPass[0].NumDescriptors = 1; // 1 Noise Texture 
				stDRThridPass[0].BaseShaderRegister = 0;
				stDRThridPass[0].RegisterSpace = 0;
				stDRThridPass[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
				stDRThridPass[0].OffsetInDescriptorsFromTableStart = 0;

				stDRThridPass[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
				stDRThridPass[1].NumDescriptors = 1; // 1 Sampler
				stDRThridPass[1].BaseShaderRegister = 0;
				stDRThridPass[1].RegisterSpace = 0;
				stDRThridPass[1].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
				stDRThridPass[1].OffsetInDescriptorsFromTableStart = 0;

				D3D12_ROOT_PARAMETER1 stRPThirdPass[2] = {};
				stRPThirdPass[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				stRPThirdPass[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//SRV��PS�ɼ�
				stRPThirdPass[0].DescriptorTable.NumDescriptorRanges = 1;
				stRPThirdPass[0].DescriptorTable.pDescriptorRanges = &stDRThridPass[0];

				stRPThirdPass[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				stRPThirdPass[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//Sample��PS�ɼ�
				stRPThirdPass[1].DescriptorTable.NumDescriptorRanges = 1;
				stRPThirdPass[1].DescriptorTable.pDescriptorRanges = &stDRThridPass[1];

				D3D12_VERSIONED_ROOT_SIGNATURE_DESC stRSThirdPass = {};
				stRSThirdPass.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
				stRSThirdPass.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
				stRSThirdPass.Desc_1_1.NumParameters = _countof(stRPThirdPass);
				stRSThirdPass.Desc_1_1.pParameters = stRPThirdPass;
				stRSThirdPass.Desc_1_1.NumStaticSamplers = 0;
				stRSThirdPass.Desc_1_1.pStaticSamplers = nullptr;

				pISignatureBlob.Reset();
				pIErrorBlob.Reset();

				GRS_THROW_IF_FAILED(D3D12SerializeVersionedRootSignature(&stRSThirdPass
					, &pISignatureBlob
					, &pIErrorBlob));

				GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pID3D12Device4->CreateRootSignature(0
					, pISignatureBlob->GetBufferPointer()
					, pISignatureBlob->GetBufferSize()
					, IID_PPV_ARGS(&pIRSPostPass)));

				GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRSPostPass);
			}}

		// 8������Shader������Ⱦ����״̬����
		{ {
#if defined(_DEBUG)
				// Enable better shader debugging with the graphics debugging tools.
				UINT nShaderCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
				UINT nShaderCompileFlags = 0;
#endif
				//����Ϊ�о�����ʽ
				nShaderCompileFlags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

				TCHAR pszShaderFileName[MAX_PATH] = {};

				ComPtr<ID3DBlob> pIVSCode;
				ComPtr<ID3DBlob> pIPSCode;
				ComPtr<ID3DBlob> pIErrMsg;
				CHAR pszErrMsg[MAX_PATH] = {};
				HRESULT hr = S_OK;

				StringCchPrintf(pszShaderFileName
					, MAX_PATH
					, _T("%s11-MultiThreadAndAdapter\\Shader\\11-MultiThreadAndAdapter.hlsl")
					, g_pszAppPath);

				hr = D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
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

				hr = D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
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
					{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
					{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
				};

				// ���� graphics pipeline state object (PSO)����
				D3D12_GRAPHICS_PIPELINE_STATE_DESC stPSODesc = {};
				stPSODesc.InputLayout = { stIALayoutSphere, _countof(stIALayoutSphere) };
				stPSODesc.pRootSignature = stGPU[c_nMainGPU].m_pIRS.Get();
				stPSODesc.VS.BytecodeLength = pIVSCode->GetBufferSize();
				stPSODesc.VS.pShaderBytecode = pIVSCode->GetBufferPointer();
				stPSODesc.PS.BytecodeLength = pIPSCode->GetBufferSize();
				stPSODesc.PS.pShaderBytecode = pIPSCode->GetBufferPointer();

				stPSODesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
				stPSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;

				stPSODesc.BlendState.AlphaToCoverageEnable = FALSE;
				stPSODesc.BlendState.IndependentBlendEnable = FALSE;
				stPSODesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

				stPSODesc.SampleMask = UINT_MAX;
				stPSODesc.SampleDesc.Count = 1;
				stPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				stPSODesc.NumRenderTargets = 1;
				stPSODesc.RTVFormats[0] = emRTFormat;
				stPSODesc.DSVFormat = emDSFormat;
				stPSODesc.DepthStencilState.StencilEnable = FALSE;
				stPSODesc.DepthStencilState.DepthEnable = TRUE;			//����Ȼ���				
				stPSODesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;//������Ȼ���д�빦��
				stPSODesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;     //��Ȳ��Ժ�������ֵΪ��ͨ����Ȳ��ԣ�����Сֵд�룩

				GRS_THROW_IF_FAILED(
					stGPU[c_nMainGPU].m_pID3D12Device4->CreateGraphicsPipelineState(
						&stPSODesc
						, IID_PPV_ARGS(&stGPU[c_nMainGPU].m_pIPSO)));

				GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPU[c_nMainGPU].m_pIPSO);

				// Second Pass Pipeline State Object
				// ������Ⱦ��λ���εĹܵ�״̬�����ڱ������Ǻ���
				StringCchPrintf(pszShaderFileName, MAX_PATH, _T("%s11-MultiThreadAndAdapter\\Shader\\11-QuadVS.hlsl"), g_pszAppPath);

				pIVSCode.Reset();
				pIPSCode.Reset();
				pIErrMsg.Reset();

				hr = D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
					, "VSMain", "vs_5_0", nShaderCompileFlags, 0, &pIVSCode, &pIErrMsg);
				if (FAILED(hr))
				{
					if (nullptr != pIErrMsg)
					{
						StringCchPrintfA(pszErrMsg, MAX_PATH, "\n%s\n", (CHAR*)pIErrMsg->GetBufferPointer());
						::OutputDebugStringA(pszErrMsg);
					}
					throw CGRSCOMException(hr);
				}

				pIErrMsg.Reset();
				StringCchPrintf(pszShaderFileName, MAX_PATH
					, _T("%s11-MultiThreadAndAdapter\\Shader\\11-WaterColourPS.hlsl")
					, g_pszAppPath);

				hr = D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
					, "PSMain", "ps_5_0", nShaderCompileFlags, 0, &pIPSCode, &pIErrMsg);
				if (FAILED(hr))
				{
					if (nullptr != pIErrMsg)
					{
						StringCchPrintfA(pszErrMsg, MAX_PATH, "\n%s\n", (CHAR*)pIErrMsg->GetBufferPointer());
						::OutputDebugStringA(pszErrMsg);
					}
					throw CGRSCOMException(hr);
				}
				// ���Ƕ������һ�����ߵĶ��壬��ĿǰShader�����ǲ�û��ʹ��
				D3D12_INPUT_ELEMENT_DESC stIALayoutQuad[] =
				{
					{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
					{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
				};

				// ���� graphics pipeline state object (PSO)����
				D3D12_GRAPHICS_PIPELINE_STATE_DESC stPSOQuadDesc = {};
				stPSOQuadDesc.InputLayout = { stIALayoutQuad, _countof(stIALayoutQuad) };
				stPSOQuadDesc.pRootSignature = stGPU[c_nSecondGPU].m_pIRS.Get();
				stPSOQuadDesc.VS.BytecodeLength = pIVSCode->GetBufferSize();
				stPSOQuadDesc.VS.pShaderBytecode = pIVSCode->GetBufferPointer();
				stPSOQuadDesc.PS.BytecodeLength = pIPSCode->GetBufferSize();
				stPSOQuadDesc.PS.pShaderBytecode = pIPSCode->GetBufferPointer();

				stPSOQuadDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
				stPSOQuadDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;

				stPSOQuadDesc.BlendState.AlphaToCoverageEnable = FALSE;
				stPSOQuadDesc.BlendState.IndependentBlendEnable = FALSE;
				stPSOQuadDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

				stPSOQuadDesc.SampleMask = UINT_MAX;
				stPSOQuadDesc.SampleDesc.Count = 1;
				stPSOQuadDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				stPSOQuadDesc.NumRenderTargets = 1;
				stPSOQuadDesc.RTVFormats[0] = emRTFormat;
				stPSOQuadDesc.DepthStencilState.StencilEnable = FALSE;
				stPSOQuadDesc.DepthStencilState.DepthEnable = FALSE;

				GRS_THROW_IF_FAILED(
					stGPU[c_nSecondGPU].m_pID3D12Device4->CreateGraphicsPipelineState(
						&stPSOQuadDesc
						, IID_PPV_ARGS(&stGPU[c_nSecondGPU].m_pIPSO)));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPU[c_nSecondGPU].m_pIPSO);

				//---------------------------------------------------------------------------------------------------
				// Create Gaussian blur in the vertical direction PSO
				pIPSCode.Reset();
				pIErrMsg.Reset();

				StringCchPrintf(pszShaderFileName
					, MAX_PATH
					, _T("%s11-MultiThreadAndAdapter\\Shader\\11-GaussianBlurPS.hlsl")
					, g_pszAppPath);

				//������Ⱦ��ֻ����PS�Ϳ����ˣ�VS����֮ǰ��QuadVS���ɣ����Ǻ�����Ҫ��PS
				hr = D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
					, "PSSimpleBlurV", "ps_5_0", nShaderCompileFlags, 0, &pIPSCode, &pIErrMsg);
				if (FAILED(hr))
				{
					if (nullptr != pIErrMsg)
					{
						StringCchPrintfA(pszErrMsg, MAX_PATH, "\n%s\n", (CHAR*)pIErrMsg->GetBufferPointer());
						::OutputDebugStringA(pszErrMsg);
					}
					throw CGRSCOMException(hr);
				}

				// ���� graphics pipeline state object (PSO)����
				D3D12_GRAPHICS_PIPELINE_STATE_DESC stPSOThirdPassDesc = {};
				stPSOThirdPassDesc.InputLayout = { stIALayoutQuad, _countof(stIALayoutQuad) };
				stPSOThirdPassDesc.pRootSignature = pIRSPostPass.Get();
				stPSOThirdPassDesc.VS.BytecodeLength = pIVSCode->GetBufferSize();
				stPSOThirdPassDesc.VS.pShaderBytecode = pIVSCode->GetBufferPointer();
				stPSOThirdPassDesc.PS.BytecodeLength = pIPSCode->GetBufferSize();
				stPSOThirdPassDesc.PS.pShaderBytecode = pIPSCode->GetBufferPointer();

				stPSOThirdPassDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
				stPSOThirdPassDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;

				stPSOThirdPassDesc.BlendState.AlphaToCoverageEnable = FALSE;
				stPSOThirdPassDesc.BlendState.IndependentBlendEnable = FALSE;
				stPSOThirdPassDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

				stPSOThirdPassDesc.SampleMask = UINT_MAX;
				stPSOThirdPassDesc.SampleDesc.Count = 1;
				stPSOThirdPassDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				stPSOThirdPassDesc.NumRenderTargets = 1;
				stPSOThirdPassDesc.RTVFormats[0] = emRTFormat;
				stPSOThirdPassDesc.DepthStencilState.StencilEnable = FALSE;
				stPSOThirdPassDesc.DepthStencilState.DepthEnable = FALSE;

				GRS_THROW_IF_FAILED(
					stGPU[c_nSecondGPU].m_pID3D12Device4->CreateGraphicsPipelineState(
						&stPSOThirdPassDesc
						, IID_PPV_ARGS(&pIPSOPostPass[c_nPostPass0])));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(pIPSOPostPass[c_nPostPass0]);

				//  Create Gaussian blur in the horizontal direction PSO
				pIPSCode.Reset();
				pIErrMsg.Reset();

				//������Ⱦ��ֻ����PS�Ϳ����ˣ�VS����֮ǰ��QuadVS���ɣ����Ǻ�����Ҫ��PS
				hr = D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
					, "PSSimpleBlurU", "ps_5_0", nShaderCompileFlags, 0, &pIPSCode, &pIErrMsg);
				if (FAILED(hr))
				{
					if (nullptr != pIErrMsg)
					{
						StringCchPrintfA(pszErrMsg, MAX_PATH, "\n%s\n", (CHAR*)pIErrMsg->GetBufferPointer());
						::OutputDebugStringA(pszErrMsg);
					}
					throw CGRSCOMException(hr);
				}

				stPSOThirdPassDesc.PS.BytecodeLength = pIPSCode->GetBufferSize();
				stPSOThirdPassDesc.PS.pShaderBytecode = pIPSCode->GetBufferPointer();

				GRS_THROW_IF_FAILED(
					stGPU[c_nSecondGPU].m_pID3D12Device4->CreateGraphicsPipelineState(
						&stPSOThirdPassDesc
						, IID_PPV_ARGS(&pIPSOPostPass[c_nPostPass1])));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(pIPSOPostPass[c_nPostPass1]);
				//---------------------------------------------------------------------------------------------------
			}}

		// 9��׼�����������������Ⱦ�߳�
		{ {
				USES_CONVERSION;
				// ������Բ���
				StringCchPrintf(g_stThread[g_nThdSphere].m_pszDDSFile, MAX_PATH, _T("%sAssets\\Earth_512.dds"), g_pszAppPath);
				StringCchPrintfA(g_stThread[g_nThdSphere].m_pszMeshFile, MAX_PATH, "%sAssets\\sphere.txt", T2A(g_pszAppPath));
				g_stThread[g_nThdSphere].m_v4ModelPos = XMFLOAT4(2.0f, 2.0f, 0.0f, 1.0f);

				// ��������Բ���
				StringCchPrintf(g_stThread[g_nThdCube].m_pszDDSFile, MAX_PATH, _T("%sAssets\\Cube.dds"), g_pszAppPath);
				StringCchPrintfA(g_stThread[g_nThdCube].m_pszMeshFile, MAX_PATH, "%sAssets\\Cube.txt", T2A(g_pszAppPath));
				g_stThread[g_nThdCube].m_v4ModelPos = XMFLOAT4(-2.0f, 2.0f, 0.0f, 1.0f);

				// ƽ����Բ���
				StringCchPrintf(g_stThread[g_nThdPlane].m_pszDDSFile, MAX_PATH, _T("%sAssets\\Plane.dds"), g_pszAppPath);
				StringCchPrintfA(g_stThread[g_nThdPlane].m_pszMeshFile, MAX_PATH, "%sAssets\\Plane.txt", T2A(g_pszAppPath));
				g_stThread[g_nThdPlane].m_v4ModelPos = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);

				// ���߳���Ⱦʱ����һ����Ⱦ��Ҫ�����Կ�����ɣ�����Ϊÿ���̴߳������Կ������������б�
				// ����Ĺ��Բ�����Ҳ���Ǹ��̵߳Ĺ��Բ���
				for (int i = 0; i < g_nMaxThread; i++)
				{
					g_stThread[i].m_nThreadIndex = i;		//��¼���

					//����ÿ���߳���Ҫ�������б�͸����������
					GRS_THROW_IF_FAILED(
						stGPU[c_nMainGPU].m_pID3D12Device4->CreateCommandAllocator(
							D3D12_COMMAND_LIST_TYPE_DIRECT
							, IID_PPV_ARGS(&g_stThread[i].m_pICMDAlloc)));
					GRS_SetD3D12DebugNameIndexed(g_stThread[i].m_pICMDAlloc, _T("pIThreadCmdAlloc"), i);

					GRS_THROW_IF_FAILED(
						stGPU[c_nMainGPU].m_pID3D12Device4->CreateCommandList(
							0, D3D12_COMMAND_LIST_TYPE_DIRECT
							, g_stThread[i].m_pICMDAlloc
							, nullptr
							, IID_PPV_ARGS(&g_stThread[i].m_pICMDList)));
					GRS_SetD3D12DebugNameIndexed(g_stThread[i].m_pICMDList, _T("pIThreadCmdList"), i);

					g_stThread[i].m_dwMainThreadID = ::GetCurrentThreadId();
					g_stThread[i].m_hMainThread = ::GetCurrentThread();
					g_stThread[i].m_hEventRun = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
					g_stThread[i].m_hEventRenderOver = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
					g_stThread[i].m_pID3D12Device4 = stGPU[c_nMainGPU].m_pID3D12Device4.Get();
					g_stThread[i].m_pIRTVHeap = stGPU[c_nMainGPU].m_pIRTVHeap.Get();
					g_stThread[i].m_pIDSVHeap = stGPU[c_nMainGPU].m_pIDSVHeap.Get();
					g_stThread[i].m_pIRS = stGPU[c_nMainGPU].m_pIRS.Get();
					g_stThread[i].m_pIPSO = stGPU[c_nMainGPU].m_pIPSO.Get();

					arHWaited.Add(g_stThread[i].m_hEventRenderOver); //��ӵ����ȴ�������

					//����ͣ��ʽ�����߳�
					g_stThread[i].m_hThisThread = (HANDLE)_beginthreadex(nullptr,
						0, RenderThread, (void*)&g_stThread[i],
						CREATE_SUSPENDED, (UINT*)&g_stThread[i].m_dwThisThreadID);

					//Ȼ���ж��̴߳����Ƿ�ɹ�
					if (nullptr == g_stThread[i].m_hThisThread
						|| reinterpret_cast<HANDLE>(-1) == g_stThread[i].m_hThisThread)
					{
						throw CGRSCOMException(HRESULT_FROM_WIN32(GetLastError()));
					}

					arHSubThread.Add(g_stThread[i].m_hThisThread);
				}

				//��һ�����߳�
				for (int i = 0; i < g_nMaxThread; i++)
				{
					::ResumeThread(g_stThread[i].m_hThisThread);
				}
			}}

		// 10������DDS��������ע�����ں������Լ��ص��ڶ����Կ���
		{
			TCHAR pszNoiseTexture[MAX_PATH] = {};
			StringCchPrintf(pszNoiseTexture, MAX_PATH, _T("%sAssets\\GaussianNoise256.dds"), g_pszAppPath);
			std::unique_ptr<uint8_t[]>			pbDDSData;
			std::vector<D3D12_SUBRESOURCE_DATA> stArSubResources;
			DDS_ALPHA_MODE						emAlphaMode = DDS_ALPHA_MODE_UNKNOWN;
			bool								bIsCube = false;

			GRS_THROW_IF_FAILED(LoadDDSTextureFromFile(
				stGPU[c_nSecondGPU].m_pID3D12Device4.Get()
				, pszNoiseTexture
				, pINoiseTexture.GetAddressOf()
				, pbDDSData
				, stArSubResources
				, SIZE_MAX
				, &emAlphaMode
				, &bIsCube));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pINoiseTexture);

			D3D12_RESOURCE_DESC stTXDesc = pINoiseTexture->GetDesc();
			UINT64 n64szUpSphere = 0;
			stGPU[c_nSecondGPU].m_pID3D12Device4->GetCopyableFootprints(&stTXDesc, 0, static_cast<UINT>(stArSubResources.size())
				, 0, nullptr, nullptr, nullptr, &n64szUpSphere);

			stBufferResSesc.Width = n64szUpSphere;
			GRS_THROW_IF_FAILED(
				stGPU[c_nSecondGPU].m_pID3D12Device4->CreateCommittedResource(
					&stUploadHeapProps
					, D3D12_HEAP_FLAG_NONE
					, &stBufferResSesc
					, D3D12_RESOURCE_STATE_GENERIC_READ
					, nullptr
					, IID_PPV_ARGS(&pINoiseTextureUpload)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pINoiseTextureUpload);

			//ִ������Copy�����������ϴ���Ĭ�϶���
			UINT nFirstSubresource = 0;
			UINT nNumSubresources = static_cast<UINT>(stArSubResources.size());
			D3D12_RESOURCE_DESC stUploadResDesc = pINoiseTextureUpload->GetDesc();
			D3D12_RESOURCE_DESC stDefaultResDesc = pINoiseTexture->GetDesc();

			UINT64 n64RequiredSize = 0;
			SIZE_T szMemToAlloc = static_cast<UINT64>(sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT)
				+ sizeof(UINT)
				+ sizeof(UINT64))
				* nNumSubresources;

			void* pMem = GRS_CALLOC(static_cast<SIZE_T>(szMemToAlloc));

			if (nullptr == pMem)
			{
				throw CGRSCOMException(HRESULT_FROM_WIN32(GetLastError()));
			}

			D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pLayouts = reinterpret_cast<D3D12_PLACED_SUBRESOURCE_FOOTPRINT*>(pMem);
			UINT64* pRowSizesInBytes = reinterpret_cast<UINT64*>(pLayouts + nNumSubresources);
			UINT* pNumRows = reinterpret_cast<UINT*>(pRowSizesInBytes + nNumSubresources);

			// �����ǵڶ��ε���GetCopyableFootprints���͵õ�����������Դ����ϸ��Ϣ
			stGPU[c_nSecondGPU].m_pID3D12Device4->GetCopyableFootprints(&stDefaultResDesc, nFirstSubresource, nNumSubresources, 0, pLayouts, pNumRows, pRowSizesInBytes, &n64RequiredSize);

			BYTE* pData = nullptr;
			GRS_THROW_IF_FAILED(pINoiseTextureUpload->Map(0, nullptr, reinterpret_cast<void**>(&pData)));
			// ��һ��Copy��ע��3��ѭ��ÿ�ص���˼
			for (UINT nSubRes = 0; nSubRes < nNumSubresources; ++nSubRes)
			{// SubResources
				if (pRowSizesInBytes[nSubRes] > (SIZE_T)-1)
				{
					throw CGRSCOMException(E_FAIL);
				}

				D3D12_MEMCPY_DEST stCopyDestData = { pData + pLayouts[nSubRes].Offset
					, pLayouts[nSubRes].Footprint.RowPitch
					, pLayouts[nSubRes].Footprint.RowPitch * pNumRows[nSubRes]
				};

				for (UINT z = 0; z < pLayouts[nSubRes].Footprint.Depth; ++z)
				{// Mipmap
					BYTE* pDestSlice = reinterpret_cast<BYTE*>(stCopyDestData.pData) + stCopyDestData.SlicePitch * z;
					const BYTE* pSrcSlice = reinterpret_cast<const BYTE*>(stArSubResources[nSubRes].pData) + stArSubResources[nSubRes].SlicePitch * z;
					for (UINT y = 0; y < pNumRows[nSubRes]; ++y)
					{// Rows
						memcpy(pDestSlice + stCopyDestData.RowPitch * y,
							pSrcSlice + stArSubResources[nSubRes].RowPitch * y,
							(SIZE_T)pRowSizesInBytes[nSubRes]);
					}
				}
			}
			pINoiseTextureUpload->Unmap(0, nullptr);

			// �ڶ���Copy��
			if (stDefaultResDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
			{// Buffer һ���Ը��ƾͿ����ˣ���Ϊû���ж�����д�С��һ�µ����⣬Buffer����������1
				stGPU[c_nSecondGPU].m_pICMDList->CopyBufferRegion(
					pINoiseTexture.Get(), 0, pINoiseTextureUpload.Get(), pLayouts[0].Offset, pLayouts[0].Footprint.Width);
			}
			else
			{
				for (UINT nSubRes = 0; nSubRes < nNumSubresources; ++nSubRes)
				{
					D3D12_TEXTURE_COPY_LOCATION stDstCopyLocation = {};
					stDstCopyLocation.pResource = pINoiseTexture.Get();
					stDstCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
					stDstCopyLocation.SubresourceIndex = nSubRes;

					D3D12_TEXTURE_COPY_LOCATION stSrcCopyLocation = {};
					stSrcCopyLocation.pResource = pINoiseTextureUpload.Get();
					stSrcCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
					stSrcCopyLocation.PlacedFootprint = pLayouts[nSubRes];

					stGPU[c_nSecondGPU].m_pICMDList->CopyTextureRegion(&stDstCopyLocation, 0, 0, 0, &stSrcCopyLocation, nullptr);
				}
			}

			GRS_SAFE_FREE(pMem);

			//ͬ��
			stResStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
			stResStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			stResStateTransBarrier.Transition.pResource = pINoiseTexture.Get();

			stGPU[c_nSecondGPU].m_pICMDList->ResourceBarrier(1, &stResStateTransBarrier);
		}
		// 11���������Կ�������Ⱦ������õľ��ο�
		{
			ST_GRS_VERTEX_QUAD stVertexQuad[] =
			{	//(   x,     y,    z,    w   )  (  u,    v   )
				{ { -1.0f,  1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } },	// Top left.
				{ {  1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },	// Top right.
				{ { -1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },	// Bottom left.
				{ {  1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },	// Bottom right.
			};

			const UINT nszVBQuad = sizeof(stVertexQuad);
			stBufferResSesc.Width = nszVBQuad;
			GRS_THROW_IF_FAILED(
				stGPU[c_nSecondGPU].m_pID3D12Device4->CreateCommittedResource(
					&stDefautHeapProps
					, D3D12_HEAP_FLAG_NONE
					, &stBufferResSesc
					, D3D12_RESOURCE_STATE_COPY_DEST
					, nullptr
					, IID_PPV_ARGS(&pIVBQuad)));

			// �������������ʾ����ν����㻺���ϴ���Ĭ�϶��ϵķ�ʽ���������ϴ�Ĭ�϶�ʵ����һ����
			stBufferResSesc.Width = nszVBQuad;
			GRS_THROW_IF_FAILED(
				stGPU[c_nSecondGPU].m_pID3D12Device4->CreateCommittedResource(
					&stUploadHeapProps
					, D3D12_HEAP_FLAG_NONE
					, &stBufferResSesc
					, D3D12_RESOURCE_STATE_GENERIC_READ
					, nullptr
					, IID_PPV_ARGS(&pIVBQuadUpload)));

			// ��һ��Copy 
			BYTE* pData = nullptr;
			GRS_THROW_IF_FAILED(pIVBQuadUpload->Map(0, nullptr, reinterpret_cast<void**>(&pData)));
			memcpy(pData, stVertexQuad, nszVBQuad);
			pIVBQuadUpload->Unmap(0, nullptr);
			// �ڶ���Copy
			stGPU[c_nSecondGPU].m_pICMDList->CopyResource(pIVBQuad.Get(), pIVBQuadUpload.Get());

			// ��Դ����ͬ��
			stResStateTransBarrier.Transition.pResource = pIVBQuad.Get();
			stResStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
			stResStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
			stGPU[c_nSecondGPU].m_pICMDList->ResourceBarrier(1, &stResStateTransBarrier);

			pstVBVQuad.BufferLocation = pIVBQuad->GetGPUVirtualAddress();
			pstVBVQuad.StrideInBytes = sizeof(ST_GRS_VERTEX_QUAD);
			pstVBVQuad.SizeInBytes = sizeof(stVertexQuad);

			// ִ����������������Լ����ζ��㻺���ϴ����ڶ����Կ���Ĭ�϶���
			GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pICMDList->Close());
			ID3D12CommandList* ppRenderCommandLists[] = { stGPU[c_nSecondGPU].m_pICMDList.Get() };
			stGPU[c_nSecondGPU].m_pICMDQueue->ExecuteCommandLists(_countof(ppRenderCommandLists), ppRenderCommandLists);

			n64CurrentFenceValue = n64FenceValue;
			GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pICMDQueue->Signal(
				stGPU[c_nSecondGPU].m_pIFence.Get()
				, n64CurrentFenceValue));

			n64FenceValue++;
			if (stGPU[c_nSecondGPU].m_pIFence->GetCompletedValue() < n64CurrentFenceValue)
			{
				GRS_THROW_IF_FAILED(
					stGPU[c_nSecondGPU].m_pIFence->SetEventOnCompletion(n64CurrentFenceValue, stGPU[c_nSecondGPU].m_hEventFence));
				WaitForSingleObject(stGPU[c_nSecondGPU].m_hEventFence, INFINITE);
			}

			//-----------------------------------�������ĸ�һ��----------------------------------------------------------
			// ���������Կ��ϵĳ���������
			stBufferResSesc.Width = szSecondPassCB; //ע�⻺��ߴ�����Ϊ256�߽�����С
			GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pID3D12Device4->CreateCommittedResource(
				&stUploadHeapProps
				, D3D12_HEAP_FLAG_NONE
				, &stBufferResSesc
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pICBResSecondPass)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICBResSecondPass);

			GRS_THROW_IF_FAILED(pICBResSecondPass->Map(0, nullptr, reinterpret_cast<void**>(&pstCBSecondPass)));

			// Create SRV Heap
			D3D12_DESCRIPTOR_HEAP_DESC stDescriptorHeapDesc = {};
			stDescriptorHeapDesc.NumDescriptors = g_nFrameBackBufCount + 2; //1 Const Bufer + 1 Texture + g_nFrameBackBufCount * Texture
			stDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			stDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pID3D12Device4->CreateDescriptorHeap(
				&stDescriptorHeapDesc
				, IID_PPV_ARGS(&stGPU[c_nSecondGPU].m_pISRVHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPU[c_nSecondGPU].m_pISRVHeap);

			D3D12_CPU_DESCRIPTOR_HANDLE stSrvHandle = stGPU[c_nSecondGPU].m_pISRVHeap->GetCPUDescriptorHandleForHeapStart();

			// Create Third Pass SRV Heap
			stDescriptorHeapDesc.NumDescriptors = g_nFrameBackBufCount;
			//Ϊÿһ����ȾĿ��������һ��SRV������ֻ��Ҫ����ʱ���˵���ӦSRV����
			for (int i = 0; i < c_nPostPassCnt; i++)
			{
				GRS_THROW_IF_FAILED(
					stGPU[c_nSecondGPU].m_pID3D12Device4->CreateDescriptorHeap(
						&stDescriptorHeapDesc, IID_PPV_ARGS(&pISRVHeapPostPass[i])));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(pISRVHeapPostPass[i]);
			}

			//-------------------------------------------------------------------------------------------------------
			//Create Cross Adapter Texture SRV
			D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
			stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			stSRVDesc.Format = emRTFormat;
			stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			stSRVDesc.Texture2D.MipLevels = 1;

			for (int i = 0; i < g_nFrameBackBufCount; i++)
			{
				if (!bCrossAdapterTextureSupport)
				{
					// ʹ�ø��Ƶ���ȾĿ��������ΪShader��������Դ
					stGPU[c_nSecondGPU].m_pID3D12Device4->CreateShaderResourceView(
						pISecondaryAdapterTexutrePerFrame[i].Get()
						, &stSRVDesc
						, stSrvHandle);
				}
				else
				{
					// ֱ��ʹ�ù������ȾĿ�긴�Ƶ�������Ϊ������Դ
					stGPU[c_nSecondGPU].m_pID3D12Device4->CreateShaderResourceView(
						stGPU[c_nSecondGPU].m_pICrossAdapterResPerFrame[i].Get()
						, &stSRVDesc
						, stSrvHandle);
				}

				stSrvHandle.ptr += stGPU[c_nSecondGPU].m_nSRVDescriptorSize;
			}

			//Create CBV
			D3D12_CONSTANT_BUFFER_VIEW_DESC stCBVDesc = {};
			stCBVDesc.BufferLocation = pICBResSecondPass->GetGPUVirtualAddress();
			stCBVDesc.SizeInBytes = static_cast<UINT>(szSecondPassCB);

			stGPU[c_nSecondGPU].m_pID3D12Device4->CreateConstantBufferView(&stCBVDesc, stSrvHandle);

			stSrvHandle.ptr += stGPU[c_nSecondGPU].m_nSRVDescriptorSize;

			// Create Noise SRV
			D3D12_RESOURCE_DESC stTXDesc = pINoiseTexture->GetDesc();
			stSRVDesc.Format = stTXDesc.Format;
			//������������������
			stGPU[c_nSecondGPU].m_pID3D12Device4->CreateShaderResourceView(pINoiseTexture.Get(), &stSRVDesc, stSrvHandle);

			//-------------------------------------------------------------------------------------------------------
			// Create Third Pass SRV
			stSRVDesc.Format = emRTFormat;
			for (int k = 0; k < c_nPostPassCnt; k++)
			{
				D3D12_CPU_DESCRIPTOR_HANDLE stThirdPassSrvHandle = pISRVHeapPostPass[k]->GetCPUDescriptorHandleForHeapStart();
				for (int i = 0; i < g_nFrameBackBufCount; i++)
				{
					stGPU[c_nSecondGPU].m_pID3D12Device4->CreateShaderResourceView(
						pIOffLineRTRes[k][i].Get()
						, &stSRVDesc
						, stThirdPassSrvHandle);
					stThirdPassSrvHandle.ptr += stGPU[c_nSecondGPU].m_nSRVDescriptorSize;
				}
			}
			//-------------------------------------------------------------------------------------------------------

			// ����Sample Descriptor Heap ��������������
			stDescriptorHeapDesc.NumDescriptors = 1;  //ֻ��һ��Sample
			stDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			GRS_THROW_IF_FAILED(
				stGPU[c_nSecondGPU].m_pID3D12Device4->CreateDescriptorHeap(
					&stDescriptorHeapDesc, IID_PPV_ARGS(&stGPU[c_nSecondGPU].m_pISampleHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPU[c_nSecondGPU].m_pISampleHeap);

			// ����Sample View ʵ�ʾ��ǲ�����
			D3D12_SAMPLER_DESC stSamplerDesc = {};
			stSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			stSamplerDesc.MinLOD = 0;
			stSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
			stSamplerDesc.MipLODBias = 0.0f;
			stSamplerDesc.MaxAnisotropy = 1;
			stSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			stSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			stSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			stSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

			stGPU[c_nSecondGPU].m_pID3D12Device4->CreateSampler(
				&stSamplerDesc
				, stGPU[c_nSecondGPU].m_pISampleHeap->GetCPUDescriptorHandleForHeapStart());

			//-------------------------------------------------------------------------------------------------------
			// Create Third Pass Sample Heap
			GRS_THROW_IF_FAILED(
				stGPU[c_nSecondGPU].m_pID3D12Device4->CreateDescriptorHeap(
					&stDescriptorHeapDesc, IID_PPV_ARGS(&pISampleHeapPostPass)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pISampleHeapPostPass);

			// Create Third Pass Sample
			stGPU[c_nSecondGPU].m_pID3D12Device4->CreateSampler(
				&stSamplerDesc
				, pISampleHeapPostPass->GetCPUDescriptorHandleForHeapStart());
		}

		//��ʾ���ڣ�׼����ʼ��Ϣѭ����Ⱦ
		ShowWindow(hWnd, nCmdShow);
		UpdateWindow(hWnd);

		UINT nStates = 0; //��ʼ״̬Ϊ0
		DWORD dwRet = 0;
		CAtlArray<ID3D12CommandList*> arCmdList;
		CAtlArray<ID3D12DescriptorHeap*> arDesHeaps;
		BOOL bExit = FALSE;

		ULONGLONG n64tmFrameStart = ::GetTickCount64();
		ULONGLONG n64tmCurrent = n64tmFrameStart;
		//������ת�Ƕ���Ҫ�ı���
		double dModelRotationYAngle = 0.0f;

		//��ʼ��ʱ��ر�һ�����������б���Ϊ�����ڿ�ʼ��Ⱦ��ʱ����Ҫ��reset���ǣ�Ϊ�˷�ֹ�������Close
		GRS_THROW_IF_FAILED(pICMDListBefore->Close());
		GRS_THROW_IF_FAILED(pICMDListLater->Close());
		GRS_THROW_IF_FAILED(pICMDListCopy->Close());
		GRS_THROW_IF_FAILED(pICMDListPostPass->Close());

		SetTimer(hWnd, WM_USER + 100, 1, nullptr); //���Ϊ�˱�֤MsgWaitForMultipleObjects �� wait for all ΪTrueʱ �ܹ���ʱ����

		//��ʼ��Ϣѭ�����������в�����Ⱦ
		while (!bExit)
		{
			//���߳̽���ȴ�
			dwRet = ::MsgWaitForMultipleObjects(static_cast<DWORD>(arHWaited.GetCount()), arHWaited.GetData(), TRUE, INFINITE, QS_ALLINPUT);
			dwRet -= WAIT_OBJECT_0;

			if (0 == dwRet)
			{//��ʾ���ȴ����¼�����Ϣ������֪ͨ��״̬�ˣ��ȴ�����Ⱦ��Ȼ������Ϣ
				switch (nStates)
				{
				case 0://״̬0����ʾ�ȵ������̼߳�����Դ��ϣ���ʱִ��һ�������б���ɸ����߳�Ҫ�����Դ�ϴ��ĵڶ���Copy����
				{// ��״ֻ̬��ִ��һ��

					arCmdList.RemoveAll();
					//ִ�и��̴߳��������Կ��������б�
					arCmdList.Add(g_stThread[g_nThdSphere].m_pICMDList);
					arCmdList.Add(g_stThread[g_nThdCube].m_pICMDList);
					arCmdList.Add(g_stThread[g_nThdPlane].m_pICMDList);

					stGPU[c_nMainGPU].m_pICMDQueue->ExecuteCommandLists(static_cast<UINT>(arCmdList.GetCount()), arCmdList.GetData());

					//---------------------------------------------------------------------------------------------
					//��ʼͬ��GPU��CPU��ִ�У��ȼ�¼Χ�����ֵ
					n64CurrentFenceValue = n64FenceValue;
					GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pICMDQueue->Signal(stGPU[c_nMainGPU].m_pIFence.Get(), n64CurrentFenceValue));
					GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pIFence->SetEventOnCompletion(n64CurrentFenceValue, stGPU[c_nMainGPU].m_hEventFence));

					++n64FenceValue;

					nStates = 1;

					arHWaited.RemoveAll();
					arHWaited.Add(stGPU[c_nMainGPU].m_hEventFence);
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
						//���� �Ӿ��� view * �ü����� projection
						XMStoreFloat4x4(&g_mxVP
							, XMMatrixMultiply(XMMatrixLookAtLH(XMLoadFloat3(&g_f3EyePos)
								, XMLoadFloat3(&g_f3LockAt)
								, XMLoadFloat3(&g_f3HeapUp))
								, XMMatrixPerspectiveFovLH(XM_PIDIV4
									, (FLOAT)g_iWndWidth / (FLOAT)g_iWndHeight, 0.1f, 1000.0f)));
					}

					//���µڶ�����Ⱦ�ĳ��������������Է���OnUpdate��
					{
						pstCBSecondPass->m_nFun = g_nFunNO;
						pstCBSecondPass->m_fQuatLevel = g_fQuatLevel;
						pstCBSecondPass->m_fWaterPower = g_fWaterPower;
					}

					//OnRender()
					{
						GRS_THROW_IF_FAILED(pICMDAllocBefore->Reset());
						GRS_THROW_IF_FAILED(pICMDListBefore->Reset(pICMDAllocBefore.Get(), stGPU[c_nMainGPU].m_pIPSO.Get()));

						GRS_THROW_IF_FAILED(pICMDAllocLater->Reset());
						GRS_THROW_IF_FAILED(pICMDListLater->Reset(pICMDAllocLater.Get(), stGPU[c_nMainGPU].m_pIPSO.Get()));

						nCurrentFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

						//��Ⱦǰ����
						{
							stResStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
							stResStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
							stResStateTransBarrier.Transition.pResource = stGPU[c_nMainGPU].m_pIRTRes[nCurrentFrameIndex].Get();

							pICMDListBefore->ResourceBarrier(1, &stResStateTransBarrier);

							//ƫ��������ָ�뵽ָ��֡������ͼλ��
							D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle = stGPU[c_nMainGPU].m_pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
							stRTVHandle.ptr += (nCurrentFrameIndex * stGPU[c_nMainGPU].m_nRTVDescriptorSize);
							D3D12_CPU_DESCRIPTOR_HANDLE stDSVHandle = stGPU[c_nMainGPU].m_pIDSVHeap->GetCPUDescriptorHandleForHeapStart();

							//������ȾĿ��
							pICMDListBefore->OMSetRenderTargets(1, &stRTVHandle, FALSE, &stDSVHandle);

							pICMDListBefore->RSSetViewports(1, &g_stViewPort);
							pICMDListBefore->RSSetScissorRects(1, &g_stScissorRect);

							pICMDListBefore->ClearRenderTargetView(stRTVHandle, faClearColor, 0, nullptr);
							pICMDListBefore->ClearDepthStencilView(stDSVHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
						}

						nStates = 2;

						arHWaited.RemoveAll();
						arHWaited.Add(g_stThread[g_nThdSphere].m_hEventRenderOver);
						arHWaited.Add(g_stThread[g_nThdCube].m_hEventRenderOver);
						arHWaited.Add(g_stThread[g_nThdPlane].m_hEventRenderOver);

						//֪ͨ���߳̿�ʼ��Ⱦ
						//ע�⣺��ʱ��ֵ���ݵ�������Ⱦ�̣߳��������߳�ʱ��ڵ�Ϊ׼������������ʱ�����ͬ����
						for (int i = 0; i < g_nMaxThread; i++)
						{
							g_stThread[i].m_nCurrentFrameIndex = nCurrentFrameIndex;
							g_stThread[i].m_nStartTime = n64tmFrameStart;
							g_stThread[i].m_nCurrentTime = n64tmCurrent;

							SetEvent(g_stThread[i].m_hEventRun);
						}

					}
				}
				break;
				case 2:// ״̬2 ��ʾ���е���Ⱦ�����б���¼����ˣ���ʼ�����ִ�������б�
				{
					// 1 First Pass
					{
						stResStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
						stResStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
						stResStateTransBarrier.Transition.pResource = stGPU[c_nMainGPU].m_pIRTRes[nCurrentFrameIndex].Get();

						pICMDListLater->ResourceBarrier(1, &stResStateTransBarrier);

						//�ر������б�����ȥִ����
						GRS_THROW_IF_FAILED(pICMDListBefore->Close());
						GRS_THROW_IF_FAILED(pICMDListLater->Close());

						arCmdList.RemoveAll();
						//ִ�������б�ע�������б��Ŷ���ϵķ�ʽ��
						arCmdList.Add(pICMDListBefore.Get());
						arCmdList.Add(g_stThread[g_nThdSphere].m_pICMDList);
						arCmdList.Add(g_stThread[g_nThdCube].m_pICMDList);
						arCmdList.Add(g_stThread[g_nThdPlane].m_pICMDList);
						arCmdList.Add(pICMDListLater.Get());

						stGPU[c_nMainGPU].m_pICMDQueue->ExecuteCommandLists(static_cast<UINT>(arCmdList.GetCount()), arCmdList.GetData());

						//���Կ�ͬ��
						n64CurrentFenceValue = n64FenceValue;

						GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pICMDQueue->Signal(stGPU[c_nMainGPU].m_pIFence.Get(), n64CurrentFenceValue));

						n64FenceValue++;
					}

					// Copy Main GPU Render Target Texture To Cross Adapter Heap Resource 
					// ��¼���Կ�����Ⱦ������Ƶ�����������Դ�е������б�
					{
						pICMDAllocCopy->Reset();
						pICMDListCopy->Reset(pICMDAllocCopy.Get(), nullptr);

						if (bCrossAdapterTextureSupport)
						{
							// ���������֧�ֿ���������������ֻ�轫�������Ƶ��������������С�
							pICMDListCopy->CopyResource(
								stGPU[c_nMainGPU].m_pICrossAdapterResPerFrame[nCurrentFrameIndex].Get()
								, stGPU[c_nMainGPU].m_pIRTRes[nCurrentFrameIndex].Get());
						}
						else
						{
							// �����������֧�ֿ�����������������������Ϊ��������
							// �Ա������ʽ�ع��������м�ࡣʹ�ó���Ŀ��ָ�����ڴ沼�ֽ��м����Ŀ�긴�Ƶ����������С�
							// ���൱�ڴ�Texture�������ݵ�Buffer�У�Ҳ��һ������Ҫ�ļ���
							D3D12_RESOURCE_DESC stRenderTargetDesc = stGPU[c_nMainGPU].m_pIRTRes[nCurrentFrameIndex]->GetDesc();
							D3D12_PLACED_SUBRESOURCE_FOOTPRINT stRenderTargetLayout = {};

							stGPU[c_nMainGPU].m_pID3D12Device4->GetCopyableFootprints(
								&stRenderTargetDesc, 0, 1, 0, &stRenderTargetLayout, nullptr, nullptr, nullptr);

							D3D12_TEXTURE_COPY_LOCATION stDstCopyLocation = {};
							stDstCopyLocation.pResource = stGPU[c_nMainGPU].m_pICrossAdapterResPerFrame[nCurrentFrameIndex].Get();
							stDstCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
							stDstCopyLocation.PlacedFootprint = stRenderTargetLayout;

							D3D12_TEXTURE_COPY_LOCATION stSrcCopyLocation = {};
							stSrcCopyLocation.pResource = stGPU[c_nMainGPU].m_pIRTRes[nCurrentFrameIndex].Get();
							stSrcCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
							stSrcCopyLocation.SubresourceIndex = 0;

							D3D12_BOX stCopyBox = { 0, 0, (UINT)g_iWndWidth, (UINT)g_iWndHeight };

							pICMDListCopy->CopyTextureRegion(&stDstCopyLocation, 0, 0, 0, &stSrcCopyLocation, &stCopyBox);
						}

						GRS_THROW_IF_FAILED(pICMDListCopy->Close());

						// ʹ�����Կ��ϵĸ���������������ȾĿ����Դ��������Դ��ĸ���
						// ͨ������������е�Wait����ʵ��ͬһ��GPU�ϵĸ��������֮��ĵȴ�ͬ�� 
						GRS_THROW_IF_FAILED(pICmdQueueCopy->Wait(stGPU[c_nMainGPU].m_pIFence.Get(), n64CurrentFenceValue));

						arCmdList.RemoveAll();
						arCmdList.Add(pICMDListCopy.Get());
						pICmdQueueCopy->ExecuteCommandLists(static_cast<UINT>(arCmdList.GetCount()), arCmdList.GetData());

						n64CurrentFenceValue = n64FenceValue;
						// ����������ź������ڹ����Χ������������ʹ�õڶ����Կ���������п��������Χ���ϵȴ�
						// �Ӷ���ɲ�ͬGPU������м��ͬ���ȴ�
						GRS_THROW_IF_FAILED(pICmdQueueCopy->Signal(stGPU[c_nMainGPU].m_pISharedFence.Get(), n64CurrentFenceValue));
						n64FenceValue++;
					}

					// 2 Second Pass
					// ��ʼ�����Կ�����Ⱦ��ͨ���Ǻ��������˶�ģ���ȣ���������ֱ�Ӿ��ǰѻ�����Ƴ���
					{
						// ʹ�ø����Կ��ϵ����������ִ�������б�
						// �����Կ��ϵ����������ͨ���ȴ������Χ��������������������Կ�֮���ͬ��
						GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pICMDQueue->Wait(stGPU[c_nSecondGPU].m_pISharedFence.Get(), n64CurrentFenceValue));

						stGPU[c_nSecondGPU].m_pICMDAlloc->Reset();
						stGPU[c_nSecondGPU].m_pICMDList->Reset(stGPU[c_nSecondGPU].m_pICMDAlloc.Get(), stGPU[c_nSecondGPU].m_pIPSO.Get());

						if (!bCrossAdapterTextureSupport)
						{//�����ܿ��Կ���������ʱ��Ҫִ�ж�һ��COPY���൱�ھ��������COPY�еĵڶ���COPY
							// ��������еĻ��������Ƶ��������������Դ���ȡ���������С�
							stResStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
							stResStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
							stResStateTransBarrier.Transition.pResource = pISecondaryAdapterTexutrePerFrame[nCurrentFrameIndex].Get();

							stGPU[c_nSecondGPU].m_pICMDList->ResourceBarrier(1, &stResStateTransBarrier);

							D3D12_RESOURCE_DESC stSecondaryAdapterTexture
								= pISecondaryAdapterTexutrePerFrame[nCurrentFrameIndex]->GetDesc();
							D3D12_PLACED_SUBRESOURCE_FOOTPRINT stTextureLayout = {};

							stGPU[c_nSecondGPU].m_pID3D12Device4->GetCopyableFootprints(
								&stSecondaryAdapterTexture, 0, 1, 0, &stTextureLayout, nullptr, nullptr, nullptr);

							D3D12_TEXTURE_COPY_LOCATION stDstCopyLocation = {};
							stDstCopyLocation.pResource = pISecondaryAdapterTexutrePerFrame[nCurrentFrameIndex].Get();
							stDstCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
							stDstCopyLocation.SubresourceIndex = 0;

							D3D12_TEXTURE_COPY_LOCATION stSrcCopyLocation = {};
							stSrcCopyLocation.pResource = stGPU[c_nMainGPU].m_pIRTRes[nCurrentFrameIndex].Get();
							stSrcCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
							stSrcCopyLocation.PlacedFootprint = stTextureLayout;

							D3D12_BOX stCopyBox = { 0, 0, (UINT)g_iWndWidth, (UINT)g_iWndHeight };

							//���Copy�����������Ϊһ�������Upload���ϴ���Default��
							//���Կ���֧�ֿ��Կ���������ʱ��������ƶ����Ϳ���ʡȴ�ˣ����������Ч��
							//ĿǰԽ��Խ�߼����Կ�������֧�ֿ��Կ������������Ծ�Խ��Ч
							//���������ڵĸ����Կ���Intel UHD G630��֧�ֿ��Կ����������Ͳ������������ĸ��ƶ���
							stGPU[c_nSecondGPU].m_pICMDList->CopyTextureRegion(&stDstCopyLocation, 0, 0, 0, &stSrcCopyLocation, &stCopyBox);

							stResStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
							stResStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
							stResStateTransBarrier.Transition.pResource = pISecondaryAdapterTexutrePerFrame[nCurrentFrameIndex].Get();

							stGPU[c_nSecondGPU].m_pICMDList->ResourceBarrier(1, &stResStateTransBarrier);
						}

						//------------------------------------------------------------------------------------------------------
						// Second Pass ʹ�ø����Կ���ʼ������Ҳ����ͼ������ʱ�����˾��Ϳ��Դ���������
						stGPU[c_nSecondGPU].m_pICMDList->SetGraphicsRootSignature(stGPU[c_nSecondGPU].m_pIRS.Get());
						stGPU[c_nSecondGPU].m_pICMDList->SetPipelineState(stGPU[c_nSecondGPU].m_pIPSO.Get());

						arDesHeaps.RemoveAll();
						arDesHeaps.Add(stGPU[c_nSecondGPU].m_pISRVHeap.Get());
						arDesHeaps.Add(stGPU[c_nSecondGPU].m_pISampleHeap.Get());
						stGPU[c_nSecondGPU].m_pICMDList->SetDescriptorHeaps(static_cast<UINT>(arDesHeaps.GetCount()), arDesHeaps.GetData());

						stGPU[c_nSecondGPU].m_pICMDList->RSSetViewports(1, &g_stViewPort);
						stGPU[c_nSecondGPU].m_pICMDList->RSSetScissorRects(1, &g_stScissorRect);

						if (0 == g_nUsePSID)
						{//û�и�˹ģ���������ֱ�����
							stResStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
							stResStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
							stResStateTransBarrier.Transition.pResource = stGPU[c_nSecondGPU].m_pIRTRes[nCurrentFrameIndex].Get();

							stGPU[c_nSecondGPU].m_pICMDList->ResourceBarrier(1, &stResStateTransBarrier);

							D3D12_CPU_DESCRIPTOR_HANDLE stRTV4Handle = stGPU[c_nSecondGPU].m_pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
							stRTV4Handle.ptr += (nCurrentFrameIndex * stGPU[c_nSecondGPU].m_nRTVDescriptorSize);

							stGPU[c_nSecondGPU].m_pICMDList->OMSetRenderTargets(1, &stRTV4Handle, false, nullptr);
							float f4ClearColor[] = { 1.0f,0.0f,1.0f,1.0f }; //����ʹ�ò�ͬ�����ɫ���鿴�Ƿ��С�¶�ס�������
							stGPU[c_nSecondGPU].m_pICMDList->ClearRenderTargetView(stRTV4Handle, f4ClearColor, 0, nullptr);
						}
						else
						{
							stResStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
							stResStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
							stResStateTransBarrier.Transition.pResource = pIOffLineRTRes[c_nPostPass0][nCurrentFrameIndex].Get();
							stGPU[c_nSecondGPU].m_pICMDList->ResourceBarrier(1, &stResStateTransBarrier);

							D3D12_CPU_DESCRIPTOR_HANDLE stRTVHadnleSecondPass = pIRTVOffLine[c_nPostPass0]->GetCPUDescriptorHandleForHeapStart();
							stRTVHadnleSecondPass.ptr += ( nCurrentFrameIndex * stGPU[c_nSecondGPU].m_nRTVDescriptorSize);

							stGPU[c_nSecondGPU].m_pICMDList->OMSetRenderTargets(1, &stRTVHadnleSecondPass, false, nullptr);
							stGPU[c_nSecondGPU].m_pICMDList->ClearRenderTargetView(stRTVHadnleSecondPass, v4ClearColor, 0, nullptr);
						}


						// CBV
						D3D12_GPU_DESCRIPTOR_HANDLE stCBVGPUHandle = stGPU[c_nSecondGPU].m_pISRVHeap->GetGPUDescriptorHandleForHeapStart();
						stCBVGPUHandle.ptr += (g_nFrameBackBufCount * stGPU[c_nSecondGPU].m_nSRVDescriptorSize);

						stGPU[c_nSecondGPU].m_pICMDList->SetGraphicsRootDescriptorTable(0, stCBVGPUHandle);

						// Render Target to Texture SRV
						D3D12_GPU_DESCRIPTOR_HANDLE stSRVGPUHandle = stGPU[c_nSecondGPU].m_pISRVHeap->GetGPUDescriptorHandleForHeapStart();
						stSRVGPUHandle.ptr += ( nCurrentFrameIndex * stGPU[c_nSecondGPU].m_nSRVDescriptorSize);

						stGPU[c_nSecondGPU].m_pICMDList->SetGraphicsRootDescriptorTable(1, stSRVGPUHandle);

						// Noise Texture SRV
						D3D12_GPU_DESCRIPTOR_HANDLE stNoiseSRVGPUHandle = stGPU[c_nSecondGPU].m_pISRVHeap->GetGPUDescriptorHandleForHeapStart();
						stNoiseSRVGPUHandle.ptr += ( (g_nFrameBackBufCount + 1) * stGPU[c_nSecondGPU].m_nSRVDescriptorSize );

						stGPU[c_nSecondGPU].m_pICMDList->SetGraphicsRootDescriptorTable(2, stNoiseSRVGPUHandle);

						stGPU[c_nSecondGPU].m_pICMDList->SetGraphicsRootDescriptorTable(3
							, stGPU[c_nSecondGPU].m_pISampleHeap->GetGPUDescriptorHandleForHeapStart());

						// ��ʼ���ƾ���
						stGPU[c_nSecondGPU].m_pICMDList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
						stGPU[c_nSecondGPU].m_pICMDList->IASetVertexBuffers(0, 1, &pstVBVQuad);
						// Draw Call!
						stGPU[c_nSecondGPU].m_pICMDList->DrawInstanced(4, 1, 0, 0);

						if (0 == g_nUsePSID)
						{// û�и�˹ģ���������ֱ�����
							stResStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
							stResStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
							stResStateTransBarrier.Transition.pResource = stGPU[c_nSecondGPU].m_pIRTRes[nCurrentFrameIndex].Get();

							stGPU[c_nSecondGPU].m_pICMDList->ResourceBarrier(1, &stResStateTransBarrier);
						}
						else
						{
							// �ڶ����Կ��ϵĵ�һ����ȾĿ��������״̬�л�
							stResStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
							stResStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
							stResStateTransBarrier.Transition.pResource = pIOffLineRTRes[c_nPostPass0][nCurrentFrameIndex].Get();

							stGPU[c_nSecondGPU].m_pICMDList->ResourceBarrier(1, &stResStateTransBarrier);
						}
						GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pICMDList->Close());

					}
					// ���ø�˹ģ��Ч�������� Post Pass��
					if (1 == g_nUsePSID)
					{
						pICMDAllocPostPass->Reset();
						pICMDListPostPass->Reset(pICMDAllocPostPass.Get(), pIPSOPostPass[c_nPostPass0].Get());

						//��������ͳһ���ú󣬾Ͳ����ˣ���Ϊ����������Ⱦ��һ������
						pICMDListPostPass->SetGraphicsRootSignature(pIRSPostPass.Get());
						pICMDListPostPass->RSSetViewports(1, &g_stViewPort);
						pICMDListPostPass->RSSetScissorRects(1, &g_stScissorRect);
						pICMDListPostPass->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
						pICMDListPostPass->IASetVertexBuffers(0, 1, &pstVBVQuad);

						//-------------------------------------------------------------------------------------------------------
						// 3 Thrid Pass
						//ʹ�ø�˹ģ��
						pICMDListPostPass->SetPipelineState(pIPSOPostPass[c_nPostPass0].Get());

						arDesHeaps.RemoveAll();
						arDesHeaps.Add(pISRVHeapPostPass[c_nPostPass0].Get());
						arDesHeaps.Add(pISampleHeapPostPass.Get());

						pICMDListPostPass->SetDescriptorHeaps(static_cast<UINT>(arDesHeaps.GetCount()), arDesHeaps.GetData());

						stResStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
						stResStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
						stResStateTransBarrier.Transition.pResource = pIOffLineRTRes[c_nPostPass1][nCurrentFrameIndex].Get();

						pICMDListPostPass->ResourceBarrier(1, &stResStateTransBarrier);

						D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandleThirdPass = pIRTVOffLine[c_nPostPass1]->GetCPUDescriptorHandleForHeapStart();
						stRTVHandleThirdPass.ptr += ( nCurrentFrameIndex * stGPU[c_nSecondGPU].m_nRTVDescriptorSize);
						pICMDListPostPass->OMSetRenderTargets(1, &stRTVHandleThirdPass, false, nullptr);
						pICMDListPostPass->ClearRenderTargetView(stRTVHandleThirdPass, v4ClearColor, 0, nullptr);

						//���ڶ�����Ⱦ�Ľ����Ϊ����
						D3D12_GPU_DESCRIPTOR_HANDLE stSRV3Handle = pISRVHeapPostPass[c_nPostPass0]->GetGPUDescriptorHandleForHeapStart();
						stSRV3Handle.ptr += ( nCurrentFrameIndex * stGPU[c_nSecondGPU].m_nSRVDescriptorSize);

						pICMDListPostPass->SetGraphicsRootDescriptorTable(0, stSRV3Handle);
						pICMDListPostPass->SetGraphicsRootDescriptorTable(1, pISampleHeapPostPass->GetGPUDescriptorHandleForHeapStart());

						// Draw Call!
						pICMDListPostPass->DrawInstanced(4, 1, 0, 0);

						// ���ú�ͬ��Χ��
						stResStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
						stResStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
						stResStateTransBarrier.Transition.pResource = pIOffLineRTRes[c_nPostPass1][nCurrentFrameIndex].Get();
						pICMDListPostPass->ResourceBarrier(1, &stResStateTransBarrier);

						//-------------------------------------------------------------------------------------------------------	
						// 4 Fourth Pass
						//��˹ģ��
						pICMDListPostPass->SetPipelineState(pIPSOPostPass[c_nPostPass1].Get());

						arDesHeaps.RemoveAll();
						arDesHeaps.Add(pISRVHeapPostPass[c_nPostPass1].Get());
						arDesHeaps.Add(pISampleHeapPostPass.Get());

						pICMDListPostPass->SetDescriptorHeaps(static_cast<UINT>(arDesHeaps.GetCount()), arDesHeaps.GetData());

						stResStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
						stResStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
						stResStateTransBarrier.Transition.pResource = stGPU[c_nSecondGPU].m_pIRTRes[nCurrentFrameIndex].Get();

						pICMDListPostPass->ResourceBarrier(1, &stResStateTransBarrier);

						D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle = stGPU[c_nSecondGPU].m_pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
						stRTVHandle.ptr += (nCurrentFrameIndex * stGPU[c_nSecondGPU].m_nRTVDescriptorSize);
						pICMDListPostPass->OMSetRenderTargets(1, &stRTVHandle, false, nullptr);

						float f4ClearColor[] = { 1.0f,0.0f,1.0f,1.0f }; //����ʹ�ò�ͬ�����ɫ���鿴�Ƿ��С�¶�ס�������
						pICMDListPostPass->ClearRenderTargetView(stRTVHandle, f4ClearColor, 0, nullptr);

						//����������Ⱦ�Ľ����Ϊ����
						D3D12_GPU_DESCRIPTOR_HANDLE stSRV4Handle = pISRVHeapPostPass[c_nPostPass1]->GetGPUDescriptorHandleForHeapStart();
						stSRV4Handle.ptr += (nCurrentFrameIndex * stGPU[c_nSecondGPU].m_nSRVDescriptorSize);

						pICMDListPostPass->SetGraphicsRootDescriptorTable(0, stSRV4Handle);
						pICMDListPostPass->SetGraphicsRootDescriptorTable(1, pISampleHeapPostPass->GetGPUDescriptorHandleForHeapStart());
						// Draw Call!
						pICMDListPostPass->DrawInstanced(4, 1, 0, 0);

						// ���ú�ͬ��Χ��
						stResStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
						stResStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
						stResStateTransBarrier.Transition.pResource = stGPU[c_nSecondGPU].m_pIRTRes[nCurrentFrameIndex].Get();

						pICMDListPostPass->ResourceBarrier(1, &stResStateTransBarrier);

						GRS_THROW_IF_FAILED(pICMDListPostPass->Close());
					}

					arCmdList.RemoveAll();
					arCmdList.Add(stGPU[c_nSecondGPU].m_pICMDList.Get());

					if (1 == g_nUsePSID)
					{
						arCmdList.Add(pICMDListPostPass.Get());
					}

					stGPU[c_nSecondGPU].m_pICMDQueue->ExecuteCommandLists(static_cast<UINT>(arCmdList.GetCount()), arCmdList.GetData());

					// ִ��Present�������ճ��ֻ���
					GRS_THROW_IF_FAILED(pISwapChain3->Present(1, 0));

					n64CurrentFenceValue = n64FenceValue;
					GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pICMDQueue->Signal(stGPU[c_nSecondGPU].m_pIFence.Get(), n64CurrentFenceValue));
					GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pIFence->SetEventOnCompletion(n64CurrentFenceValue, stGPU[c_nSecondGPU].m_hEventFence));

					n64FenceValue++;

					// ״̬������1 ��ʼ��һ����Ⱦ
					nStates = 1;

					// ��ʼ��Ⱦ��ֻ��Ҫ�ڸ����Կ����¼�����ϵȴ�����
					// ��Ϊ�����Կ��Ѿ���GPU�������Կ�ͨ��Wait���������ͬ��
					arHWaited.RemoveAll();
					arHWaited.Add(stGPU[c_nSecondGPU].m_hEventFence);

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
			{//�����������˳�
				bExit = TRUE;
			}

			//���һ���̵߳Ļ�����������߳��Ѿ��˳��ˣ����˳�ѭ��
			dwRet = WaitForMultipleObjects(static_cast<DWORD>(arHSubThread.GetCount()), arHSubThread.GetData(), FALSE, 0);
			dwRet -= WAIT_OBJECT_0;
			if (dwRet >= 0 && dwRet < g_nMaxThread)
			{//���̵߳ľ���Ѿ���Ϊ���ź�״̬������Ӧ�߳��Ѿ��˳�����˵�������˴���
				bExit = TRUE;
			}
		}

		::KillTimer(hWnd, WM_USER + 100);

	}
	catch (CGRSCOMException& e)
	{//������COM�쳣
		e;
	}

	try
	{
		// ֪ͨ���߳��˳�
		for (int i = 0; i < g_nMaxThread; i++)
		{
			::PostThreadMessage(g_stThread[i].m_dwThisThreadID, WM_QUIT, 0, 0);
		}

		// �ȴ��������߳��˳�
		DWORD dwRet = WaitForMultipleObjects(static_cast<DWORD>(arHSubThread.GetCount()), arHSubThread.GetData(), TRUE, INFINITE);
		// �����������߳���Դ
		for (int i = 0; i < g_nMaxThread; i++)
		{
			::CloseHandle(g_stThread[i].m_hThisThread);
			::CloseHandle(g_stThread[i].m_hEventRenderOver);
			GRS_SAFE_RELEASE(g_stThread[i].m_pICMDList);
			GRS_SAFE_RELEASE(g_stThread[i].m_pICMDAlloc);
		}
	}
	catch (CGRSCOMException& e)
	{//�������쳣
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
		if (nullptr == pThdPms)
		{//�����쳣�����쳣��ֹ�߳�
			throw CGRSCOMException(E_INVALIDARG);
		}

		SIZE_T								szMVPBuf = GRS_UPPER(sizeof(ST_GRS_MVP), 256);
		SIZE_T								szPerObjDataBuf = GRS_UPPER(sizeof(ST_GRS_PEROBJECT_CB), 256);

		ComPtr<ID3D12Resource>				pITexture;
		ComPtr<ID3D12Resource>				pITextureUpload;
		ComPtr<ID3D12Resource>				pIVB;
		ComPtr<ID3D12Resource>				pIIB;
		ComPtr<ID3D12Resource>			    pICBWVP;
		ComPtr<ID3D12Resource>				pICBPerObjData;
		ComPtr<ID3D12DescriptorHeap>		pISRVHeap;
		ComPtr<ID3D12DescriptorHeap>		pISampleHeap;

		ST_GRS_MVP* pMVPBufModule = nullptr;
		ST_GRS_PEROBJECT_CB* pPerObjBuf = nullptr;
		D3D12_VERTEX_BUFFER_VIEW			stVBV = {};
		D3D12_INDEX_BUFFER_VIEW				stIBV = {};
		UINT								nIndexCnt = 0;
		XMMATRIX							mxPosModule = XMMatrixTranslationFromVector(XMLoadFloat4(&pThdPms->m_v4ModelPos));  //��ǰ��Ⱦ�����λ��
		// Mesh Value
		ST_GRS_VERTEX* pstVertices = nullptr;
		UINT* pnIndices = nullptr;
		UINT								nVertexCnt = 0;
		// DDS Value
		std::unique_ptr<uint8_t[]>			pbDDSData;
		std::vector<D3D12_SUBRESOURCE_DATA> stArSubResources;
		DDS_ALPHA_MODE						emAlphaMode = DDS_ALPHA_MODE_UNKNOWN;
		bool								bIsCube = false;

		UINT								nSRVDescriptorSize = 0;
		UINT								nRTVDescriptorSize = 0;

		D3D12_HEAP_PROPERTIES stDefautHeapProps = {};
		stDefautHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
		stDefautHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		stDefautHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		stDefautHeapProps.CreationNodeMask = 0;
		stDefautHeapProps.VisibleNodeMask = 0;

		D3D12_HEAP_PROPERTIES stUploadHeapProps = {};
		stUploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
		stUploadHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		stUploadHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		stUploadHeapProps.CreationNodeMask = 0;
		stUploadHeapProps.VisibleNodeMask = 0;

		D3D12_RESOURCE_DESC stBufferResSesc = {};
		stBufferResSesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		stBufferResSesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		stBufferResSesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		stBufferResSesc.Format = DXGI_FORMAT_UNKNOWN;
		stBufferResSesc.Width = 0;
		stBufferResSesc.Height = 1;
		stBufferResSesc.DepthOrArraySize = 1;
		stBufferResSesc.MipLevels = 1;
		stBufferResSesc.SampleDesc.Count = 1;
		stBufferResSesc.SampleDesc.Quality = 0;

		D3D12_RESOURCE_BARRIER stResStateTransBarrier = {};
		stResStateTransBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		stResStateTransBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		stResStateTransBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		//1������DDS����
		{
			nRTVDescriptorSize = pThdPms->m_pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			nSRVDescriptorSize = pThdPms->m_pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			GRS_THROW_IF_FAILED(LoadDDSTextureFromFile(
				pThdPms->m_pID3D12Device4
				, pThdPms->m_pszDDSFile
				, pITexture.GetAddressOf()
				, pbDDSData
				, stArSubResources
				, SIZE_MAX
				, &emAlphaMode
				, &bIsCube));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pITexture);

			D3D12_RESOURCE_DESC stTXDesc = pITexture->GetDesc();
			UINT64 n64szUpSphere = 0;			
			pThdPms->m_pID3D12Device4->GetCopyableFootprints(&stTXDesc, 0, static_cast<UINT>(stArSubResources.size())
					, 0, nullptr, nullptr, nullptr, &n64szUpSphere);

			stBufferResSesc.Width = n64szUpSphere;
			GRS_THROW_IF_FAILED(pThdPms->m_pID3D12Device4->CreateCommittedResource(
				&stUploadHeapProps
				, D3D12_HEAP_FLAG_NONE
				, &stBufferResSesc
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pITextureUpload)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pITextureUpload);

			//UpdateSubresources(pThdPms->m_pICMDList
			//	, pITexture.Get()
			//	, pITextureUpload.Get()
			//	, 0
			//	, 0
			//	, static_cast<UINT>(stArSubResources.size())
			//	, stArSubResources.data());

			//ִ������Copy�����������ϴ���Ĭ�϶���

			UINT nFirstSubresource = 0;
			UINT nNumSubresources = static_cast<UINT>(stArSubResources.size());
			D3D12_RESOURCE_DESC stUploadResDesc = pITextureUpload->GetDesc();
			D3D12_RESOURCE_DESC stDefaultResDesc = pITexture->GetDesc();

			UINT64 n64RequiredSize = 0;
			SIZE_T szMemToAlloc = static_cast<UINT64>(sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT)
				+ sizeof(UINT)
				+ sizeof(UINT64))
				* nNumSubresources;

			void* pMem = GRS_CALLOC(static_cast<SIZE_T>(szMemToAlloc));

			if (nullptr == pMem)
			{
				throw CGRSCOMException(HRESULT_FROM_WIN32(GetLastError()));
			}

			D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pLayouts = reinterpret_cast<D3D12_PLACED_SUBRESOURCE_FOOTPRINT*>(pMem);
			UINT64* pRowSizesInBytes = reinterpret_cast<UINT64*>(pLayouts + nNumSubresources);
			UINT* pNumRows = reinterpret_cast<UINT*>(pRowSizesInBytes + nNumSubresources);

			// �����ǵڶ��ε���GetCopyableFootprints���͵õ�����������Դ����ϸ��Ϣ
			pThdPms->m_pID3D12Device4->GetCopyableFootprints(&stDefaultResDesc, nFirstSubresource, nNumSubresources, 0, pLayouts, pNumRows, pRowSizesInBytes, &n64RequiredSize);

			BYTE* pData = nullptr;
			GRS_THROW_IF_FAILED(pITextureUpload->Map(0, nullptr, reinterpret_cast<void**>(&pData)));

			// ��һ��Copy��ע��3��ѭ��ÿ�ص���˼
			for (UINT nSubRes = 0; nSubRes < nNumSubresources; ++nSubRes)
			{// SubResources
				if (pRowSizesInBytes[nSubRes] > (SIZE_T)-1)
				{
					throw CGRSCOMException(E_FAIL);
				}

				D3D12_MEMCPY_DEST stCopyDestData = { pData + pLayouts[nSubRes].Offset
					, pLayouts[nSubRes].Footprint.RowPitch
					, pLayouts[nSubRes].Footprint.RowPitch * pNumRows[nSubRes]
				};

				for (UINT z = 0; z < pLayouts[nSubRes].Footprint.Depth; ++z)
				{// Mipmap
					BYTE* pDestSlice = reinterpret_cast<BYTE*>(stCopyDestData.pData) + stCopyDestData.SlicePitch * z;
					const BYTE* pSrcSlice = reinterpret_cast<const BYTE*>(stArSubResources[nSubRes].pData) + stArSubResources[nSubRes].SlicePitch * z;
					for (UINT y = 0; y < pNumRows[nSubRes]; ++y)
					{// Rows
						memcpy(pDestSlice + stCopyDestData.RowPitch * y,
							pSrcSlice + stArSubResources[nSubRes].RowPitch * y,
							(SIZE_T)pRowSizesInBytes[nSubRes]);
					}
				}
			}
			pITextureUpload->Unmap(0, nullptr);

			// �ڶ���Copy��
			if (stDefaultResDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
			{// Buffer һ���Ը��ƾͿ����ˣ���Ϊû���ж�����д�С��һ�µ����⣬Buffer����������1
				pThdPms->m_pICMDList->CopyBufferRegion(
					pITexture.Get(), 0, pITextureUpload.Get(), pLayouts[0].Offset, pLayouts[0].Footprint.Width);
			}
			else
			{
				for (UINT nSubRes = 0; nSubRes < nNumSubresources; ++nSubRes)
				{
					D3D12_TEXTURE_COPY_LOCATION stDstCopyLocation = {};
					stDstCopyLocation.pResource = pITexture.Get();
					stDstCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
					stDstCopyLocation.SubresourceIndex = nSubRes;

					D3D12_TEXTURE_COPY_LOCATION stSrcCopyLocation = {};
					stSrcCopyLocation.pResource = pITextureUpload.Get();
					stSrcCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
					stSrcCopyLocation.PlacedFootprint = pLayouts[nSubRes];

					pThdPms->m_pICMDList->CopyTextureRegion(&stDstCopyLocation, 0, 0, 0, &stSrcCopyLocation, nullptr);
				}
			}
			GRS_SAFE_FREE(pMem);

			//ͬ��
			stResStateTransBarrier.Transition.pResource = pITexture.Get();
			stResStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
			stResStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

			pThdPms->m_pICMDList->ResourceBarrier(1, &stResStateTransBarrier);
		}

		//2��������������
		{
			D3D12_DESCRIPTOR_HEAP_DESC stSRVHeapDesc = {};
			stSRVHeapDesc.NumDescriptors = 2; // 1 CBV + 1 SRV
			stSRVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			stSRVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			GRS_THROW_IF_FAILED(pThdPms->m_pID3D12Device4->CreateDescriptorHeap(
				&stSRVHeapDesc
				, IID_PPV_ARGS(&pISRVHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pISRVHeap);

			D3D12_RESOURCE_DESC stTXDesc = pITexture->GetDesc();

			//����Texture �� SRV
			D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
			stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			stSRVDesc.Format = stTXDesc.Format;
			stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			stSRVDesc.Texture2D.MipLevels = 1;

			D3D12_CPU_DESCRIPTOR_HANDLE stCbvSrvHandle = pISRVHeap->GetCPUDescriptorHandleForHeapStart();
			stCbvSrvHandle.ptr += nSRVDescriptorSize;

			pThdPms->m_pID3D12Device4->CreateShaderResourceView(
				pITexture.Get()
				, &stSRVDesc
				, stCbvSrvHandle);
		}

		//3�������������� 
		{
			stBufferResSesc.Width = szMVPBuf; //ע�⻺��ߴ�����Ϊ256�߽�����С
			GRS_THROW_IF_FAILED(pThdPms->m_pID3D12Device4->CreateCommittedResource(
				&stUploadHeapProps
				, D3D12_HEAP_FLAG_NONE
				, &stBufferResSesc
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pICBWVP)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICBWVP);

			// Map ֮��Ͳ���Unmap�� ֱ�Ӹ������ݽ�ȥ ����ÿ֡������map-copy-unmap�˷�ʱ����
			GRS_THROW_IF_FAILED(pICBWVP->Map(0, nullptr, reinterpret_cast<void**>(&pMVPBufModule)));

			// ����CBV
			D3D12_CONSTANT_BUFFER_VIEW_DESC stCBVDesc = {};
			stCBVDesc.BufferLocation = pICBWVP->GetGPUVirtualAddress();
			stCBVDesc.SizeInBytes = static_cast<UINT>(szMVPBuf);

			D3D12_CPU_DESCRIPTOR_HANDLE stCbvSrvHandle = pISRVHeap->GetCPUDescriptorHandleForHeapStart();
			pThdPms->m_pID3D12Device4->CreateConstantBufferView(&stCBVDesc, stCbvSrvHandle);
		}

		//4������Sample
		{
			D3D12_DESCRIPTOR_HEAP_DESC stSamplerHeapDesc = {};
			stSamplerHeapDesc.NumDescriptors = 1;
			stSamplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			stSamplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			GRS_THROW_IF_FAILED(pThdPms->m_pID3D12Device4->CreateDescriptorHeap(
				&stSamplerHeapDesc
				, IID_PPV_ARGS(&pISampleHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pISampleHeap);

			D3D12_SAMPLER_DESC stSamplerDesc = {};
			stSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			stSamplerDesc.MinLOD = 0;
			stSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
			stSamplerDesc.MipLODBias = 0.0f;
			stSamplerDesc.MaxAnisotropy = 1;
			stSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			stSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			stSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			stSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

			pThdPms->m_pID3D12Device4->CreateSampler(&stSamplerDesc
				, pISampleHeap->GetCPUDescriptorHandleForHeapStart());
		}

		//5��������������
		{
			LoadMeshVertex(pThdPms->m_pszMeshFile
				, nVertexCnt
				, pstVertices
				, pnIndices);
			nIndexCnt = nVertexCnt;

			//���� Vertex Buffer ��ʹ��Upload��ʽ��
			stBufferResSesc.Width = nVertexCnt * sizeof(ST_GRS_VERTEX);
			GRS_THROW_IF_FAILED(pThdPms->m_pID3D12Device4->CreateCommittedResource(
				&stUploadHeapProps
				, D3D12_HEAP_FLAG_NONE
				, &stBufferResSesc
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pIVB)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIVB);


			UINT8* pVertexDataBegin = nullptr;
			D3D12_RANGE stReadRange = { 0, 0 };

			//ʹ��map-memcpy-unmap�󷨽����ݴ������㻺�����
			GRS_THROW_IF_FAILED(pIVB->Map(0
				, &stReadRange
				, reinterpret_cast<void**>(&pVertexDataBegin)));
			memcpy(pVertexDataBegin, pstVertices, nVertexCnt * sizeof(ST_GRS_VERTEX));
			pIVB->Unmap(0, nullptr);

			//���� Index Buffer ��ʹ��Upload��ʽ��
			stBufferResSesc.Width = nIndexCnt * sizeof(UINT);
			GRS_THROW_IF_FAILED(pThdPms->m_pID3D12Device4->CreateCommittedResource(
				&stUploadHeapProps
				, D3D12_HEAP_FLAG_NONE
				, &stBufferResSesc
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pIIB)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIIB);

			UINT8* pIndexDataBegin = nullptr;
			GRS_THROW_IF_FAILED(pIIB->Map(0
				, &stReadRange
				, reinterpret_cast<void**>(&pIndexDataBegin)));
			memcpy(pIndexDataBegin, pnIndices, nIndexCnt * sizeof(UINT));
			pIIB->Unmap(0, nullptr);

			//����Vertex Buffer View
			stVBV.BufferLocation = pIVB->GetGPUVirtualAddress();
			stVBV.StrideInBytes = sizeof(ST_GRS_VERTEX);
			stVBV.SizeInBytes = nVertexCnt * sizeof(ST_GRS_VERTEX);

			//����Index Buffer View
			stIBV.BufferLocation = pIIB->GetGPUVirtualAddress();
			stIBV.Format = DXGI_FORMAT_R32_UINT;
			stIBV.SizeInBytes = nIndexCnt * sizeof(UINT);

			GRS_SAFE_FREE(pstVertices);
			GRS_SAFE_FREE(pnIndices);
		}

		//6�������¼����� ֪ͨ���л����߳� �����Դ�ĵڶ���Copy����
		{
			GRS_THROW_IF_FAILED(pThdPms->m_pICMDList->Close());
			//��һ��֪ͨ���̱߳��̼߳�����Դ���
			::SetEvent(pThdPms->m_hEventRenderOver); // �����źţ�֪ͨ���̱߳��߳���Դ�������
		}

		// ��������������������Ч�ı���
		float fJmpSpeed = 0.003f; //�����ٶ�
		float fUp = 1.0f;
		float fRawYPos = pThdPms->m_v4ModelPos.y;
		XMMATRIX xmMWVP;
		CAtlArray<ID3D12DescriptorHeap*> arDesHeaps;
		DWORD dwRet = 0;
		BOOL  bQuit = FALSE;
		MSG   msg = {};

		//7����Ⱦѭ��
		while (!bQuit)
		{
			// �ȴ����߳�֪ͨ��ʼ��Ⱦ��ͬʱ���������߳�Post��������Ϣ��Ŀǰ����Ϊ�˵ȴ�WM_QUIT��Ϣ
			dwRet = ::MsgWaitForMultipleObjects(1, &pThdPms->m_hEventRun, FALSE, INFINITE, QS_ALLPOSTMESSAGE);
			switch (dwRet - WAIT_OBJECT_0)
			{
			case 0:
			{
				// OnUpdate()
				// ׼��MWVP����
				{
					//==============================================================================
					//ʹ�����̸߳��µ�ͳһ֡ʱ����������������״̬�ȣ��������ʾ������������������
					//����ʽ������˵�����ö��߳���Ⱦʱ������任�����ڲ�ͬ���߳��У������ڲ�ͬ��CPU�ϲ��е�ִ��
					if (g_nThdSphere == pThdPms->m_nThreadIndex)
					{
						if (pThdPms->m_v4ModelPos.y >= 2.0f * fRawYPos)
						{
							fUp = -0.2f;
							pThdPms->m_v4ModelPos.y = 2.0f * fRawYPos;
						}

						if (pThdPms->m_v4ModelPos.y <= fRawYPos)
						{
							fUp = 0.2f;
							pThdPms->m_v4ModelPos.y = fRawYPos;
						}

						pThdPms->m_v4ModelPos.y
							+= fUp * fJmpSpeed * static_cast<float>(pThdPms->m_nCurrentTime - pThdPms->m_nStartTime);

						mxPosModule = XMMatrixTranslationFromVector(XMLoadFloat4(&pThdPms->m_v4ModelPos));
					}
					//==============================================================================

					// Module * World
					xmMWVP = XMMatrixMultiply(mxPosModule, XMLoadFloat4x4(&g_mxWorld));

					// (Module * World) * View * Projection
					xmMWVP = XMMatrixMultiply(xmMWVP, XMLoadFloat4x4(&g_mxVP));

					XMStoreFloat4x4(&pMVPBufModule->m_MVP, xmMWVP);
				}

				// OnRender()
				//�����������Resetһ�£��ղ��Ѿ�ִ�й���һ���������������
				GRS_THROW_IF_FAILED(pThdPms->m_pICMDAlloc->Reset());
				//Reset�����б�������ָ�������������PSO����
				GRS_THROW_IF_FAILED(pThdPms->m_pICMDList->Reset(pThdPms->m_pICMDAlloc, pThdPms->m_pIPSO));

				//---------------------------------------------------------------------------------------------
				//���ö�Ӧ����ȾĿ����Ӳü���(������Ⱦ���̱߳���Ҫ���Ĳ��裬����Ҳ������ν���߳���Ⱦ�ĺ�������������)
				{
					D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle = pThdPms->m_pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
					stRTVHandle.ptr += (pThdPms->m_nCurrentFrameIndex * nRTVDescriptorSize);
					D3D12_CPU_DESCRIPTOR_HANDLE stDSVHandle = pThdPms->m_pIDSVHeap->GetCPUDescriptorHandleForHeapStart();
					//������ȾĿ��
					pThdPms->m_pICMDList->OMSetRenderTargets(1, &stRTVHandle, FALSE, &stDSVHandle);
					pThdPms->m_pICMDList->RSSetViewports(1, &g_stViewPort);
					pThdPms->m_pICMDList->RSSetScissorRects(1, &g_stScissorRect);
				}
				//---------------------------------------------------------------------------------------------

				//��Ⱦ��ʵ�ʾ��Ǽ�¼��Ⱦ�����б�
				{
					pThdPms->m_pICMDList->SetGraphicsRootSignature(pThdPms->m_pIRS);
					pThdPms->m_pICMDList->SetPipelineState(pThdPms->m_pIPSO);

					arDesHeaps.RemoveAll();
					arDesHeaps.Add(pISRVHeap.Get());
					arDesHeaps.Add(pISampleHeap.Get());

					pThdPms->m_pICMDList->SetDescriptorHeaps(static_cast<UINT>(arDesHeaps.GetCount()), arDesHeaps.GetData());

					D3D12_GPU_DESCRIPTOR_HANDLE stSRVHandle = pISRVHeap->GetGPUDescriptorHandleForHeapStart();
					//����CBV
					pThdPms->m_pICMDList->SetGraphicsRootDescriptorTable(0, stSRVHandle);

					//ƫ����Texture��View
					stSRVHandle.ptr += nSRVDescriptorSize;
					//����SRV
					pThdPms->m_pICMDList->SetGraphicsRootDescriptorTable(1, stSRVHandle);

					//����Sample
					pThdPms->m_pICMDList->SetGraphicsRootDescriptorTable(2, pISampleHeap->GetGPUDescriptorHandleForHeapStart());

					//ע������ʹ�õ���Ⱦ�ַ����������б�Ҳ����ͨ����Mesh����
					pThdPms->m_pICMDList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
					pThdPms->m_pICMDList->IASetVertexBuffers(0, 1, &stVBV);
					pThdPms->m_pICMDList->IASetIndexBuffer(&stIBV);

					//Draw Call������
					pThdPms->m_pICMDList->DrawIndexedInstanced(nIndexCnt, 1, 0, 0, 0);
				}

				//�����Ⱦ�����ر������б�������ͬ������֪ͨ���߳̿�ʼִ�У�
				{
					GRS_THROW_IF_FAILED(pThdPms->m_pICMDList->Close());
					::SetEvent(pThdPms->m_hEventRenderOver); // �����źţ�֪ͨ���̱߳��߳���Ⱦ���
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
	catch (CGRSCOMException& e)
	{
		e;
	}

	return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY:
	{
		PostQuitMessage(0);
	}
	break;
	case WM_KEYDOWN:
	{
		USHORT n16KeyCode = (wParam & 0xFF);

		if (VK_SPACE == n16KeyCode)
		{//�л�Ч��
			g_nFunNO += 1;
			if (g_nFunNO >= g_nMaxFunNO)
			{
				g_nFunNO = 0;
			}
		}

		if (VK_TAB == n16KeyCode)
		{
			++g_nUsePSID;
			if (g_nUsePSID > 1)
			{
				g_nUsePSID = 0;
			}
		}

		if (1 == g_nFunNO)
		{//������ˮ�ʻ�Ч����Ⱦʱ������Ч
			if ('Q' == n16KeyCode || 'q' == n16KeyCode)
			{//q ����ˮ��ȡɫ�뾶
				g_fWaterPower += 1.0f;
				if (g_fWaterPower >= 64.0f)
				{
					g_fWaterPower = 8.0f;
				}
			}

			if ('E' == n16KeyCode || 'e' == n16KeyCode)
			{//e ����������������Χ 2 - 6
				g_fQuatLevel += 1.0f;
				if (g_fQuatLevel > 6.0f)
				{
					g_fQuatLevel = 2.0f;
				}
			}
		}

		if (VK_ADD == n16KeyCode || VK_OEM_PLUS == n16KeyCode)
		{
			//double g_fPalstance = 10.0f * XM_PI / 180.0f;	//������ת�Ľ��ٶȣ���λ������/��
			g_fPalstance += 10.0f * XM_PI / 180.0f;
			if (g_fPalstance > XM_PI)
			{
				g_fPalstance = XM_PI;
			}
		}

		if (VK_SUBTRACT == n16KeyCode || VK_OEM_MINUS == n16KeyCode)
		{
			g_fPalstance -= 10.0f * XM_PI / 180.0f;
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

		if (VK_ESCAPE == n16KeyCode)
		{//��ESC����ԭ�����λ��
			g_f3EyePos = XMFLOAT3(0.0f, 5.0f, -10.0f); //�۾�λ��
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

BOOL LoadMeshVertex(const CHAR* pszMeshFileName, UINT& nVertexCnt, ST_GRS_VERTEX*& ppVertex, UINT*& ppIndices)
{
	ifstream fin;
	char input;
	BOOL bRet = TRUE;
	try
	{
		fin.open(pszMeshFileName);
		if (fin.fail())
		{
			throw CGRSCOMException(E_FAIL);
		}
		fin.get(input);
		while (input != ':')
		{
			fin.get(input);
		}
		fin >> nVertexCnt;

		fin.get(input);
		while (input != ':')
		{
			fin.get(input);
		}
		fin.get(input);
		fin.get(input);

		ppVertex = (ST_GRS_VERTEX*)GRS_ALLOC(nVertexCnt * sizeof(ST_GRS_VERTEX));
		ppIndices = (UINT*)GRS_ALLOC(nVertexCnt * sizeof(UINT));

		for (UINT i = 0; i < nVertexCnt; i++)
		{
			fin >> ppVertex[i].m_v4Position.x >> ppVertex[i].m_v4Position.y >> ppVertex[i].m_v4Position.z;
			ppVertex[i].m_v4Position.w = 1.0f;
			fin >> ppVertex[i].m_vTex.x >> ppVertex[i].m_vTex.y;
			fin >> ppVertex[i].m_vNor.x >> ppVertex[i].m_vNor.y >> ppVertex[i].m_vNor.z;

			ppIndices[i] = i;
		}
	}
	catch (CGRSCOMException& e)
	{
		e;
		bRet = FALSE;
	}
	return bRet;
}
