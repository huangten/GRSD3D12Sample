#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // �� Windows ͷ���ų�����ʹ�õ�����
#include <windows.h>
#include <tchar.h>
#include <fstream>		//for ifstream
#include <wrl.h>		//���WTL֧�� ����ʹ��COM
#include <atlconv.h>	//for T2A
#include <atlcoll.h>	//for atl array
#include <strsafe.h>	//for StringCchxxxxx function
#include <dxgi1_6.h>
#include <d3d12.h>		//for d3d12
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

#define GRS_WND_CLASS_NAME _T("GRS Game Window Class")
#define GRS_WND_TITLE	_T("GRS D3D12 MultiAdapter Sample")

#define GRS_SAFE_RELEASE(p) if(p){(p)->Release();(p)=nullptr;}
#define GRS_THROW_IF_FAILED(hr) {HRESULT _hr = (hr);if (FAILED(_hr)){ throw CGRSCOMException(_hr); }}

//�¶���ĺ�������ȡ������
#define GRS_UPPER_DIV(A,B) ((UINT)(((A)+((B)-1))/(B)))
//���������ϱ߽�����㷨 �ڴ�����г��� ���ס
#define GRS_UPPER(A,B) ((UINT)(((A)+((B)-1))&~(B - 1)))

// �ڴ����ĺ궨��
#define GRS_ALLOC(sz)		::HeapAlloc(GetProcessHeap(),0,(sz))
#define GRS_CALLOC(sz)		::HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(sz))
#define GRS_CREALLOC(p,sz)	::HeapReAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(p),(sz))
#define GRS_SAFE_FREE(p)	if( nullptr != (p) ){ ::HeapFree( ::GetProcessHeap(),0,(p) ); (p) = nullptr; }

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

// ����ṹ
struct ST_GRS_VERTEX_MODULE
{
	XMFLOAT4 m_v4Position;		//Position
	XMFLOAT2 m_vTex;		//Texcoord
	XMFLOAT3 m_vNor;		//Normal
};

// �����Կ�����Ⱦ��λ���εĶ���ṹ
struct ST_GRS_VERTEX_QUAD
{
	XMFLOAT4 m_v4Position;		//Position
	XMFLOAT2 m_vTex;		//Texcoord
};

const UINT g_nFrameBackBufCount = 3u;
// �Կ���������
struct ST_GRS_GPU_PARAMS
{
	UINT								m_nIndex;
	UINT								m_nszRTV;
	UINT								m_nszSRVCBVUAV;

	ComPtr<ID3D12Device4>				m_pID3D12Device4;
	ComPtr<ID3D12CommandQueue>			m_pICmdQueue;
	ComPtr<ID3D12DescriptorHeap>		m_pIDHRTV;
	ComPtr<ID3D12Resource>				m_pIRTRes[g_nFrameBackBufCount];
	ComPtr<ID3D12CommandAllocator>		m_pICmdAllocPerFrame[g_nFrameBackBufCount];
	ComPtr<ID3D12GraphicsCommandList>	m_pICmdList;
	ComPtr<ID3D12Heap>					m_pICrossAdapterHeap;
	ComPtr<ID3D12Resource>				m_pICrossAdapterResPerFrame[g_nFrameBackBufCount];
	ComPtr<ID3D12Fence>					m_pIFence;
	ComPtr<ID3D12Fence>					m_pISharedFence;
	ComPtr<ID3D12Resource>				m_pIDSTex;
	ComPtr<ID3D12DescriptorHeap>		m_pIDHDSVTex;
};

// ����������
struct ST_GRS_MVP
{
	XMFLOAT4X4 m_MVP;			//�����Model-view-projection(MVP)����.
};

// �����������
struct ST_GRS_MODULE_PARAMS
{
	UINT								nIndex;
	XMFLOAT4							v4ModelPos;
	TCHAR								pszDDSFile[MAX_PATH];
	CHAR								pszMeshFile[MAX_PATH];
	UINT								nVertexCnt;
	UINT								nIndexCnt;
	ComPtr<ID3D12Resource>				pITexture;
	ComPtr<ID3D12Resource>				pITextureUpload;
	ComPtr<ID3D12DescriptorHeap>		pISampleHp;
	ComPtr<ID3D12Resource>				pIVertexBuffer;
	ComPtr<ID3D12Resource>				pIIndexsBuffer;
	ComPtr<ID3D12DescriptorHeap>		pISRVCBVHp;
	ComPtr<ID3D12Resource>			    pIConstBuffer;
	ComPtr<ID3D12CommandAllocator>		pIBundleAlloc;
	ComPtr<ID3D12GraphicsCommandList>	pIBundle;
	ST_GRS_MVP* pMVPBuf;
	D3D12_VERTEX_BUFFER_VIEW			stVertexBufferView;
	D3D12_INDEX_BUFFER_VIEW				stIndexsBufferView;
};


//��ʼ��Ĭ���������λ��
XMFLOAT3 g_f3EyePos = XMFLOAT3(0.0f, 5.0f, -10.0f);  //�۾�λ��
XMFLOAT3 g_f3LockAt = XMFLOAT3(0.0f, 0.0f, 0.0f);    //�۾�������λ��
XMFLOAT3 g_f3HeapUp = XMFLOAT3(0.0f, 1.0f, 0.0f);    //ͷ�����Ϸ�λ��

float g_fYaw = 0.0f;								// ����Z�����ת��.
float g_fPitch = 0.0f;								// ��XZƽ�����ת��
double g_fPalstance = 10.0f * XM_PI / 180.0f;		//������ת�Ľ��ٶȣ���λ������/��


LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
BOOL LoadMeshVertex(const CHAR* pszMeshFileName, UINT& nVertexCnt, ST_GRS_VERTEX_MODULE*& ppVertex, UINT*& ppIndices);

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR    lpCmdLine, int nCmdShow)
{
	::CoInitialize(nullptr);  //for WIC & COM
	try
	{
		UINT								nWndWidth = 1024;
		UINT								nWndHeight = 768;
		HWND								hWnd = nullptr;
		MSG									msg = {};
		TCHAR								pszAppPath[MAX_PATH] = {};

		
		UINT								nCurrentFrameIndex = 0u;
		DXGI_FORMAT							emRTFmt = DXGI_FORMAT_R8G8B8A8_UNORM;
		DXGI_FORMAT							emDSFmt = DXGI_FORMAT_D24_UNORM_S8_UINT;

		D3D12_VIEWPORT						stViewPort = { 0.0f, 0.0f, static_cast<float>(nWndWidth), static_cast<float>(nWndHeight), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
		D3D12_RECT							stScissorRect = { 0, 0, static_cast<LONG>(nWndWidth), static_cast<LONG>(nWndHeight) };

		const UINT							nMaxGPUParams = 2;			//��ʾ��֧������GPU������Ⱦ
		const UINT							nIDGPUMain = 0;
		const UINT							nIDGPUSecondary = 1;
		ST_GRS_GPU_PARAMS					stGPUParams[nMaxGPUParams] = {};

		// �����������
		const UINT							nMaxObject = 3;
		const UINT							nSphere = 0;
		const UINT							nCube = 1;
		const UINT							nPlane = 2;
		ST_GRS_MODULE_PARAMS				stModuleParams[nMaxObject] = {};

		// ������������С�϶��뵽256Bytes�߽�
		SIZE_T								szMVPBuf = GRS_UPPER(sizeof(ST_GRS_MVP), 256);

		// ���ڸ�����Ⱦ����ĸ���������С�����������������б����
		ComPtr<ID3D12CommandQueue>			pICmdQueueCopy;
		ComPtr<ID3D12CommandAllocator>		pICmdAllocCopyPerFrame[g_nFrameBackBufCount];
		ComPtr<ID3D12GraphicsCommandList>	pICmdListCopy;

		//������ֱ�ӹ�����Դʱ�����ڸ����Կ��ϴ�����������Դ�����ڸ������Կ���Ⱦ�������
		ComPtr<ID3D12Resource>				pISecondaryAdapterTexutrePerFrame[g_nFrameBackBufCount];
		BOOL								bCrossAdapterTextureSupport = FALSE;
		D3D12_RESOURCE_DESC					stRenderTargetDesc = {};
		const float							arf4ClearColor[4] = { 0.2f, 0.5f, 1.0f, 1.0f };

		ComPtr<IDXGIFactory5>				pIDXGIFactory5;
		ComPtr<IDXGIFactory6>				pIDXGIFactory6;
		ComPtr<IDXGISwapChain1>				pISwapChain1;
		ComPtr<IDXGISwapChain3>				pISwapChain3;

		ComPtr<ID3DBlob>					pIVSMain;
		ComPtr<ID3DBlob>					pIPSMain;
		ComPtr<ID3D12RootSignature>			pIRSMain;
		ComPtr<ID3D12PipelineState>			pIPSOMain;

		ComPtr<ID3DBlob>					pIVSQuad;
		ComPtr<ID3DBlob>					pIPSQuad;
		ComPtr<ID3D12RootSignature>			pIRSQuad;
		ComPtr<ID3D12PipelineState>			pIPSOQuad;

		ComPtr<ID3D12Resource>				pIVBQuad;
		ComPtr<ID3D12Resource>				pIVBQuadUpload;
		D3D12_VERTEX_BUFFER_VIEW			pstVBVQuad;

		ComPtr<ID3D12DescriptorHeap>		pIDHSRVSecondary;
		ComPtr<ID3D12DescriptorHeap>		pIDHSampleSecondary;

		HANDLE								hEventFence = nullptr;
		UINT64								n64CurrentFenceValue = 0;
		UINT64								n64FenceValue = 1ui64;


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

		// �õ���ǰ�Ĺ���Ŀ¼����������ʹ�����·�������ʸ�����Դ�ļ�
		{
			if (0 == ::GetModuleFileName(nullptr, pszAppPath, MAX_PATH))
			{
				GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
			}

			WCHAR* lastSlash = _tcsrchr(pszAppPath, _T('\\'));
			if (lastSlash)
			{//ɾ��Exe�ļ���
				*(lastSlash) = _T('\0');
			}

			lastSlash = _tcsrchr(pszAppPath, _T('\\'));
			if (lastSlash)
			{//ɾ��x64·��
				*(lastSlash) = _T('\0');
			}

			lastSlash = _tcsrchr(pszAppPath, _T('\\'));
			if (lastSlash)
			{//ɾ��Debug �� Release·��
				*(lastSlash + 1) = _T('\0');
			}
		}

		// ��������
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
			RECT rtWnd = { 0, 0, nWndWidth, nWndHeight };
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

		// ����DXGI Factory����
		{
			UINT nDXGIFactoryFlags = 0U;
			// ����ʾ��ϵͳ�ĵ���֧��
#if defined(_DEBUG)
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

		// ö�������������豸
		{// ʹ��DXGI���º���EnumAdapterByGpuPreference���Ӹߵ���ö��ϵͳ�е��Կ�
			DXGI_ADAPTER_DESC1 stAdapterDesc[nMaxGPUParams] = {};
		    IDXGIAdapter1* pIAdapterTmp = nullptr;
			UINT			i = 0;
			for (i = 0;
				(i < nMaxGPUParams) &&
				SUCCEEDED(pIDXGIFactory6->EnumAdapterByGpuPreference(
					i
					, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
					, IID_PPV_ARGS(&pIAdapterTmp)));
				++i)
			{
				GRS_THROW_IF_FAILED(pIAdapterTmp->GetDesc1(&stAdapterDesc[i]));

				if (stAdapterDesc[i].Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				{//������������������豸��
				 //ע�����if�жϣ�����ʹ��һ����ʵGPU��һ������������������˫GPU��ʾ��
					continue;
				}

				if (i == nIDGPUMain)
				{
					GRS_THROW_IF_FAILED(D3D12CreateDevice(pIAdapterTmp
						, D3D_FEATURE_LEVEL_12_1
						, IID_PPV_ARGS(&stGPUParams[nIDGPUMain].m_pID3D12Device4)));
				}
				else
				{
					GRS_THROW_IF_FAILED(D3D12CreateDevice(pIAdapterTmp
						, D3D_FEATURE_LEVEL_12_1
						, IID_PPV_ARGS(&stGPUParams[nIDGPUSecondary].m_pID3D12Device4)));
				}
				GRS_SAFE_RELEASE(pIAdapterTmp);
			}

			//---------------------------------------------------------------------------------------------
			if (nullptr == stGPUParams[nIDGPUMain].m_pID3D12Device4.Get()
				|| nullptr == stGPUParams[nIDGPUSecondary].m_pID3D12Device4.Get())
			{// �����Ļ����Ͼ�Ȼû���������ϵ��Կ� �������˳����� ��Ȼ�����ʹ�����������ջ������
				OutputDebugString(_T("\n�������Կ���������������ʾ�������˳���\n"));
				throw CGRSCOMException(E_FAIL);
			}

			GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPUParams[nIDGPUMain].m_pID3D12Device4);
			GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPUParams[nIDGPUSecondary].m_pID3D12Device4);

			TCHAR pszWndTitle[MAX_PATH] = {};
			::GetWindowText(hWnd, pszWndTitle, MAX_PATH);
			StringCchPrintf(pszWndTitle
				, MAX_PATH
				, _T("%s(GPU[0]:%s & GPU[1]:%s)")
				, pszWndTitle
				, stAdapterDesc[nIDGPUMain].Description
				, stAdapterDesc[nIDGPUSecondary].Description);
			::SetWindowText(hWnd, pszWndTitle);
		}

		// �����������
		if(TRUE)
		{
			D3D12_COMMAND_QUEUE_DESC stQueueDesc = {};
			stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

			for (int i = 0; i < nMaxGPUParams; i++)
			{
				GRS_THROW_IF_FAILED(stGPUParams[i].m_pID3D12Device4->CreateCommandQueue(&stQueueDesc, IID_PPV_ARGS(&stGPUParams[i].m_pICmdQueue)));
				GRS_SetD3D12DebugNameIndexed(stGPUParams[i].m_pICmdQueue.Get(), _T("m_pICmdQueue"), i);

				//�������ѭ����ɸ��������ĸ�ֵ��ע�����������ϵ�ÿ���������Ĵ�С������ͬ��GPU����ͬ�����Ե�����ΪGPU�Ĳ���
				//ͬʱ����Щֵһ����ȡ�����洢���Ͳ���ÿ�ε��ú�����������������͵��ô����������������
				stGPUParams[i].m_nIndex = i;
				stGPUParams[i].m_nszRTV = stGPUParams[i].m_pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
				stGPUParams[i].m_nszSRVCBVUAV = stGPUParams[i].m_pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}

			// ����������д����ڶ�����, ������Ҫ�ö�������Ⱦ��������Դ��Ҫ���Ƶ�������
			stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateCommandQueue(&stQueueDesc, IID_PPV_ARGS(&pICmdQueueCopy)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICmdQueueCopy);
		}

		// ����Χ�������Լ���GPUͬ��Χ������
		{
			for (int iGPUIndex = 0; iGPUIndex < nMaxGPUParams; iGPUIndex++)
			{
				GRS_THROW_IF_FAILED(stGPUParams[iGPUIndex].m_pID3D12Device4->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&stGPUParams[iGPUIndex].m_pIFence)));
			}

			// �����Կ��ϴ���һ���ɹ����Χ������
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateFence(0
				, D3D12_FENCE_FLAG_SHARED | D3D12_FENCE_FLAG_SHARED_CROSS_ADAPTER
				, IID_PPV_ARGS(&stGPUParams[nIDGPUMain].m_pISharedFence)));

			// �������Χ����ͨ�������ʽ
			HANDLE hFenceShared = nullptr;
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateSharedHandle(
				stGPUParams[nIDGPUMain].m_pISharedFence.Get(),
				nullptr,
				GENERIC_ALL,
				nullptr,
				&hFenceShared));

			// �ڸ����Կ��ϴ����Χ��������ɹ���
			HRESULT hrOpenSharedHandleResult = stGPUParams[nIDGPUSecondary].m_pID3D12Device4->OpenSharedHandle(hFenceShared
				, IID_PPV_ARGS(&stGPUParams[nIDGPUSecondary].m_pISharedFence));

			// �ȹرվ�������ж��Ƿ���ɹ�
			::CloseHandle(hFenceShared);

			GRS_THROW_IF_FAILED(hrOpenSharedHandleResult);

			hEventFence = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (hEventFence == nullptr)
			{
				GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
			}
		}

		// ����������
		{
			DXGI_SWAP_CHAIN_DESC1 stSwapChainDesc = {};
			stSwapChainDesc.BufferCount = g_nFrameBackBufCount;
			stSwapChainDesc.Width = nWndWidth;
			stSwapChainDesc.Height = nWndHeight;
			stSwapChainDesc.Format = emRTFmt;
			stSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			stSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			stSwapChainDesc.SampleDesc.Count = 1;

			GRS_THROW_IF_FAILED(pIDXGIFactory5->CreateSwapChainForHwnd(
				stGPUParams[nIDGPUSecondary].m_pICmdQueue.Get(),		// ʹ�ýӲ�����ʾ�����Կ������������Ϊ���������������
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
			D3D12_DESCRIPTOR_HEAP_DESC		stRTVHeapDesc = {};
			stRTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			stRTVHeapDesc.NumDescriptors = g_nFrameBackBufCount;			
			stRTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

			for (int i = 0; i < nMaxGPUParams; i++)
			{
				GRS_THROW_IF_FAILED(stGPUParams[i].m_pID3D12Device4->CreateDescriptorHeap(&stRTVHeapDesc, IID_PPV_ARGS(&stGPUParams[i].m_pIDHRTV)));
				GRS_SetD3D12DebugNameIndexed(stGPUParams[i].m_pIDHRTV.Get(), _T("m_pIDHRTV"), i);
			}

			D3D12_CLEAR_VALUE   stClearValue = { };
			stClearValue.Format	  = stSwapChainDesc.Format;
			stClearValue.Color[0] = arf4ClearColor[0];
			stClearValue.Color[1] = arf4ClearColor[1];
			stClearValue.Color[2] = arf4ClearColor[2];
			stClearValue.Color[3] = arf4ClearColor[3];

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
			for (UINT iGPUIndex = 0; iGPUIndex < nMaxGPUParams; iGPUIndex++)
			{
				D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = stGPUParams[iGPUIndex].m_pIDHRTV->GetCPUDescriptorHandleForHeapStart();

				for (UINT j = 0; j < g_nFrameBackBufCount; j++)
				{
					if (iGPUIndex == nIDGPUSecondary)
					{
						GRS_THROW_IF_FAILED(pISwapChain3->GetBuffer(j, IID_PPV_ARGS(&stGPUParams[iGPUIndex].m_pIRTRes[j])));
					}
					else
					{
						GRS_THROW_IF_FAILED(stGPUParams[iGPUIndex].m_pID3D12Device4->CreateCommittedResource(
							&stDefautHeapProps,
							D3D12_HEAP_FLAG_NONE,
							&stRenderTargetDesc,
							D3D12_RESOURCE_STATE_COMMON,
							&stClearValue,
							IID_PPV_ARGS(&stGPUParams[iGPUIndex].m_pIRTRes[j])));
					}

					stGPUParams[iGPUIndex].m_pID3D12Device4->CreateRenderTargetView(stGPUParams[iGPUIndex].m_pIRTRes[j].Get(), nullptr, rtvHandle);
					rtvHandle.ptr += stGPUParams[iGPUIndex].m_nszRTV;

					GRS_THROW_IF_FAILED(stGPUParams[iGPUIndex].m_pID3D12Device4->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT
						, IID_PPV_ARGS(&stGPUParams[iGPUIndex].m_pICmdAllocPerFrame[j])));

					if (iGPUIndex == nIDGPUMain)
					{ //�������Կ��ϵĸ�����������õ������������ÿ֡ʹ��һ��������
						GRS_THROW_IF_FAILED(stGPUParams[iGPUIndex].m_pID3D12Device4->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY
							, IID_PPV_ARGS(&pICmdAllocCopyPerFrame[j])));
					}
				}

				// ����ÿ��GPU�������б����
				GRS_THROW_IF_FAILED(stGPUParams[iGPUIndex].m_pID3D12Device4->CreateCommandList(0
					, D3D12_COMMAND_LIST_TYPE_DIRECT
					, stGPUParams[iGPUIndex].m_pICmdAllocPerFrame[nCurrentFrameIndex].Get()
					, nullptr
					, IID_PPV_ARGS(&stGPUParams[iGPUIndex].m_pICmdList)));


				if (SUCCEEDED(StringCchPrintfW(pszDebugName, MAX_PATH, L"stGPUParams[%u].m_pICmdList", iGPUIndex)))
				{
					stGPUParams[iGPUIndex].m_pICmdList->SetName(pszDebugName);
				}

				if ( iGPUIndex == nIDGPUMain )
				{// �����Կ��ϴ������������б����
					GRS_THROW_IF_FAILED(stGPUParams[iGPUIndex].m_pID3D12Device4->CreateCommandList(0
						, D3D12_COMMAND_LIST_TYPE_COPY
						, pICmdAllocCopyPerFrame[nCurrentFrameIndex].Get()
						, nullptr
						, IID_PPV_ARGS(&pICmdListCopy)));

					GRS_SET_D3D12_DEBUGNAME_COMPTR(pICmdListCopy);
				}

				// ����ÿ���Կ��ϵ�������建����
				D3D12_CLEAR_VALUE stDepthOptimizedClearValue = {};
				stDepthOptimizedClearValue.Format = emDSFmt;
				stDepthOptimizedClearValue.DepthStencil.Depth = 1.0f;
				stDepthOptimizedClearValue.DepthStencil.Stencil = 0;

				D3D12_RESOURCE_DESC stDSResDesc = {};
				stDSResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
				stDSResDesc.Alignment = 0;
				stDSResDesc.Format = emDSFmt;
				stDSResDesc.Width = nWndWidth;
				stDSResDesc.Height = nWndHeight;
				stDSResDesc.DepthOrArraySize = 1;
				stDSResDesc.MipLevels = 0;
				stDSResDesc.SampleDesc.Count = 1;
				stDSResDesc.SampleDesc.Quality = 0;
				stDSResDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
				stDSResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

				//ʹ����ʽĬ�϶Ѵ���һ��������建������
				//��Ϊ��������Ȼ�������һֱ��ʹ�ã����õ����岻������ֱ��ʹ����ʽ�ѣ�ͼ����
				GRS_THROW_IF_FAILED(stGPUParams[iGPUIndex].m_pID3D12Device4->CreateCommittedResource(
					&stDefautHeapProps
					, D3D12_HEAP_FLAG_NONE
					, &stDSResDesc
					, D3D12_RESOURCE_STATE_DEPTH_WRITE
					, &stDepthOptimizedClearValue
					, IID_PPV_ARGS(&stGPUParams[iGPUIndex].m_pIDSTex)
				));
				GRS_SetD3D12DebugNameIndexed(stGPUParams[iGPUIndex].m_pIDSTex.Get(), _T("m_pIDSTex"), iGPUIndex);

				D3D12_DEPTH_STENCIL_VIEW_DESC stDepthStencilDesc = {};
				stDepthStencilDesc.Format = emDSFmt;
				stDepthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
				stDepthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

				D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
				dsvHeapDesc.NumDescriptors = 1;
				dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
				dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
				GRS_THROW_IF_FAILED(stGPUParams[iGPUIndex].m_pID3D12Device4->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&stGPUParams[iGPUIndex].m_pIDHDSVTex)));
				GRS_SetD3D12DebugNameIndexed(stGPUParams[iGPUIndex].m_pIDHDSVTex.Get(), _T("m_pIDHDSVTex"), iGPUIndex);

				stGPUParams[iGPUIndex].m_pID3D12Device4->CreateDepthStencilView(stGPUParams[iGPUIndex].m_pIDSTex.Get()
					, &stDepthStencilDesc
					, stGPUParams[iGPUIndex].m_pIDHDSVTex->GetCPUDescriptorHandleForHeapStart());
			}

			// �ر�ALT+ENTER���л�ȫ���Ĺ��ܣ���Ϊ����û��ʵ��OnSize���������ȹر�
			GRS_THROW_IF_FAILED(pIDXGIFactory5->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
		}

		// �������������Ĺ�����Դ��
		if(TRUE)
		{
			{
				D3D12_FEATURE_DATA_D3D12_OPTIONS stOptions = {};
				// ͨ����������ʾ������Կ��Ƿ�֧�ֿ��Կ���Դ���������Կ�����Դ��δ���
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3D12Device4->CheckFeatureSupport(
					D3D12_FEATURE_D3D12_OPTIONS, reinterpret_cast<void*>(&stOptions), sizeof(stOptions)));

				bCrossAdapterTextureSupport = stOptions.CrossAdapterRowMajorTextureSupported;

				UINT64 n64szTexture = 0;
				D3D12_RESOURCE_DESC stCrossAdapterResDesc = {};

				if ( bCrossAdapterTextureSupport )
				{// ���ʾ�������Կ���ֱ�ӹ���Texture�����ʡȥ��Texture���ظ��Ƶ��鷳
					// ���֧����ôֱ�Ӵ������Կ���Դ��
					stCrossAdapterResDesc = stRenderTargetDesc;
					stCrossAdapterResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
					stCrossAdapterResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

					D3D12_RESOURCE_ALLOCATION_INFO stTextureInfo
						= stGPUParams[nIDGPUMain].m_pID3D12Device4->GetResourceAllocationInfo(0, 1, &stCrossAdapterResDesc);
					n64szTexture = stTextureInfo.SizeInBytes;
				}
				else
				{// ���ʾֻ���ڶ���Կ��乲��Buffer����Ҳ��ĿǰD3D12������Դ�����Ҫ��ͨ���칹�Կ�Ŀǰֻ�ܹ���Buffer
				 // ֻ�ܹ���Bufferʱ��ֻ��ͨ�������Buffer���ظ���Texture����ʾ���о���Ҫ�Ǹ���Render Target Texture
					// �����֧�֣���ô���Ǿ���Ҫ�������Կ��ϴ������ڸ�����Ⱦ�������Դ�ѣ�Ȼ���ٹ���ѵ������Կ���
					D3D12_PLACED_SUBRESOURCE_FOOTPRINT stResLayout = {};
					stGPUParams[nIDGPUMain].m_pID3D12Device4->GetCopyableFootprints(
						&stRenderTargetDesc, 0, 1, 0, &stResLayout, nullptr, nullptr, nullptr);

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

				// �������Կ��������Դ�ѣ�ע����Ĭ�϶�
				D3D12_HEAP_DESC stCrossHeapDesc = {};
				stCrossHeapDesc.SizeInBytes = n64szTexture * g_nFrameBackBufCount;
				stCrossHeapDesc.Alignment = 0;
				stCrossHeapDesc.Flags = D3D12_HEAP_FLAG_SHARED | D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER;
				stCrossHeapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
				stCrossHeapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
				stCrossHeapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
				stCrossHeapDesc.Properties.CreationNodeMask = 0;
				stCrossHeapDesc.Properties.VisibleNodeMask = 0;

				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateHeap(&stCrossHeapDesc
					, IID_PPV_ARGS(&stGPUParams[nIDGPUMain].m_pICrossAdapterHeap)));

				// �����Կ��ϵõ�����ѵľ��
				HANDLE hHeap = nullptr;
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateSharedHandle(
					stGPUParams[nIDGPUMain].m_pICrossAdapterHeap.Get(),
					nullptr,
					GENERIC_ALL,
					nullptr,
					&hHeap));

				// �ڵڶ����Կ��ϴ򿪾����ʵ�ʾ����������Դ�ѵĹ���
				HRESULT hrOpenSharedHandle = stGPUParams[nIDGPUSecondary].m_pID3D12Device4->OpenSharedHandle(hHeap
					, IID_PPV_ARGS(&stGPUParams[nIDGPUSecondary].m_pICrossAdapterHeap));

				// �ȹرվ�������ж��Ƿ���ɹ�
				::CloseHandle(hHeap);

				GRS_THROW_IF_FAILED(hrOpenSharedHandle);

				// �Զ�λ��ʽ�ڹ�����ϴ���ÿ���Կ��ϵ���Դ
				for ( UINT nFrameIndex = 0; nFrameIndex < g_nFrameBackBufCount; nFrameIndex++ )
				{
					GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreatePlacedResource(
						stGPUParams[nIDGPUMain].m_pICrossAdapterHeap.Get(),
						n64szTexture * nFrameIndex,
						&stCrossAdapterResDesc,
						D3D12_RESOURCE_STATE_COPY_DEST,
						nullptr,
						IID_PPV_ARGS(&stGPUParams[nIDGPUMain].m_pICrossAdapterResPerFrame[nFrameIndex])));

					GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3D12Device4->CreatePlacedResource(
						stGPUParams[nIDGPUSecondary].m_pICrossAdapterHeap.Get(),
						n64szTexture * nFrameIndex,
						&stCrossAdapterResDesc,
						bCrossAdapterTextureSupport ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COPY_SOURCE,
						nullptr,
						IID_PPV_ARGS(&stGPUParams[nIDGPUSecondary].m_pICrossAdapterResPerFrame[nFrameIndex])));

					if (!bCrossAdapterTextureSupport)
					{
						// ���������������ȾĿ�������Ϊ������������ô����һ��������Դ���临�Ƶ������������ϡ�
						GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3D12Device4->CreateCommittedResource(
							&stDefautHeapProps,
							D3D12_HEAP_FLAG_NONE,
							&stRenderTargetDesc,
							D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
							nullptr,
							IID_PPV_ARGS(&pISecondaryAdapterTexutrePerFrame[nFrameIndex])));
					}
				}
			}
		}

		// ������ǩ��
		if(TRUE)
		{ //��������У���������ʹ����ͬ�ĸ�ǩ������Ϊ��Ⱦ��������Ҫ�Ĳ�����һ����
			D3D12_FEATURE_DATA_ROOT_SIGNATURE stFeatureData = {};

			// ����Ƿ�֧��V1.1�汾�ĸ�ǩ��
			stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
			if (FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE
				, &stFeatureData, sizeof(stFeatureData))))
			{// 1.0�� ֱ�Ӷ��쳣�˳���
				GRS_THROW_IF_FAILED(E_NOTIMPL);
			}

			D3D12_DESCRIPTOR_RANGE1 stDSPRanges[3] = {};

			stDSPRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
			stDSPRanges[0].NumDescriptors = 1;
			stDSPRanges[0].BaseShaderRegister = 0;
			stDSPRanges[0].RegisterSpace = 0;
			stDSPRanges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
			stDSPRanges[0].OffsetInDescriptorsFromTableStart = 0;

			stDSPRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			stDSPRanges[1].NumDescriptors = 1;
			stDSPRanges[1].BaseShaderRegister = 0;
			stDSPRanges[1].RegisterSpace = 0;
			stDSPRanges[1].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
			stDSPRanges[1].OffsetInDescriptorsFromTableStart = 0;

			stDSPRanges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
			stDSPRanges[2].NumDescriptors = 1;
			stDSPRanges[2].BaseShaderRegister = 0;
			stDSPRanges[2].RegisterSpace = 0;
			stDSPRanges[2].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
			stDSPRanges[2].OffsetInDescriptorsFromTableStart = 0;

			D3D12_ROOT_PARAMETER1 stRootParameters[3] = {};

			stRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			stRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			stRootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
			stRootParameters[0].DescriptorTable.pDescriptorRanges = &stDSPRanges[0];

			stRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			stRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			stRootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
			stRootParameters[1].DescriptorTable.pDescriptorRanges = &stDSPRanges[1];

			stRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			stRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
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

			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateRootSignature(0
				, pISignatureBlob->GetBufferPointer()
				, pISignatureBlob->GetBufferSize()
				, IID_PPV_ARGS(&pIRSMain)));

			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRSMain);
			//------------------------------------------------------------------------------------------------

			// ������ȾQuad�ĸ�ǩ������ע�������ǩ���ڸ����Կ�����
			stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
			if (FAILED(stGPUParams[nIDGPUSecondary].m_pID3D12Device4->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &stFeatureData, sizeof(stFeatureData))))
			{
				stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
			}

			D3D12_DESCRIPTOR_RANGE1 stDRQuad[2] = {};
			stDRQuad[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			stDRQuad[0].NumDescriptors = 1;
			stDRQuad[0].BaseShaderRegister = 0;
			stDRQuad[0].RegisterSpace = 0;
			stDRQuad[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
			stDRQuad[0].OffsetInDescriptorsFromTableStart = 0;

			stDRQuad[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
			stDRQuad[1].NumDescriptors = 1;
			stDRQuad[1].BaseShaderRegister = 0;
			stDRQuad[1].RegisterSpace = 0;
			stDRQuad[1].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
			stDRQuad[1].OffsetInDescriptorsFromTableStart = 0;

			D3D12_ROOT_PARAMETER1 stRPQuad[2] = {};
			stRPQuad[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			stRPQuad[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; //SRV��PS�ɼ�
			stRPQuad[0].DescriptorTable.NumDescriptorRanges = 1;
			stRPQuad[0].DescriptorTable.pDescriptorRanges = &stDRQuad[0];

			stRPQuad[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			stRPQuad[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; //SAMPLE��PS�ɼ�
			stRPQuad[1].DescriptorTable.NumDescriptorRanges = 1;
			stRPQuad[1].DescriptorTable.pDescriptorRanges = &stDRQuad[1];

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

			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3D12Device4->CreateRootSignature(0
				, pISignatureBlob->GetBufferPointer()
				, pISignatureBlob->GetBufferSize()
				, IID_PPV_ARGS(&pIRSQuad)));

			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRSQuad);
		}

		// ����Shader������Ⱦ����״̬����
		if(TRUE)
		{
			UINT nShaderCompileFlags = 0;
#if defined(_DEBUG)
			// Enable better shader debugging with the graphics debugging tools.
			nShaderCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
			//����Ϊ�о�����ʽ	   
			nShaderCompileFlags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

			TCHAR pszShaderFileName[MAX_PATH] = {};
			StringCchPrintf(pszShaderFileName, MAX_PATH, _T("%s7-D3D12MultiAdapter\\Shader\\TextureModule.hlsl"), pszAppPath);

			GRS_THROW_IF_FAILED(D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
				, "VSMain", "vs_5_0", nShaderCompileFlags, 0, &pIVSMain, nullptr));
			GRS_THROW_IF_FAILED(D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
				, "PSMain", "ps_5_0", nShaderCompileFlags, 0, &pIPSMain, nullptr));

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
			stPSODesc.pRootSignature = pIRSMain.Get();
			stPSODesc.VS.BytecodeLength = pIVSMain->GetBufferSize();
			stPSODesc.VS.pShaderBytecode = pIVSMain->GetBufferPointer();
			stPSODesc.PS.BytecodeLength = pIPSMain->GetBufferSize();
			stPSODesc.PS.pShaderBytecode = pIPSMain->GetBufferPointer();

			stPSODesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
			stPSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;

			stPSODesc.BlendState.AlphaToCoverageEnable = FALSE;
			stPSODesc.BlendState.IndependentBlendEnable = FALSE;
			stPSODesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

			stPSODesc.SampleMask = UINT_MAX;
			stPSODesc.SampleDesc.Count = 1;
			stPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			stPSODesc.NumRenderTargets = 1;
			stPSODesc.RTVFormats[0] = emRTFmt;
			stPSODesc.DepthStencilState.StencilEnable = FALSE;
			stPSODesc.DepthStencilState.DepthEnable = TRUE;			//����Ȼ���		
			stPSODesc.DSVFormat = emDSFmt;
			stPSODesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;//������Ȼ���д�빦��
			stPSODesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;     //��Ȳ��Ժ�������ֵΪ��ͨ����Ȳ��ԣ�����Сֵд�룩				

			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateGraphicsPipelineState(&stPSODesc
				, IID_PPV_ARGS(&pIPSOMain)));

			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIPSOMain);

			//---------------------------------------------------------------------------------------------------
			//������Ⱦ��λ���εĹܵ�״̬����
			TCHAR pszUnitQuadShader[MAX_PATH] = {};
			StringCchPrintf(pszUnitQuadShader, MAX_PATH, _T("%s7-D3D12MultiAdapter\\Shader\\UnitQuad.hlsl"), pszAppPath);

			GRS_THROW_IF_FAILED(D3DCompileFromFile(pszUnitQuadShader, nullptr, nullptr
				, "VSMain", "vs_5_0", nShaderCompileFlags, 0, &pIVSQuad, nullptr));
			GRS_THROW_IF_FAILED(D3DCompileFromFile(pszUnitQuadShader, nullptr, nullptr
				, "PSMain", "ps_5_0", nShaderCompileFlags, 0, &pIPSQuad, nullptr));

			// ���Ƕ������һ�����ߵĶ��壬��ĿǰShader�����ǲ�û��ʹ��
			D3D12_INPUT_ELEMENT_DESC stIALayoutQuad[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
			};

			// ���� graphics pipeline state object (PSO)����
			D3D12_GRAPHICS_PIPELINE_STATE_DESC stPSOQuadDesc = {};
			stPSOQuadDesc.InputLayout = { stIALayoutQuad, _countof(stIALayoutQuad) };
			stPSOQuadDesc.pRootSignature = pIRSQuad.Get();
			stPSOQuadDesc.VS.BytecodeLength = pIVSQuad->GetBufferSize();
			stPSOQuadDesc.VS.pShaderBytecode = pIVSQuad->GetBufferPointer();
			stPSOQuadDesc.PS.BytecodeLength = pIPSQuad->GetBufferSize();
			stPSOQuadDesc.PS.pShaderBytecode = pIPSQuad->GetBufferPointer();

			stPSOQuadDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
			stPSOQuadDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;

			stPSOQuadDesc.BlendState.AlphaToCoverageEnable = FALSE;
			stPSOQuadDesc.BlendState.IndependentBlendEnable = FALSE;
			stPSOQuadDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

			stPSOQuadDesc.SampleMask = UINT_MAX;
			stPSOQuadDesc.SampleDesc.Count = 1;
			stPSOQuadDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			stPSOQuadDesc.NumRenderTargets = 1;
			stPSOQuadDesc.RTVFormats[0] = emRTFmt;
			stPSOQuadDesc.DepthStencilState.StencilEnable = FALSE;
			stPSOQuadDesc.DepthStencilState.DepthEnable = FALSE;

			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3D12Device4->CreateGraphicsPipelineState(&stPSOQuadDesc
				, IID_PPV_ARGS(&pIPSOQuad)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIPSOQuad);
		}

		// ׼������������
		if(TRUE) 
		{
			USES_CONVERSION;
			// ������Բ���
			StringCchPrintf(stModuleParams[nSphere].pszDDSFile, MAX_PATH, _T("%sAssets\\sphere.dds"), pszAppPath);
			StringCchPrintfA(stModuleParams[nSphere].pszMeshFile, MAX_PATH, "%sAssets\\sphere.txt", T2A(pszAppPath));
			stModuleParams[nSphere].v4ModelPos = XMFLOAT4(2.0f, 2.0f, 0.0f, 1.0f);

			// ��������Բ���
			StringCchPrintf(stModuleParams[nCube].pszDDSFile, MAX_PATH, _T("%sAssets\\Cube.dds"), pszAppPath);
			StringCchPrintfA(stModuleParams[nCube].pszMeshFile, MAX_PATH, "%sAssets\\Cube.txt", T2A(pszAppPath));
			stModuleParams[nCube].v4ModelPos = XMFLOAT4(-2.0f, 2.0f, 0.0f, 1.0f);

			// ƽ����Բ���
			StringCchPrintf(stModuleParams[nPlane].pszDDSFile, MAX_PATH, _T("%sAssets\\Plane.dds"), pszAppPath);
			StringCchPrintfA(stModuleParams[nPlane].pszMeshFile, MAX_PATH, "%sAssets\\Plane.txt", T2A(pszAppPath));
			stModuleParams[nPlane].v4ModelPos = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);

			// Mesh Value
			ST_GRS_VERTEX_MODULE* pstVertices = nullptr;
			UINT* pnIndices = nullptr;
			// DDS Value
			std::unique_ptr<uint8_t[]>			pbDDSData;
			std::vector<D3D12_SUBRESOURCE_DATA> stArSubResources;
			DDS_ALPHA_MODE						emAlphaMode = DDS_ALPHA_MODE_UNKNOWN;
			bool								bIsCube = false;

			// ��������Ĺ��Բ�����Ҳ���Ǹ��̵߳Ĺ��Բ���
			for (int i = 0; i < nMaxObject; i++)
			{
				stModuleParams[i].nIndex = i;

				//����ÿ��Module�������
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE
					, IID_PPV_ARGS(&stModuleParams[i].pIBundleAlloc)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pIBundleAlloc.Get(), _T("pIBundleAlloc"), i);

				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE
					, stModuleParams[i].pIBundleAlloc.Get(), nullptr, IID_PPV_ARGS(&stModuleParams[i].pIBundle)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pIBundle.Get(), _T("pIBundle"), i);

				//����DDS
				pbDDSData.release();
				stArSubResources.clear();

				GRS_THROW_IF_FAILED(LoadDDSTextureFromFile(stGPUParams[nIDGPUMain].m_pID3D12Device4.Get()
					, stModuleParams[i].pszDDSFile
					, stModuleParams[i].pITexture.GetAddressOf()
					, pbDDSData
					, stArSubResources
					, SIZE_MAX
					, &emAlphaMode
					, &bIsCube));

				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pITexture.Get(), _T("pITexture"), i);

				D3D12_RESOURCE_DESC stTXDesc = stModuleParams[i].pITexture->GetDesc();
				UINT64 n64szUpSphere = 0;
				stGPUParams[nIDGPUMain].m_pID3D12Device4->GetCopyableFootprints(&stTXDesc, 0, static_cast<UINT>(stArSubResources.size())
					, 0, nullptr, nullptr, nullptr, &n64szUpSphere);

				D3D12_RESOURCE_DESC stUploadBufDesc = {};
				stUploadBufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
				stUploadBufDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
				stUploadBufDesc.Width = n64szUpSphere;
				stUploadBufDesc.Height = 1;
				stUploadBufDesc.DepthOrArraySize = 1;
				stUploadBufDesc.MipLevels = 1;
				stUploadBufDesc.Format = DXGI_FORMAT_UNKNOWN;
				stUploadBufDesc.SampleDesc.Count = 1;
				stUploadBufDesc.SampleDesc.Quality = 0;
				stUploadBufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
				stUploadBufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

				//�����ϴ���
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateCommittedResource(
					&stUploadHeapProps
					, D3D12_HEAP_FLAG_NONE
					, &stUploadBufDesc
					, D3D12_RESOURCE_STATE_GENERIC_READ
					, nullptr
					, IID_PPV_ARGS(&stModuleParams[i].pITextureUpload)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pITextureUpload.Get(), _T("pITextureUpload"), i);

				//�ϴ�DDS
				UINT nFirstSubresource = 0;
				UINT nNumSubresources = static_cast<UINT>(stArSubResources.size());
				D3D12_RESOURCE_DESC stUploadResDesc = stModuleParams[i].pITextureUpload->GetDesc();
				D3D12_RESOURCE_DESC stDefaultResDesc = stModuleParams[i].pITexture->GetDesc();

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
				stGPUParams[nIDGPUMain].m_pID3D12Device4->GetCopyableFootprints(&stDefaultResDesc, nFirstSubresource, nNumSubresources, 0, pLayouts, pNumRows, pRowSizesInBytes, &n64RequiredSize);

				BYTE* pData = nullptr;
				GRS_THROW_IF_FAILED(stModuleParams[i].pITextureUpload->Map(0, nullptr, reinterpret_cast<void**>(&pData)));

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
				stModuleParams[i].pITextureUpload->Unmap(0, nullptr);

				// �ڶ���Copy��
				if (stDefaultResDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
				{// Buffer һ���Ը��ƾͿ����ˣ���Ϊû���ж�����д�С��һ�µ����⣬Buffer����������1
					stGPUParams[nIDGPUMain].m_pICmdList->CopyBufferRegion(
						stModuleParams[i].pITexture.Get(), 0, stModuleParams[i].pITextureUpload.Get(), pLayouts[0].Offset, pLayouts[0].Footprint.Width);
				}
				else
				{
					for (UINT nSubRes = 0; nSubRes < nNumSubresources; ++nSubRes)
					{
						D3D12_TEXTURE_COPY_LOCATION stDstCopyLocation = {};
						stDstCopyLocation.pResource = stModuleParams[i].pITexture.Get();
						stDstCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
						stDstCopyLocation.SubresourceIndex = nSubRes;

						D3D12_TEXTURE_COPY_LOCATION stSrcCopyLocation = {};
						stSrcCopyLocation.pResource = stModuleParams[i].pITextureUpload.Get();
						stSrcCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
						stSrcCopyLocation.PlacedFootprint = pLayouts[nSubRes];

						stGPUParams[nIDGPUMain].m_pICmdList->CopyTextureRegion(&stDstCopyLocation, 0, 0, 0, &stSrcCopyLocation, nullptr);
					}
				}

				GRS_SAFE_FREE(pMem);

				//ͬ��
				D3D12_RESOURCE_BARRIER stUploadTransResBarrier = {};
				stUploadTransResBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				stUploadTransResBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				stUploadTransResBarrier.Transition.pResource = stModuleParams[i].pITexture.Get();
				stUploadTransResBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
				stUploadTransResBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
				stUploadTransResBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

				stGPUParams[nIDGPUMain].m_pICmdList->ResourceBarrier(1, &stUploadTransResBarrier);


				//����SRV CBV��
				D3D12_DESCRIPTOR_HEAP_DESC stSRVHeapDesc = {};
				stSRVHeapDesc.NumDescriptors = 2; //1 SRV + 1 CBV
				stSRVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
				stSRVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateDescriptorHeap(&stSRVHeapDesc
					, IID_PPV_ARGS(&stModuleParams[i].pISRVCBVHp)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pISRVCBVHp.Get(), _T("pISRVCBVHp"), i);

				//����SRV
				D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
				stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				stSRVDesc.Format = stTXDesc.Format;
				stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				stSRVDesc.Texture2D.MipLevels = 1;

				D3D12_CPU_DESCRIPTOR_HANDLE stCBVSRVHandle = stModuleParams[i].pISRVCBVHp->GetCPUDescriptorHandleForHeapStart();
				stCBVSRVHandle.ptr += stGPUParams[nIDGPUMain].m_nszSRVCBVUAV;

				stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateShaderResourceView(stModuleParams[i].pITexture.Get()
					, &stSRVDesc
					, stCBVSRVHandle);

				// ����Sample
				D3D12_DESCRIPTOR_HEAP_DESC stSamplerHeapDesc = {};
				stSamplerHeapDesc.NumDescriptors = 1;
				stSamplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
				stSamplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateDescriptorHeap(&stSamplerHeapDesc
					, IID_PPV_ARGS(&stModuleParams[i].pISampleHp)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pISampleHp.Get(), _T("pISampleHp"), i);

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

				stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateSampler(&stSamplerDesc
					, stModuleParams[i].pISampleHp->GetCPUDescriptorHandleForHeapStart());

				//������������
				LoadMeshVertex(stModuleParams[i].pszMeshFile, stModuleParams[i].nVertexCnt, pstVertices, pnIndices);
				stModuleParams[i].nIndexCnt = stModuleParams[i].nVertexCnt;

				//���� Vertex Buffer ��ʹ��Upload��ʽ��
				stUploadBufDesc.Width = stModuleParams[i].nVertexCnt * sizeof(ST_GRS_VERTEX_MODULE);
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateCommittedResource(
					&stUploadHeapProps
					, D3D12_HEAP_FLAG_NONE
					, &stUploadBufDesc
					, D3D12_RESOURCE_STATE_GENERIC_READ
					, nullptr
					, IID_PPV_ARGS(&stModuleParams[i].pIVertexBuffer)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pIVertexBuffer.Get(), _T("pIVertexBuffer"), i);

				//ʹ��map-memcpy-unmap�󷨽����ݴ������㻺�����
				UINT8* pVertexDataBegin = nullptr;
				D3D12_RANGE stReadRange = { 0, 0 };		// We do not intend to read from this resource on the CPU.

				GRS_THROW_IF_FAILED(stModuleParams[i].pIVertexBuffer->Map(0, &stReadRange, reinterpret_cast<void**>(&pVertexDataBegin)));
				memcpy(pVertexDataBegin, pstVertices, stModuleParams[i].nVertexCnt * sizeof(ST_GRS_VERTEX_MODULE));
				stModuleParams[i].pIVertexBuffer->Unmap(0, nullptr);

				//���� Index Buffer ��ʹ��Upload��ʽ��
				stUploadBufDesc.Width = stModuleParams[i].nIndexCnt * sizeof(UINT);
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateCommittedResource(
					&stUploadHeapProps
					, D3D12_HEAP_FLAG_NONE
					, &stUploadBufDesc
					, D3D12_RESOURCE_STATE_GENERIC_READ
					, nullptr
					, IID_PPV_ARGS(&stModuleParams[i].pIIndexsBuffer)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pIIndexsBuffer.Get(), _T("pIIndexsBuffer"), i);

				UINT8* pIndexDataBegin = nullptr;
				GRS_THROW_IF_FAILED(stModuleParams[i].pIIndexsBuffer->Map(0, &stReadRange, reinterpret_cast<void**>(&pIndexDataBegin)));
				memcpy(pIndexDataBegin, pnIndices, stModuleParams[i].nIndexCnt * sizeof(UINT));
				stModuleParams[i].pIIndexsBuffer->Unmap(0, nullptr);

				//����Vertex Buffer View
				stModuleParams[i].stVertexBufferView.BufferLocation = stModuleParams[i].pIVertexBuffer->GetGPUVirtualAddress();
				stModuleParams[i].stVertexBufferView.StrideInBytes = sizeof(ST_GRS_VERTEX_MODULE);
				stModuleParams[i].stVertexBufferView.SizeInBytes = stModuleParams[i].nVertexCnt * sizeof(ST_GRS_VERTEX_MODULE);

				//����Index Buffer View
				stModuleParams[i].stIndexsBufferView.BufferLocation = stModuleParams[i].pIIndexsBuffer->GetGPUVirtualAddress();
				stModuleParams[i].stIndexsBufferView.Format = DXGI_FORMAT_R32_UINT;
				stModuleParams[i].stIndexsBufferView.SizeInBytes = stModuleParams[i].nIndexCnt * sizeof(UINT);

				GRS_SAFE_FREE(pstVertices);
				GRS_SAFE_FREE(pnIndices);

				// ������������ ע�⻺��ߴ�����Ϊ256�߽�����С
				stUploadBufDesc.Width = szMVPBuf;
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateCommittedResource(
					&stUploadHeapProps
					, D3D12_HEAP_FLAG_NONE
					, &stUploadBufDesc
					, D3D12_RESOURCE_STATE_GENERIC_READ
					, nullptr
					, IID_PPV_ARGS(&stModuleParams[i].pIConstBuffer)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pIConstBuffer.Get(), _T("pIConstBuffer"), i);
				// Map ֮��Ͳ���Unmap�� ֱ�Ӹ������ݽ�ȥ ����ÿ֡������map-copy-unmap�˷�ʱ����
				GRS_THROW_IF_FAILED(stModuleParams[i].pIConstBuffer->Map(0, nullptr, reinterpret_cast<void**>(&stModuleParams[i].pMVPBuf)));
				//---------------------------------------------------------------------------------------------

				// ����CBV
				D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
				cbvDesc.BufferLocation = stModuleParams[i].pIConstBuffer->GetGPUVirtualAddress();
				cbvDesc.SizeInBytes = static_cast<UINT>(szMVPBuf);
				stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateConstantBufferView(&cbvDesc, stModuleParams[i].pISRVCBVHp->GetCPUDescriptorHandleForHeapStart());
			}
		}

		// ��¼����������Ⱦ�������
		{
			{
				//	������Ⱦ���������һ����
				for (int i = 0; i < nMaxObject; i++)
				{
					stModuleParams[i].pIBundle->SetGraphicsRootSignature(pIRSMain.Get());
					stModuleParams[i].pIBundle->SetPipelineState(pIPSOMain.Get());

					ID3D12DescriptorHeap* ppHeapsSphere[] = { stModuleParams[i].pISRVCBVHp.Get(),stModuleParams[i].pISampleHp.Get() };
					stModuleParams[i].pIBundle->SetDescriptorHeaps(_countof(ppHeapsSphere), ppHeapsSphere);

					//����SRV
					D3D12_GPU_DESCRIPTOR_HANDLE stGPUCBVHandleSphere = stModuleParams[i].pISRVCBVHp->GetGPUDescriptorHandleForHeapStart();
					stModuleParams[i].pIBundle->SetGraphicsRootDescriptorTable(0, stGPUCBVHandleSphere);

					stGPUCBVHandleSphere.ptr += stGPUParams[nIDGPUMain].m_nszSRVCBVUAV;
					//����CBV
					stModuleParams[i].pIBundle->SetGraphicsRootDescriptorTable(1, stGPUCBVHandleSphere);

					//����Sample
					stModuleParams[i].pIBundle->SetGraphicsRootDescriptorTable(2, stModuleParams[i].pISampleHp->GetGPUDescriptorHandleForHeapStart());

					//ע������ʹ�õ���Ⱦ�ַ����������б�Ҳ����ͨ����Mesh����
					stModuleParams[i].pIBundle->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
					stModuleParams[i].pIBundle->IASetVertexBuffers(0, 1, &stModuleParams[i].stVertexBufferView);
					stModuleParams[i].pIBundle->IASetIndexBuffer(&stModuleParams[i].stIndexsBufferView);

					//Draw Call������
					stModuleParams[i].pIBundle->DrawIndexedInstanced(stModuleParams[i].nIndexCnt, 1, 0, 0, 0);
					stModuleParams[i].pIBundle->Close();
				}
				//End for
			}
		}

		// �������Կ�������Ⱦ������õľ��ο�
		{
			ST_GRS_VERTEX_QUAD stVertexQuad[] =
			{	//(   x,     y,    z,    w   )  (  u,    v   )
				{ { -1.0f,  1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } },	// Top left.
				{ {  1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },	// Top right.
				{ { -1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },	// Bottom left.
				{ {  1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },	// Bottom right.
			};

			const UINT nszVBQuad = sizeof(stVertexQuad);


			D3D12_RESOURCE_DESC stBufferDesc = {};
			stBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			stBufferDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
			stBufferDesc.Width = nszVBQuad;
			stBufferDesc.Height = 1;
			stBufferDesc.DepthOrArraySize = 1;
			stBufferDesc.MipLevels = 1;
			stBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
			stBufferDesc.SampleDesc.Count = 1;
			stBufferDesc.SampleDesc.Quality = 0;
			stBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			stBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3D12Device4->CreateCommittedResource(
				&stDefautHeapProps
				, D3D12_HEAP_FLAG_NONE
				, &stBufferDesc
				, D3D12_RESOURCE_STATE_COPY_DEST
				, nullptr
				, IID_PPV_ARGS(&pIVBQuad)));

			// �������������ʾ����ν����㻺���ϴ���Ĭ�϶��ϵķ�ʽ���������ϴ�Ĭ�϶�ʵ����һ����
			stBufferDesc.Width = nszVBQuad;
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3D12Device4->CreateCommittedResource(
				&stUploadHeapProps
				, D3D12_HEAP_FLAG_NONE
				, &stBufferDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&pIVBQuadUpload)));

			D3D12_PLACED_SUBRESOURCE_FOOTPRINT stTxtLayouts = {};
			D3D12_RESOURCE_DESC stDestDesc = pIVBQuad->GetDesc();
			UINT64 n64RequiredSize = 0u;
			UINT   nNumSubresources = 1u;  //����ֻ��һ��ͼƬ��������Դ����Ϊ1
			UINT64 n64TextureRowSizes = 0u;
			UINT   nTextureRowNum = 0u;

			stGPUParams[nIDGPUSecondary].m_pID3D12Device4->GetCopyableFootprints(&stDestDesc
				, 0
				, nNumSubresources
				, 0
				, &stTxtLayouts
				, &nTextureRowNum
				, &n64TextureRowSizes
				, &n64RequiredSize);

			BYTE* pData = nullptr;
			GRS_THROW_IF_FAILED(pIVBQuadUpload->Map(0, NULL, reinterpret_cast<void**>(&pData)));

			BYTE* pDestSlice = reinterpret_cast<BYTE*>(pData) + stTxtLayouts.Offset;
			const BYTE* pSrcSlice = reinterpret_cast<const BYTE*>(stVertexQuad);
			for (UINT y = 0; y < nTextureRowNum; ++y)
			{
				memcpy(pDestSlice + static_cast<SIZE_T>(stTxtLayouts.Footprint.RowPitch) * y
					, pSrcSlice + static_cast<SIZE_T>(nszVBQuad) * y
					, nszVBQuad);
			}
			//ȡ��ӳ�� �����ױ��������ÿ֡�ı任��������ݣ�������������Unmap�ˣ�
			//������פ�ڴ�,������������ܣ���Ϊÿ��Map��Unmap�Ǻܺ�ʱ�Ĳ���
			//��Ϊ�������붼��64λϵͳ��Ӧ���ˣ���ַ�ռ����㹻�ģ�������ռ�ò���Ӱ��ʲô
			pIVBQuadUpload->Unmap(0, NULL);

			//��������з������ϴ��Ѹ����������ݵ�Ĭ�϶ѵ�����
			D3D12_TEXTURE_COPY_LOCATION stDst = {};
			stDst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			stDst.pResource = pIVBQuad.Get();
			stDst.SubresourceIndex = 0;

			D3D12_TEXTURE_COPY_LOCATION stSrc = {};
			stSrc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			stSrc.pResource = pIVBQuadUpload.Get();
			stSrc.PlacedFootprint = stTxtLayouts;

			stGPUParams[nIDGPUSecondary].m_pICmdList->CopyBufferRegion(pIVBQuad.Get(), 0, pIVBQuadUpload.Get(), 0, nszVBQuad);

			D3D12_RESOURCE_BARRIER stUploadTransResBarrier = {};

			stUploadTransResBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			stUploadTransResBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			stUploadTransResBarrier.Transition.pResource = pIVBQuad.Get();
			stUploadTransResBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
			stUploadTransResBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
			stUploadTransResBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

			stGPUParams[nIDGPUSecondary].m_pICmdList->ResourceBarrier(1,&stUploadTransResBarrier);

			pstVBVQuad.BufferLocation = pIVBQuad->GetGPUVirtualAddress();
			pstVBVQuad.StrideInBytes = sizeof(ST_GRS_VERTEX_QUAD);
			pstVBVQuad.SizeInBytes = sizeof(stVertexQuad);

			// ִ����������ζ��㻺���ϴ����ڶ����Կ���Ĭ�϶���
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pICmdList->Close());
			ID3D12CommandList* ppRenderCommandLists[] = { stGPUParams[nIDGPUSecondary].m_pICmdList.Get() };
			stGPUParams[nIDGPUSecondary].m_pICmdQueue->ExecuteCommandLists(_countof(ppRenderCommandLists), ppRenderCommandLists);

			n64CurrentFenceValue = n64FenceValue;
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pICmdQueue->Signal(stGPUParams[nIDGPUSecondary].m_pIFence.Get(), n64CurrentFenceValue));
			n64FenceValue++;
			if (stGPUParams[nIDGPUSecondary].m_pIFence->GetCompletedValue() < n64CurrentFenceValue)
			{
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pIFence->SetEventOnCompletion(n64CurrentFenceValue, hEventFence));
				WaitForSingleObject(hEventFence, INFINITE);
			}
		}

		// �����ڸ����Կ�����Ⱦ��λ�����õ�SRV�Ѻ�Sample��
		{
			D3D12_DESCRIPTOR_HEAP_DESC stHeapDesc = {};
			stHeapDesc.NumDescriptors = g_nFrameBackBufCount;
			stHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			stHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			//SRV��
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3D12Device4->CreateDescriptorHeap(&stHeapDesc, IID_PPV_ARGS(&pIDHSRVSecondary)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDHSRVSecondary);

			// SRV
			D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
			stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			stSRVDesc.Format = emRTFmt;
			stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			stSRVDesc.Texture2D.MipLevels = 1;

			D3D12_CPU_DESCRIPTOR_HANDLE stSrvHandle = pIDHSRVSecondary->GetCPUDescriptorHandleForHeapStart();

			for (int i = 0; i < g_nFrameBackBufCount; i++)
			{
				if (!bCrossAdapterTextureSupport)
				{
					// ʹ�ø��Ƶ���ȾĿ��������ΪShader��������Դ
					stGPUParams[nIDGPUSecondary].m_pID3D12Device4->CreateShaderResourceView(
						pISecondaryAdapterTexutrePerFrame[i].Get()
						, &stSRVDesc
						, stSrvHandle);
				}
				else
				{
					// ֱ��ʹ�ù������ȾĿ�긴�Ƶ�������Ϊ������Դ
					stGPUParams[nIDGPUSecondary].m_pID3D12Device4->CreateShaderResourceView(
						stGPUParams[nIDGPUSecondary].m_pICrossAdapterResPerFrame[i].Get()
						, &stSRVDesc
						, stSrvHandle);
				}
				stSrvHandle.ptr += stGPUParams[nIDGPUSecondary].m_nszSRVCBVUAV;
			}


			// ����Sample Descriptor Heap ��������������
			stHeapDesc.NumDescriptors = 1;  //ֻ��һ��Sample
			stHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3D12Device4->CreateDescriptorHeap(&stHeapDesc, IID_PPV_ARGS(&pIDHSampleSecondary)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDHSampleSecondary);

			// ����Sample View ʵ�ʾ��ǲ�����
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

			stGPUParams[nIDGPUSecondary].m_pID3D12Device4->CreateSampler(&stSamplerDesc, pIDHSampleSecondary->GetCPUDescriptorHandleForHeapStart());
		}

		// ִ�еڶ���Copy����ȴ����е������ϴ�����Ĭ�϶���
		{
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pICmdList->Close());
			ID3D12CommandList* ppRenderCommandLists[] = { stGPUParams[nIDGPUMain].m_pICmdList.Get() };
			stGPUParams[nIDGPUMain].m_pICmdQueue->ExecuteCommandLists(_countof(ppRenderCommandLists), ppRenderCommandLists);

			n64CurrentFenceValue = n64FenceValue;
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pICmdQueue->Signal(stGPUParams[nIDGPUMain].m_pIFence.Get(), n64CurrentFenceValue));
			n64FenceValue++;
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pIFence->SetEventOnCompletion(n64CurrentFenceValue, hEventFence));

		}

		GRS_THROW_IF_FAILED(pICmdListCopy->Close());

		D3D12_RESOURCE_BARRIER stRTVStateTransBarrier = {};

		stRTVStateTransBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		stRTVStateTransBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		stRTVStateTransBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		// Time Value ʱ�������
		ULONGLONG n64tmFrameStart = ::GetTickCount64();
		ULONGLONG n64tmCurrent = n64tmFrameStart;
		// ������ת�Ƕ���Ҫ�ı���
		double dModelRotationYAngle = 0.0f;

		XMMATRIX mxRotY;
		// ����ͶӰ����
		XMMATRIX mxProjection = XMMatrixPerspectiveFovLH(XM_PIDIV4, (FLOAT)nWndWidth / (FLOAT)nWndHeight, 0.1f, 1000.0f);

		DWORD dwRet = 0;
		BOOL bExit = FALSE;

		ShowWindow(hWnd, nCmdShow);
		UpdateWindow(hWnd);

		//17����ʼ��Ϣѭ�����������в�����Ⱦ
		while (!bExit)
		{//��Ϣѭ�������ȴ���ʱֵ����Ϊ0��ͬʱ����ʱ�Ե���Ⱦ���ĳ���ÿ��ѭ������Ⱦ
			dwRet = ::MsgWaitForMultipleObjects(1, &hEventFence, FALSE, INFINITE, QS_ALLINPUT);
			switch (dwRet - WAIT_OBJECT_0)
			{
			case 0:
			{//��ʼ�µ�һ֡��Ⱦ
				//OnUpdate()
				{//���� Module ģ�;��� * �Ӿ��� view * �ü����� projection
					n64tmCurrent = ::GetTickCount();
					//������ת�ĽǶȣ���ת�Ƕ�(����) = ʱ��(��) * ���ٶ�(����/��)
					dModelRotationYAngle += ((n64tmCurrent - n64tmFrameStart) / 1000.0f) * g_fPalstance;

					n64tmFrameStart = n64tmCurrent;

					//��ת�Ƕ���2PI���ڵı�����ȥ����������ֻ�������0���ȿ�ʼ��С��2PI�Ļ��ȼ���
					if (dModelRotationYAngle > XM_2PI)
					{
						dModelRotationYAngle = fmod(dModelRotationYAngle, XM_2PI);
					}
					mxRotY = XMMatrixRotationY(static_cast<float>(dModelRotationYAngle));

					//ע��ʵ�ʵı任Ӧ���� Module * World * View * Projection
					//�˴�ʵ��World��һ����λ���󣬹�ʡ����
					//��Module��������λ������ת��Ҳ���Խ���ת�������Ϊ����World����
					XMMATRIX xmMVP = XMMatrixMultiply(XMMatrixLookAtLH(XMLoadFloat3(&g_f3EyePos)
						, XMLoadFloat3(&g_f3LockAt)
						, XMLoadFloat3(&g_f3HeapUp))
						, mxProjection);

					//���������MVP
					xmMVP = XMMatrixMultiply(mxRotY, xmMVP);

					for (int i = 0; i < nMaxObject; i++)
					{
						XMMATRIX xmModulePos = XMMatrixTranslation(stModuleParams[i].v4ModelPos.x
							, stModuleParams[i].v4ModelPos.y
							, stModuleParams[i].v4ModelPos.z);

						xmModulePos = XMMatrixMultiply(xmModulePos, xmMVP);

						XMStoreFloat4x4(&stModuleParams[i].pMVPBuf->m_MVP, xmModulePos);
					}
				}

				//OnRender()
				{
					// ��ȡ֡��
					nCurrentFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

					stGPUParams[nIDGPUMain].m_pICmdAllocPerFrame[nCurrentFrameIndex]->Reset();
					stGPUParams[nIDGPUMain].m_pICmdList->Reset(stGPUParams[nIDGPUMain].m_pICmdAllocPerFrame[nCurrentFrameIndex].Get(), pIPSOMain.Get());

					pICmdAllocCopyPerFrame[nCurrentFrameIndex]->Reset();
					pICmdListCopy->Reset(pICmdAllocCopyPerFrame[nCurrentFrameIndex].Get(), pIPSOMain.Get());

					stGPUParams[nIDGPUSecondary].m_pICmdAllocPerFrame[nCurrentFrameIndex]->Reset();
					stGPUParams[nIDGPUSecondary].m_pICmdList->Reset(stGPUParams[nIDGPUSecondary].m_pICmdAllocPerFrame[nCurrentFrameIndex].Get(), pIPSOQuad.Get());

					// ���Կ���Ⱦ
					{
						stGPUParams[nIDGPUMain].m_pICmdList->SetGraphicsRootSignature(pIRSMain.Get());
						stGPUParams[nIDGPUMain].m_pICmdList->SetPipelineState(pIPSOMain.Get());
						stGPUParams[nIDGPUMain].m_pICmdList->RSSetViewports(1, &stViewPort);
						stGPUParams[nIDGPUMain].m_pICmdList->RSSetScissorRects(1, &stScissorRect);

						stRTVStateTransBarrier.Transition.pResource = stGPUParams[nIDGPUMain].m_pIRTRes[nCurrentFrameIndex].Get();
						stRTVStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
						stRTVStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

						stGPUParams[nIDGPUMain].m_pICmdList->ResourceBarrier(1, &stRTVStateTransBarrier);

						D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = stGPUParams[nIDGPUMain].m_pIDHRTV->GetCPUDescriptorHandleForHeapStart();
						rtvHandle.ptr += (nCurrentFrameIndex * stGPUParams[nIDGPUMain].m_nszRTV);
						D3D12_CPU_DESCRIPTOR_HANDLE stDSVHandle = stGPUParams[nIDGPUMain].m_pIDHDSVTex->GetCPUDescriptorHandleForHeapStart();

						stGPUParams[nIDGPUMain].m_pICmdList->OMSetRenderTargets(
							1, &rtvHandle, false, &stDSVHandle);
						stGPUParams[nIDGPUMain].m_pICmdList->ClearRenderTargetView(
							rtvHandle, arf4ClearColor, 0, nullptr);
						stGPUParams[nIDGPUMain].m_pICmdList->ClearDepthStencilView(
							stDSVHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

						//ִ��ʵ�ʵ����������Ⱦ��Draw Call��
						for (int i = 0; i < nMaxObject; i++)
						{
							ID3D12DescriptorHeap* ppHeapsSkybox[]
								= { stModuleParams[i].pISRVCBVHp.Get(),stModuleParams[i].pISampleHp.Get() };
							stGPUParams[nIDGPUMain].m_pICmdList->SetDescriptorHeaps(_countof(ppHeapsSkybox), ppHeapsSkybox);
							stGPUParams[nIDGPUMain].m_pICmdList->ExecuteBundle(stModuleParams[i].pIBundle.Get());
						}
						
						stRTVStateTransBarrier.Transition.pResource = stGPUParams[nIDGPUMain].m_pIRTRes[nCurrentFrameIndex].Get();
						stRTVStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
						stRTVStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;

						stGPUParams[nIDGPUMain].m_pICmdList->ResourceBarrier(1, &stRTVStateTransBarrier);

						GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pICmdList->Close());
					}

					{// ��һ���������Կ��������������ִ���������б�
						ID3D12CommandList* ppRenderCommandLists[]
							= { stGPUParams[nIDGPUMain].m_pICmdList.Get() };
						stGPUParams[nIDGPUMain].m_pICmdQueue->ExecuteCommandLists(
							_countof(ppRenderCommandLists), ppRenderCommandLists);

						n64CurrentFenceValue = n64FenceValue;
						GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pICmdQueue->Signal(
							stGPUParams[nIDGPUMain].m_pIFence.Get(), n64CurrentFenceValue));
						n64FenceValue++;
					}

					// �����Կ�����Ⱦ������Ƶ�����������Դ��
					{
						if (bCrossAdapterTextureSupport)
						{
							// ���������֧�ֿ���������������ֻ�轫�������Ƶ��������������С�
							pICmdListCopy->CopyResource(
								stGPUParams[nIDGPUMain].m_pICrossAdapterResPerFrame[nCurrentFrameIndex].Get()
								, stGPUParams[nIDGPUMain].m_pIRTRes[nCurrentFrameIndex].Get());
						}
						else
						{
							// �����������֧�ֿ�����������������������Ϊ��������
							// �Ա������ʽ�ع��������м�ࡣʹ�ó���Ŀ��ָ�����ڴ沼�ֽ��м����Ŀ�긴�Ƶ����������С�
							D3D12_RESOURCE_DESC stRenderTargetDesc = stGPUParams[nIDGPUMain].m_pIRTRes[nCurrentFrameIndex]->GetDesc();
							D3D12_PLACED_SUBRESOURCE_FOOTPRINT stRenderTargetLayout = {};

							stGPUParams[nIDGPUMain].m_pID3D12Device4->GetCopyableFootprints(&stRenderTargetDesc, 0, 1, 0, &stRenderTargetLayout, nullptr, nullptr, nullptr);

							D3D12_TEXTURE_COPY_LOCATION stDstCopyLocation = {};
							stDstCopyLocation.pResource = stGPUParams[nIDGPUMain].m_pICrossAdapterResPerFrame[nCurrentFrameIndex].Get();
							stDstCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
							stDstCopyLocation.PlacedFootprint = stRenderTargetLayout;

							D3D12_TEXTURE_COPY_LOCATION stSrcCopyLocation = {};
							stSrcCopyLocation.pResource = stGPUParams[nIDGPUMain].m_pIRTRes[nCurrentFrameIndex].Get();
							stSrcCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
							stSrcCopyLocation.SubresourceIndex = 0;

							D3D12_BOX stCopyBox = { 0, 0, nWndWidth, nWndHeight };

							pICmdListCopy->CopyBufferRegion(stGPUParams[nIDGPUMain].m_pICrossAdapterResPerFrame[nCurrentFrameIndex].Get()
								, 0
								, stGPUParams[nIDGPUMain].m_pIRTRes[nCurrentFrameIndex].Get()
								, 0, stRenderTargetDesc.Width * stRenderTargetDesc.Height);
						}

						GRS_THROW_IF_FAILED(pICmdListCopy->Close());
					}

					{// �ڶ�����ʹ�����Կ��ϵĸ���������������ȾĿ����Դ��������Դ��ĸ���
					// ͨ������������е�Wait����ʵ��ͬһ��GPU�ϵĸ��������֮��ĵȴ�ͬ�� 
						GRS_THROW_IF_FAILED(pICmdQueueCopy->Wait(stGPUParams[nIDGPUMain].m_pIFence.Get(), n64CurrentFenceValue));

						ID3D12CommandList* ppCopyCommandLists[] = { pICmdListCopy.Get() };
						pICmdQueueCopy->ExecuteCommandLists(_countof(ppCopyCommandLists), ppCopyCommandLists);

						n64CurrentFenceValue = n64FenceValue;
						// ����������ź������ڹ����Χ������������ʹ�õڶ����Կ���������п��������Χ���ϵȴ�
						// �Ӷ���ɲ�ͬGPU������м��ͬ���ȴ�
						GRS_THROW_IF_FAILED(pICmdQueueCopy->Signal(stGPUParams[nIDGPUMain].m_pISharedFence.Get(), n64CurrentFenceValue));
						n64FenceValue++;
					}

					// ��ʼ�����Կ�����Ⱦ��ͨ���Ǻ��������˶�ģ���ȣ���������ֱ�Ӿ��ǰѻ�����Ƴ���
					{
						if (!bCrossAdapterTextureSupport)
						{
							// ��������еĻ��������Ƶ��������������Դ���ȡ���������С�
							stRTVStateTransBarrier.Transition.pResource = pISecondaryAdapterTexutrePerFrame[nCurrentFrameIndex].Get();
							stRTVStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
							stRTVStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

							stGPUParams[nIDGPUSecondary].m_pICmdList->ResourceBarrier(1, &stRTVStateTransBarrier);

							// Copy the shared buffer contents into the texture using the memory
							// layout prescribed by the texture.
							D3D12_RESOURCE_DESC stSecondaryAdapterTexture = pISecondaryAdapterTexutrePerFrame[nCurrentFrameIndex]->GetDesc();
							D3D12_PLACED_SUBRESOURCE_FOOTPRINT stTextureLayout = {};

							stGPUParams[nIDGPUSecondary].m_pID3D12Device4->GetCopyableFootprints(&stSecondaryAdapterTexture, 0, 1, 0, &stTextureLayout, nullptr, nullptr, nullptr);

							D3D12_TEXTURE_COPY_LOCATION stDstCopyLocation = {};
							stDstCopyLocation.pResource = pISecondaryAdapterTexutrePerFrame[nCurrentFrameIndex].Get();
							stDstCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
							stDstCopyLocation.SubresourceIndex = 0;

							D3D12_TEXTURE_COPY_LOCATION stSrcCopyLocation = {};
							stSrcCopyLocation.pResource = stGPUParams[nIDGPUSecondary].m_pICrossAdapterResPerFrame[nCurrentFrameIndex].Get();
							stSrcCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
							stSrcCopyLocation.PlacedFootprint = stTextureLayout;

							D3D12_BOX stCopyBox = { 0, 0, nWndWidth, nWndHeight };

							stGPUParams[nIDGPUSecondary].m_pICmdList->CopyBufferRegion(pISecondaryAdapterTexutrePerFrame[nCurrentFrameIndex].Get()
								, 0
								, stGPUParams[nIDGPUSecondary].m_pICrossAdapterResPerFrame[nCurrentFrameIndex].Get()
								, 0
								, stSecondaryAdapterTexture.Width * stSecondaryAdapterTexture.Height);

							stRTVStateTransBarrier.Transition.pResource = pISecondaryAdapterTexutrePerFrame[nCurrentFrameIndex].Get();
							stRTVStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
							stRTVStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

							stGPUParams[nIDGPUSecondary].m_pICmdList->ResourceBarrier(1, &stRTVStateTransBarrier);
						}

						stGPUParams[nIDGPUSecondary].m_pICmdList->SetGraphicsRootSignature(pIRSQuad.Get());
						stGPUParams[nIDGPUSecondary].m_pICmdList->SetPipelineState(pIPSOQuad.Get());
						ID3D12DescriptorHeap* ppHeaps[] = { pIDHSRVSecondary.Get(),pIDHSampleSecondary.Get() };
						stGPUParams[nIDGPUSecondary].m_pICmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

						stGPUParams[nIDGPUSecondary].m_pICmdList->RSSetViewports(1, &stViewPort);
						stGPUParams[nIDGPUSecondary].m_pICmdList->RSSetScissorRects(1, &stScissorRect);

						stRTVStateTransBarrier.Transition.pResource = stGPUParams[nIDGPUSecondary].m_pIRTRes[nCurrentFrameIndex].Get();
						stRTVStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
						stRTVStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

						stGPUParams[nIDGPUSecondary].m_pICmdList->ResourceBarrier(1, &stRTVStateTransBarrier);

						D3D12_CPU_DESCRIPTOR_HANDLE rtvSecondaryHandle = stGPUParams[nIDGPUSecondary].m_pIDHRTV->GetCPUDescriptorHandleForHeapStart();
						rtvSecondaryHandle.ptr += (nCurrentFrameIndex * stGPUParams[nIDGPUSecondary].m_nszRTV);
						stGPUParams[nIDGPUSecondary].m_pICmdList->OMSetRenderTargets(1, &rtvSecondaryHandle, false, nullptr);
						float f4ClearColor[] = { 1.0f,0.0f,0.0f,1.0f }; //����ʹ�������Կ���ȾĿ�겻ͬ�����ɫ���鿴�Ƿ��С�¶�ס�������
						stGPUParams[nIDGPUSecondary].m_pICmdList->ClearRenderTargetView(rtvSecondaryHandle, f4ClearColor, 0, nullptr);

						// ��ʼ���ƾ���
						D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = pIDHSRVSecondary->GetGPUDescriptorHandleForHeapStart();
						srvHandle.ptr += (nCurrentFrameIndex * stGPUParams[nIDGPUSecondary].m_nszSRVCBVUAV);
						stGPUParams[nIDGPUSecondary].m_pICmdList->SetGraphicsRootDescriptorTable(0, srvHandle);
						stGPUParams[nIDGPUSecondary].m_pICmdList->SetGraphicsRootDescriptorTable(1, pIDHSampleSecondary->GetGPUDescriptorHandleForHeapStart());

						stGPUParams[nIDGPUSecondary].m_pICmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
						stGPUParams[nIDGPUSecondary].m_pICmdList->IASetVertexBuffers(0, 1, &pstVBVQuad);
						// Draw Call!
						stGPUParams[nIDGPUSecondary].m_pICmdList->DrawInstanced(4, 1, 0, 0);

						// ���ú�ͬ��Χ��
						stRTVStateTransBarrier.Transition.pResource = stGPUParams[nIDGPUSecondary].m_pIRTRes[nCurrentFrameIndex].Get();
						stRTVStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
						stRTVStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

						stGPUParams[nIDGPUSecondary].m_pICmdList->ResourceBarrier(1, &stRTVStateTransBarrier);

						GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pICmdList->Close());
					}

					{// ��������ʹ�ø����Կ��ϵ����������ִ�������б�
						// �����Կ��ϵ����������ͨ���ȴ������Χ��������������������Կ�֮���ͬ��
						GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pICmdQueue->Wait(
							stGPUParams[nIDGPUSecondary].m_pISharedFence.Get()
							, n64CurrentFenceValue));

						ID3D12CommandList* ppBlurCommandLists[] = { stGPUParams[nIDGPUSecondary].m_pICmdList.Get() };
						stGPUParams[nIDGPUSecondary].m_pICmdQueue->ExecuteCommandLists(_countof(ppBlurCommandLists), ppBlurCommandLists);
					}

					// ִ��Present�������ճ��ֻ���
					GRS_THROW_IF_FAILED(pISwapChain3->Present(1, 0));

					n64CurrentFenceValue = n64FenceValue;
					GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pICmdQueue->Signal(stGPUParams[nIDGPUSecondary].m_pIFence.Get(), n64CurrentFenceValue));
					n64FenceValue++;
					GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pIFence->SetEventOnCompletion(n64CurrentFenceValue, hEventFence));
				}
			}
			break;
			case 1:
			{//������Ϣ
				while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
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
			case WAIT_TIMEOUT:
			{

			}
			break;

			default:
				break;
			}
		}
	}
	catch (CGRSCOMException& e)
	{//������COM�쳣
		e;
	}
#if defined(_DEBUG)
	{
		ComPtr<IDXGIDebug1> dxgiDebug;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
		{
			dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
		}
	}
#endif
	::CoUninitialize();
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
		{//

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

BOOL LoadMeshVertex(const CHAR* pszMeshFileName, UINT& nVertexCnt, ST_GRS_VERTEX_MODULE*& ppVertex, UINT*& ppIndices)
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

		ppVertex = (ST_GRS_VERTEX_MODULE*)GRS_CALLOC(nVertexCnt * sizeof(ST_GRS_VERTEX_MODULE));
		ppIndices = (UINT*)GRS_CALLOC(nVertexCnt * sizeof(UINT));

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