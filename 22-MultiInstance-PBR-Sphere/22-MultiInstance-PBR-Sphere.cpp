#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // �� Windows ͷ���ų�����ʹ�õ�����
#include <windows.h>
#include <tchar.h>
#include <commdlg.h>
#include <wrl.h> //���WTL֧�� ����ʹ��COM
#include <strsafe.h>
#include <fstream>  //for ifstream

#include <atlbase.h>
#include <atlcoll.h>
#include <atlchecked.h>
#include <atlstr.h>
#include <atlconv.h>
#include <atlexcept.h>

#include <DirectXMath.h>

#include <dxgi1_6.h>
#include <d3d12.h> //for d3d12
#include <d3dcompiler.h>
#if defined(_DEBUG)
#include <dxgidebug.h>
#endif

using namespace std;
using namespace DirectX;
using namespace Microsoft;
using namespace Microsoft::WRL;

#include "../Commons/GRS_D3D12_Utility.h"
#include "../Commons/GRS_Mem.h"

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define GRS_SLEEP(dwMilliseconds)  WaitForSingleObject(GetCurrentThread(),dwMilliseconds)
#define GRS_THROW_IF_FAILED(hr) {HRESULT _hr = (hr);if (FAILED(_hr)){ ATLTRACE("Error: 0x%08x\n",_hr); AtlThrow(_hr); }}
//���������ϱ߽�����㷨 �ڴ�����г��� ���ס
#define GRS_UPPER(A,B) ((size_t)(((A)+((B)-1))&~((B) - 1)))
//��ȡ������
#define GRS_UPPER_DIV(A,B) ((UINT)(((A)+((B)-1))/(B)))

#define GRS_WND_CLASS _T("GRS Game Window Class")
#define GRS_WND_TITLE _T("GRS DirectX12 Sample: MultiInstance PBR Sphere With Point Light")

const UINT		g_nFrameBackBufCount = 3u;

struct ST_GRS_GPU_TARGET_STATUS
{
    DWORD                           m_dwWndStyle;
    INT                             m_iWndWidth;
    INT							    m_iWndHeight;
    HWND						    m_hWnd;
    RECT                            m_rtWnd;
    RECT                            m_rtWndClient;

    BOOL                            m_bFullScreen;
    BOOL                            m_bSupportTearing;

    UINT			                m_nFrameIndex;
    DXGI_FORMAT		                m_emRenderTargetFormat;
    DXGI_FORMAT                     m_emDepthStencilFormat;
    UINT                            m_nRTVDescriptorSize;
    UINT                            m_nDSVDescriptorSize;
    UINT                            m_nSamplerDescriptorSize;
    UINT                            m_nCBVSRVDescriptorSize;

    UINT64						    m_n64FenceValue;
    HANDLE						    m_hEventFence;

    D3D12_VIEWPORT                  m_stViewPort;
    D3D12_RECT                      m_stScissorRect;

    ComPtr<ID3D12Fence>			    m_pIFence;
    ComPtr<IDXGISwapChain3>		    m_pISwapChain3;
    ComPtr<ID3D12DescriptorHeap>    m_pIRTVHeap;
    ComPtr<ID3D12Resource>		    m_pIARenderTargets[g_nFrameBackBufCount];
    ComPtr<ID3D12DescriptorHeap>    m_pIDSVHeap;
    ComPtr<ID3D12Resource>		    m_pIDepthStencilBuffer;
};

TCHAR g_pszAppPath[MAX_PATH] = {};

XMVECTOR g_v4EyePos = XMVectorSet(0.0f, 1.0f, -50.0f, 0.0f);	//�۾�λ��
XMVECTOR g_v4LookAt = XMVectorSet(0.0f, 1.0f, 1.0f, 0.0f);		//�۾�������λ��
XMVECTOR g_v4UpDir = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);		//ͷ�����Ϸ�λ��

ST_GRS_GPU_TARGET_STATUS        g_stGPUStatus = {};
D3D12_HEAP_PROPERTIES           g_stDefautHeapProps = {};
D3D12_HEAP_PROPERTIES           g_stUploadHeapProps = {};
D3D12_RESOURCE_DESC             g_stBufferResSesc = {};
D3D12_DEPTH_STENCIL_VIEW_DESC   g_stDepthStencilDesc = {};
D3D12_CLEAR_VALUE               g_stDepthOptimizedClearValue = {};
D3D12_RESOURCE_DESC             g_stDSTex2DDesc = {};
D3D12_DESCRIPTOR_HEAP_DESC      g_stDSVHeapDesc = {};

//--------------------------------------------------------------------------------------------------------------------------
struct ST_GRS_CB_MVP
{
    XMFLOAT4X4 mxWorld;                   //�������������ʵ��Model->World��ת������
    XMFLOAT4X4 mxView;                    //�Ӿ���
    XMFLOAT4X4 mxProjection;              //ͶӰ����
    XMFLOAT4X4 mxViewProj;                //�Ӿ���*ͶӰ
    XMFLOAT4X4 mxMVP;                     //����*�Ӿ���*ͶӰ
};

// Camera
struct ST_CB_CAMERA //: register( b1 )
{
    XMFLOAT4 m_v4CameraPos;
};

#define GRS_LIGHT_COUNT 4
// lights
struct ST_CB_LIGHTS //: register( b2 )
{
    XMFLOAT4 m_v4LightPos[GRS_LIGHT_COUNT];
    XMFLOAT4 m_v4LightColors[GRS_LIGHT_COUNT];
};

struct ST_GRS_VERTEX
{
    XMFLOAT4 m_v4Position;		//Position
    XMFLOAT4 m_v4Normal;		//Normal
    XMFLOAT2 m_v2UV;		    //Texcoord
};

struct ST_GRS_PER_INSTANCE
{
    XMFLOAT4X4	mxModel2World;
    XMFLOAT4	mv4Albedo;    // ������
    float		mfMetallic;    // ������
    float		mfRoughness;    // �ֲڶ�
    float		mfAO;    // 
};

float g_fScaling = 1.0f;    // ����ϵ��
double g_fPalstance = 10.0f * XM_PI / 180.0f;	//������ת�Ľ��ٶȣ���λ������/��
//--------------------------------------------------------------------------------------------------------------------------

void OnSize(UINT width, UINT height, bool minimized);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
BOOL LoadMeshVertex(const CHAR* pszMeshFileName, UINT& nVertexCnt, ST_GRS_VERTEX*& ppVertex, UINT*& ppIndices);

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR    lpCmdLine, int nCmdShow)
{
    const float						    faClearColor[] = { 0.17647f, 0.549f, 0.941176f, 1.0f };
    D3D_FEATURE_LEVEL					emFeatureLevel = D3D_FEATURE_LEVEL_12_1;
    ComPtr<IDXGIFactory6>				pIDXGIFactory6;
    ComPtr<ID3D12Device4>				pID3D12Device4;

    ComPtr<ID3D12CommandQueue>			pIMainCMDQueue;
    ComPtr<ID3D12CommandAllocator>		pIMainCMDAlloc;
    ComPtr<ID3D12GraphicsCommandList>	pIMainCMDList;

    D3D12_RESOURCE_BARRIER              stBeginResBarrier = {};
    D3D12_RESOURCE_BARRIER              stEneResBarrier = {};
    D3D12_RESOURCE_BARRIER              stResStateTransBarrier = {};

    ComPtr<ID3D12RootSignature>		    pIRootSignature;
    ComPtr<ID3D12PipelineState>		    pIPSOModel;

    const UINT                          nConstBufferCount = 3;  // ��ʾ������Ҫ3��Const Buffer
    ComPtr<ID3D12DescriptorHeap>	    pICBVSRVHeap;

    UINT                                nIndexCnt = 0;
    ComPtr<ID3D12Resource>			    pIVB;
    ComPtr<ID3D12Resource>			    pIIB;

    int                                 iRowCnts = 20;             // ����   
    int                                 iColCnts = 2 * iRowCnts;      // ������ÿ�е�ʵ������

    ComPtr<ID3D12Resource>			    pIInstanceData;

    const UINT                          cnSlotCount = 2;            // ����������ϴ������ʵ������
    D3D12_VERTEX_BUFFER_VIEW		    stVBV[cnSlotCount] = {};
    D3D12_INDEX_BUFFER_VIEW			    stIBV = {};

    ComPtr<ID3D12Resource>			    pICBMVP;
    ST_GRS_CB_MVP* pstCBMVP = nullptr;
    ComPtr<ID3D12Resource>			    pICBCameraPos;
    ST_CB_CAMERA* pstCBCameraPos = nullptr;
    ComPtr<ID3D12Resource>			    pICBLights;
    ST_CB_LIGHTS* pstCBLights = nullptr;

    USES_CONVERSION;
    {
        g_stGPUStatus.m_iWndWidth = 1280;
        g_stGPUStatus.m_iWndHeight = 800;
        g_stGPUStatus.m_hWnd = nullptr;
        g_stGPUStatus.m_nFrameIndex = 0;
        g_stGPUStatus.m_emRenderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        g_stGPUStatus.m_emDepthStencilFormat = DXGI_FORMAT_D32_FLOAT;
        g_stGPUStatus.m_nRTVDescriptorSize = 0U;
        g_stGPUStatus.m_nDSVDescriptorSize = 0U;
        g_stGPUStatus.m_nSamplerDescriptorSize = 0u;
        g_stGPUStatus.m_nCBVSRVDescriptorSize = 0u;
        g_stGPUStatus.m_stViewPort = { 0.0f, 0.0f, static_cast<float>(g_stGPUStatus.m_iWndWidth), static_cast<float>(g_stGPUStatus.m_iWndHeight), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
        g_stGPUStatus.m_stScissorRect = { 0, 0, static_cast<LONG>(g_stGPUStatus.m_iWndWidth), static_cast<LONG>(g_stGPUStatus.m_iWndHeight) };
        g_stGPUStatus.m_n64FenceValue = 0ui64;
        g_stGPUStatus.m_hEventFence = nullptr;

        g_stDefautHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
        g_stDefautHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        g_stDefautHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        g_stDefautHeapProps.CreationNodeMask = 0;
        g_stDefautHeapProps.VisibleNodeMask = 0;

        g_stUploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        g_stUploadHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        g_stUploadHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        g_stUploadHeapProps.CreationNodeMask = 0;
        g_stUploadHeapProps.VisibleNodeMask = 0;

        g_stBufferResSesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        g_stBufferResSesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        g_stBufferResSesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        g_stBufferResSesc.Format = DXGI_FORMAT_UNKNOWN;
        g_stBufferResSesc.Width = 0;
        g_stBufferResSesc.Height = 1;
        g_stBufferResSesc.DepthOrArraySize = 1;
        g_stBufferResSesc.MipLevels = 1;
        g_stBufferResSesc.SampleDesc.Count = 1;
        g_stBufferResSesc.SampleDesc.Quality = 0;

        g_stDepthStencilDesc.Format = g_stGPUStatus.m_emDepthStencilFormat;
        g_stDepthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        g_stDepthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

        g_stDepthOptimizedClearValue.Format = g_stGPUStatus.m_emDepthStencilFormat;
        g_stDepthOptimizedClearValue.DepthStencil.Depth = 1.0f;
        g_stDepthOptimizedClearValue.DepthStencil.Stencil = 0;

        g_stDSTex2DDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        g_stDSTex2DDesc.Alignment = 0;
        g_stDSTex2DDesc.Width = g_stGPUStatus.m_iWndWidth;
        g_stDSTex2DDesc.Height = g_stGPUStatus.m_iWndHeight;
        g_stDSTex2DDesc.DepthOrArraySize = 1;
        g_stDSTex2DDesc.MipLevels = 0;
        g_stDSTex2DDesc.Format = g_stGPUStatus.m_emDepthStencilFormat;
        g_stDSTex2DDesc.SampleDesc.Count = 1;
        g_stDSTex2DDesc.SampleDesc.Quality = 0;
        g_stDSTex2DDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        g_stDSTex2DDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        g_stDSVHeapDesc.NumDescriptors = 1;
        g_stDSVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        g_stDSVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        stResStateTransBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        stResStateTransBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        stResStateTransBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    }

    try
    {
        ::CoInitialize(nullptr);  //for WIC & COM

        // 0���õ���ǰ�Ĺ���Ŀ¼����������ʹ�����·�������ʸ�����Դ�ļ�
        {
            if ( 0 == ::GetModuleFileName(nullptr, g_pszAppPath, MAX_PATH) )
            {
                GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
            }

            WCHAR* lastSlash = _tcsrchr(g_pszAppPath, _T('\\'));
            if ( lastSlash )
            {//ɾ��Exe�ļ���
                *(lastSlash) = _T('\0');
            }

            lastSlash = _tcsrchr(g_pszAppPath, _T('\\'));
            if ( lastSlash )
            {//ɾ��x64·��
                *(lastSlash) = _T('\0');
            }

            lastSlash = _tcsrchr(g_pszAppPath, _T('\\'));
            if ( lastSlash )
            {//ɾ��Debug �� Release·��
                *(lastSlash + 1) = _T('\0');
            }
        }

        // 1����������
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
            wcex.lpszClassName = GRS_WND_CLASS;
            RegisterClassEx(&wcex);

            g_stGPUStatus.m_dwWndStyle = WS_OVERLAPPEDWINDOW;

            RECT rtWnd = { 0, 0, g_stGPUStatus.m_iWndWidth, g_stGPUStatus.m_iWndHeight };
            AdjustWindowRect(&rtWnd, g_stGPUStatus.m_dwWndStyle, FALSE);

            // ���㴰�ھ��е���Ļ����
            INT posX = (GetSystemMetrics(SM_CXSCREEN) - rtWnd.right - rtWnd.left) / 2;
            INT posY = (GetSystemMetrics(SM_CYSCREEN) - rtWnd.bottom - rtWnd.top) / 2;

            g_stGPUStatus.m_hWnd = CreateWindowW(GRS_WND_CLASS
                , GRS_WND_TITLE
                , g_stGPUStatus.m_dwWndStyle
                , posX
                , posY
                , rtWnd.right - rtWnd.left
                , rtWnd.bottom - rtWnd.top
                , nullptr
                , nullptr
                , hInstance
                , nullptr);

            if ( !g_stGPUStatus.m_hWnd )
            {
                GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
            }
        }

        // 2������DXGI Factory����
        {
            UINT nDXGIFactoryFlags = 0U;
#if defined(_DEBUG)
            // ����ʾ��ϵͳ�ĵ���֧��
            ComPtr<ID3D12Debug> debugController;
            if ( SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))) )
            {
                debugController->EnableDebugLayer();
                // �򿪸��ӵĵ���֧��
                nDXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
            }
#endif
            ComPtr<IDXGIFactory5> pIDXGIFactory5;
            GRS_THROW_IF_FAILED(CreateDXGIFactory2(nDXGIFactoryFlags, IID_PPV_ARGS(&pIDXGIFactory5)));
            GRS_SET_DXGI_DEBUGNAME_COMPTR(pIDXGIFactory5);

            //��ȡIDXGIFactory6�ӿ�
            GRS_THROW_IF_FAILED(pIDXGIFactory5.As(&pIDXGIFactory6));
            GRS_SET_DXGI_DEBUGNAME_COMPTR(pIDXGIFactory6);

            // ���Tearing֧��
            HRESULT hr = pIDXGIFactory6->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING
                , &g_stGPUStatus.m_bSupportTearing
                , sizeof(g_stGPUStatus.m_bSupportTearing));
            g_stGPUStatus.m_bSupportTearing = SUCCEEDED(hr) && g_stGPUStatus.m_bSupportTearing;
        }

        // 3��ö�������������豸
        {//ѡ��NUMA�ܹ��Ķ���������3D�豸����,��ʱ�Ȳ�֧�ּ����ˣ���Ȼ������޸���Щ��Ϊ
            ComPtr<IDXGIAdapter1> pIAdapter1;
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
            ::GetWindowText(g_stGPUStatus.m_hWnd, pszWndTitle, MAX_PATH);
            StringCchPrintf(pszWndTitle
                , MAX_PATH
                , _T("%s(GPU[0]:%s)")
                , pszWndTitle
                , stAdapterDesc.Description);
            ::SetWindowText(g_stGPUStatus.m_hWnd, pszWndTitle);

            // �õ�ÿ��������Ԫ�صĴ�С
            g_stGPUStatus.m_nRTVDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            g_stGPUStatus.m_nDSVDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
            g_stGPUStatus.m_nSamplerDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
            g_stGPUStatus.m_nCBVSRVDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        // 4�������������&�����б�
        {
            D3D12_COMMAND_QUEUE_DESC stQueueDesc = {};
            stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandQueue(&stQueueDesc, IID_PPV_ARGS(&pIMainCMDQueue)));
            GRS_SET_D3D12_DEBUGNAME_COMPTR(pIMainCMDQueue);

            GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT
                , IID_PPV_ARGS(&pIMainCMDAlloc)));

            GRS_SET_D3D12_DEBUGNAME_COMPTR(pIMainCMDAlloc);

            // ����ͼ�������б�
            GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandList(
                0
                , D3D12_COMMAND_LIST_TYPE_DIRECT
                , pIMainCMDAlloc.Get()
                , nullptr
                , IID_PPV_ARGS(&pIMainCMDList)));

            GRS_SET_D3D12_DEBUGNAME_COMPTR(pIMainCMDList);
        }

        // 5������������
        {
            DXGI_SWAP_CHAIN_DESC1 stSwapChainDesc = {};
            stSwapChainDesc.BufferCount = g_nFrameBackBufCount;
            stSwapChainDesc.Width = g_stGPUStatus.m_iWndWidth;
            stSwapChainDesc.Height = g_stGPUStatus.m_iWndHeight;
            stSwapChainDesc.Format = g_stGPUStatus.m_emRenderTargetFormat;
            stSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            stSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            stSwapChainDesc.SampleDesc.Count = 1;
            stSwapChainDesc.Scaling = DXGI_SCALING_NONE;

            // ��Ⲣ����Tearing֧��
            stSwapChainDesc.Flags = g_stGPUStatus.m_bSupportTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

            ComPtr<IDXGISwapChain1> pISwapChain1;

            GRS_THROW_IF_FAILED(pIDXGIFactory6->CreateSwapChainForHwnd(
                pIMainCMDQueue.Get(),		// ��������Ҫ������У�Present����Ҫִ��
                g_stGPUStatus.m_hWnd,
                &stSwapChainDesc,
                nullptr,
                nullptr,
                &pISwapChain1
            ));

            GRS_SET_DXGI_DEBUGNAME_COMPTR(pISwapChain1);
            GRS_THROW_IF_FAILED(pISwapChain1.As(&g_stGPUStatus.m_pISwapChain3));
            GRS_SET_DXGI_DEBUGNAME_COMPTR(g_stGPUStatus.m_pISwapChain3);

            //�õ���ǰ�󻺳�������ţ�Ҳ������һ����Ҫ������ʾ�Ļ����������
            //ע��˴�ʹ���˸߰汾��SwapChain�ӿڵĺ���
            g_stGPUStatus.m_nFrameIndex = g_stGPUStatus.m_pISwapChain3->GetCurrentBackBufferIndex();

            //����RTV(��ȾĿ����ͼ)��������(����ѵĺ���Ӧ�����Ϊ������߹̶���СԪ�صĹ̶���С�Դ��)
            {
                D3D12_DESCRIPTOR_HEAP_DESC stRTVHeapDesc = {};
                stRTVHeapDesc.NumDescriptors = g_nFrameBackBufCount;
                stRTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
                stRTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

                GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(
                    &stRTVHeapDesc
                    , IID_PPV_ARGS(&g_stGPUStatus.m_pIRTVHeap)));

                GRS_SET_D3D12_DEBUGNAME_COMPTR(g_stGPUStatus.m_pIRTVHeap);

            }

            //����RTV��������
            {
                D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle
                    = g_stGPUStatus.m_pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
                for ( UINT i = 0; i < g_nFrameBackBufCount; i++ )
                {
                    GRS_THROW_IF_FAILED(g_stGPUStatus.m_pISwapChain3->GetBuffer(i
                        , IID_PPV_ARGS(&g_stGPUStatus.m_pIARenderTargets[i])));

                    GRS_SET_D3D12_DEBUGNAME_INDEXED_COMPTR(g_stGPUStatus.m_pIARenderTargets, i);

                    pID3D12Device4->CreateRenderTargetView(g_stGPUStatus.m_pIARenderTargets[i].Get()
                        , nullptr
                        , stRTVHandle);

                    stRTVHandle.ptr += g_stGPUStatus.m_nRTVDescriptorSize;
                }
            }

            if ( g_stGPUStatus.m_bSupportTearing )
            {// ���֧��Tearing����ô�͹ر�ϵͳ��ALT + Enteryȫ����֧�֣������������Լ�����
                GRS_THROW_IF_FAILED(pIDXGIFactory6->MakeWindowAssociation(g_stGPUStatus.m_hWnd, DXGI_MWA_NO_ALT_ENTER));
            }

        }

        // 6��������Ȼ��弰��Ȼ�����������
        {
            //ʹ����ʽĬ�϶Ѵ���һ��������建������
            //��Ϊ��������Ȼ�������һֱ��ʹ�ã����õ����岻������ֱ��ʹ����ʽ�ѣ�ͼ����
            GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
                &g_stDefautHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &g_stDSTex2DDesc
                , D3D12_RESOURCE_STATE_DEPTH_WRITE
                , &g_stDepthOptimizedClearValue
                , IID_PPV_ARGS(&g_stGPUStatus.m_pIDepthStencilBuffer)
            ));

            //==============================================================================================
            GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(&g_stDSVHeapDesc, IID_PPV_ARGS(&g_stGPUStatus.m_pIDSVHeap)));

            pID3D12Device4->CreateDepthStencilView(g_stGPUStatus.m_pIDepthStencilBuffer.Get()
                , &g_stDepthStencilDesc
                , g_stGPUStatus.m_pIDSVHeap->GetCPUDescriptorHandleForHeapStart());
        }

        // 7������Χ��
        {
            GRS_THROW_IF_FAILED(pID3D12Device4->CreateFence(0
                , D3D12_FENCE_FLAG_NONE
                , IID_PPV_ARGS(&g_stGPUStatus.m_pIFence)));
            GRS_SET_D3D12_DEBUGNAME_COMPTR(g_stGPUStatus.m_pIFence);

            g_stGPUStatus.m_n64FenceValue = 1;

            // ����һ��Eventͬ���������ڵȴ�Χ���¼�֪ͨ
            g_stGPUStatus.m_hEventFence = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            if ( g_stGPUStatus.m_hEventFence == nullptr )
            {
                GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
            }
        }

        // 8������Shader������Ⱦ����״̬����
        {
            // �ȴ�����ǩ��
            D3D12_FEATURE_DATA_ROOT_SIGNATURE stFeatureData = {};
            // ����Ƿ�֧��V1.1�汾�ĸ�ǩ��
            stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
            if ( FAILED(pID3D12Device4->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &stFeatureData, sizeof(stFeatureData))) )
            {// 1.0�� ֱ�Ӷ��쳣�˳���
                GRS_THROW_IF_FAILED(E_NOTIMPL);
            }

            D3D12_DESCRIPTOR_RANGE1 stDSPRanges1[1] = {};

            stDSPRanges1[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
            stDSPRanges1[0].NumDescriptors = nConstBufferCount;
            stDSPRanges1[0].BaseShaderRegister = 0;
            stDSPRanges1[0].RegisterSpace = 0;
            stDSPRanges1[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
            stDSPRanges1[0].OffsetInDescriptorsFromTableStart = 0;

            D3D12_ROOT_PARAMETER1 stRootParameters1[1] = {};
            stRootParameters1[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            stRootParameters1[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            stRootParameters1[0].DescriptorTable.NumDescriptorRanges = 1;
            stRootParameters1[0].DescriptorTable.pDescriptorRanges = &stDSPRanges1[0];

            D3D12_VERSIONED_ROOT_SIGNATURE_DESC stRootSignatureDesc = {};
            stRootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
            stRootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
            stRootSignatureDesc.Desc_1_1.NumParameters = _countof(stRootParameters1);
            stRootSignatureDesc.Desc_1_1.pParameters = stRootParameters1;
            stRootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
            stRootSignatureDesc.Desc_1_1.pStaticSamplers = nullptr;

            ComPtr<ID3DBlob> pISignatureBlob;
            ComPtr<ID3DBlob> pIErrorBlob;

            HRESULT _hr = D3D12SerializeVersionedRootSignature(&stRootSignatureDesc, &pISignatureBlob, &pIErrorBlob);

            if ( FAILED(_hr) )
            {
                ATLTRACE("Create Root Signature Error��%s\n", (CHAR*)(pIErrorBlob ? pIErrorBlob->GetBufferPointer() : "δ֪����"));
                AtlThrow(_hr);
            }

            GRS_THROW_IF_FAILED(pID3D12Device4->CreateRootSignature(0, pISignatureBlob->GetBufferPointer()
                , pISignatureBlob->GetBufferSize(), IID_PPV_ARGS(&pIRootSignature)));
            GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRootSignature);

            // �����Ժ�����ʽ����Shader
            UINT compileFlags = 0;
#if defined(_DEBUG)
            // Enable better shader debugging with the graphics debugging tools.
            compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
            //����Ϊ�о�����ʽ	   
            compileFlags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

            TCHAR pszShaderFileName[MAX_PATH] = {};
            ComPtr<ID3DBlob> pIVSModel;
            ComPtr<ID3DBlob> pIPSModel;
            pIErrorBlob.Reset();
            StringCchPrintf(pszShaderFileName, MAX_PATH, _T("%s22-MultiInstance-PBR-Sphere\\Shader\\Multiple_Instances_VS.hlsl"), g_pszAppPath);
            HRESULT hr = D3DCompileFromFile(pszShaderFileName, nullptr, nullptr, "VSMain", "vs_5_1", compileFlags, 0, &pIVSModel, &pIErrorBlob);

            if ( FAILED(hr) )
            {
                ATLTRACE("���� Vertex Shader��\"%s\" ��������%s\n"
                    , T2A(pszShaderFileName)
                    , pIErrorBlob ? pIErrorBlob->GetBufferPointer() : "�޷���ȡ�ļ���");
                GRS_THROW_IF_FAILED(hr);
            }
            pIErrorBlob.Reset();
            StringCchPrintf(pszShaderFileName, MAX_PATH, _T("%s22-MultiInstance-PBR-Sphere\\Shader\\PBR.hlsl"), g_pszAppPath);
            hr = D3DCompileFromFile(pszShaderFileName, nullptr, nullptr, "PSMain", "ps_5_1", compileFlags, 0, &pIPSModel, &pIErrorBlob);
            if ( FAILED(hr) )
            {
                ATLTRACE("���� Pixel Shader��\"%s\" ��������%s\n"
                    , T2A(pszShaderFileName)
                    , pIErrorBlob ? pIErrorBlob->GetBufferPointer() : "�޷���ȡ�ļ���");
                GRS_THROW_IF_FAILED(hr);
            }

            // ����ÿ����ÿʵ�����ݽṹ 
            D3D12_INPUT_ELEMENT_DESC stIALayoutSphere[] =
            {
                // ǰ������ÿ�������ݴӲ��0����
                { "POSITION",0, DXGI_FORMAT_R32G32B32A32_FLOAT,0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "NORMAL",  0, DXGI_FORMAT_R32G32B32A32_FLOAT,0,16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "TEXCOORD",0, DXGI_FORMAT_R32G32_FLOAT,      0,32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

                // �������ûʵ�����ݴӲ��1���룬ǰ�ĸ�������ͬ���һ�����󣬽�ʵ����ģ�;ֲ��ռ�任������ռ�
                { "WORLD",   0, DXGI_FORMAT_R32G32B32A32_FLOAT,1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
                { "WORLD",   1, DXGI_FORMAT_R32G32B32A32_FLOAT,1,16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
                { "WORLD",   2, DXGI_FORMAT_R32G32B32A32_FLOAT,1,32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
                { "WORLD",   3, DXGI_FORMAT_R32G32B32A32_FLOAT,1,48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
                { "COLOR",   0, DXGI_FORMAT_R32G32B32A32_FLOAT,1,64, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
                { "COLOR",   1, DXGI_FORMAT_R32_FLOAT,         1,80, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
                { "COLOR",   2, DXGI_FORMAT_R32_FLOAT,         1,84, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
                { "COLOR",   3, DXGI_FORMAT_R32_FLOAT,         1,88, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
            };

            // ���� graphics pipeline state object (PSO)����
            D3D12_GRAPHICS_PIPELINE_STATE_DESC stPSODesc = {};
            stPSODesc.InputLayout = { stIALayoutSphere, _countof(stIALayoutSphere) };

            stPSODesc.pRootSignature = pIRootSignature.Get();
            stPSODesc.VS.pShaderBytecode = pIVSModel->GetBufferPointer();
            stPSODesc.VS.BytecodeLength = pIVSModel->GetBufferSize();
            stPSODesc.PS.pShaderBytecode = pIPSModel->GetBufferPointer();
            stPSODesc.PS.BytecodeLength = pIPSModel->GetBufferSize();

            stPSODesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
            stPSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;

            stPSODesc.BlendState.AlphaToCoverageEnable = FALSE;
            stPSODesc.BlendState.IndependentBlendEnable = FALSE;
            stPSODesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

            stPSODesc.DSVFormat = g_stGPUStatus.m_emDepthStencilFormat;
            stPSODesc.DepthStencilState.DepthEnable = TRUE;
            stPSODesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;//������Ȼ���д�빦��
            stPSODesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;     //��Ȳ��Ժ�������ֵΪ��ͨ����Ȳ��ԣ�
            stPSODesc.DepthStencilState.StencilEnable = FALSE;

            stPSODesc.SampleMask = UINT_MAX;
            stPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            stPSODesc.NumRenderTargets = 1;
            stPSODesc.RTVFormats[0] = g_stGPUStatus.m_emRenderTargetFormat;
            stPSODesc.SampleDesc.Count = 1;

            GRS_THROW_IF_FAILED(pID3D12Device4->CreateGraphicsPipelineState(&stPSODesc, IID_PPV_ARGS(&pIPSOModel)));
            GRS_SET_D3D12_DEBUGNAME_COMPTR(pIPSOModel);
        }

        // 8������CBV�ѡ��������塢������
        if ( TRUE )      // �������˼�����ҵ�VS2019�д����ʾʧЧ�ˣ��Ӹ����������if�������ʾ��������
        {
            //��������ͼ��������CBV����������һ������������
            D3D12_DESCRIPTOR_HEAP_DESC stSRVHeapDesc = {};
            stSRVHeapDesc.NumDescriptors = nConstBufferCount; // nConstBufferCount * CBV 
            stSRVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            stSRVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

            GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(&stSRVHeapDesc, IID_PPV_ARGS(&pICBVSRVHeap)));
            GRS_SET_D3D12_DEBUGNAME_COMPTR(pICBVSRVHeap);

            // ������������Ͷ�Ӧ������      
            // MVP Const Buffer
            g_stBufferResSesc.Width = GRS_UPPER(sizeof(ST_GRS_CB_MVP), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
            GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
                &g_stUploadHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &g_stBufferResSesc //ע�⻺��ߴ�����Ϊ256�߽�����С
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS(&pICBMVP)));
            GRS_SET_D3D12_DEBUGNAME_COMPTR(pICBMVP);

            // Map ֮��Ͳ���Unmap�� ֱ�Ӹ������ݽ�ȥ ����ÿ֡������map-copy-unmap�˷�ʱ����
            GRS_THROW_IF_FAILED(pICBMVP->Map(0, nullptr, reinterpret_cast<void**>(&pstCBMVP)));

            g_stBufferResSesc.Width = GRS_UPPER(sizeof(ST_CB_CAMERA), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
            GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
                &g_stUploadHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &g_stBufferResSesc //ע�⻺��ߴ�����Ϊ256�߽�����С
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS(&pICBCameraPos)));
            GRS_SET_D3D12_DEBUGNAME_COMPTR(pICBCameraPos);

            // Map ֮��Ͳ���Unmap�� ֱ�Ӹ������ݽ�ȥ ����ÿ֡������map-copy-unmap�˷�ʱ����
            GRS_THROW_IF_FAILED(pICBCameraPos->Map(0, nullptr, reinterpret_cast<void**>(&pstCBCameraPos)));

            g_stBufferResSesc.Width = GRS_UPPER(sizeof(ST_CB_LIGHTS), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
            GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
                &g_stUploadHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &g_stBufferResSesc //ע�⻺��ߴ�����Ϊ256�߽�����С
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS(&pICBLights)));
            GRS_SET_D3D12_DEBUGNAME_COMPTR(pICBLights);

            // Map ֮��Ͳ���Unmap�� ֱ�Ӹ������ݽ�ȥ ����ÿ֡������map-copy-unmap�˷�ʱ����
            GRS_THROW_IF_FAILED(pICBLights->Map(0, nullptr, reinterpret_cast<void**>(&pstCBLights)));

            // Create Const Buffer View 
            D3D12_CPU_DESCRIPTOR_HANDLE stCBVHandle = pICBVSRVHeap->GetCPUDescriptorHandleForHeapStart();

            // ������һ��CBV ������� �Ӿ��� ͸�Ӿ��� 
            D3D12_CONSTANT_BUFFER_VIEW_DESC stCBVDesc = {};
            stCBVDesc.BufferLocation = pICBMVP->GetGPUVirtualAddress();
            stCBVDesc.SizeInBytes = (UINT)GRS_UPPER(sizeof(ST_GRS_CB_MVP), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
            pID3D12Device4->CreateConstantBufferView(&stCBVDesc, stCBVHandle);

            stCBVHandle.ptr += g_stGPUStatus.m_nCBVSRVDescriptorSize;
            stCBVDesc.BufferLocation = pICBCameraPos->GetGPUVirtualAddress();
            stCBVDesc.SizeInBytes = (UINT)GRS_UPPER(sizeof(ST_CB_CAMERA), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
            pID3D12Device4->CreateConstantBufferView(&stCBVDesc, stCBVHandle);

            stCBVHandle.ptr += g_stGPUStatus.m_nCBVSRVDescriptorSize;
            stCBVDesc.BufferLocation = pICBLights->GetGPUVirtualAddress();
            stCBVDesc.SizeInBytes = (UINT)GRS_UPPER(sizeof(ST_CB_LIGHTS), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
            pID3D12Device4->CreateConstantBufferView(&stCBVDesc, stCBVHandle);

        }

        // 9������D3D12��ģ����Դ������������
        {
            CHAR pszMeshFile[MAX_PATH] = {};
            StringCchPrintfA(pszMeshFile, MAX_PATH, "%sAssets\\sphere.txt", T2A(g_pszAppPath));

            ST_GRS_VERTEX* pstVertices = nullptr;
            UINT* pnIndices = nullptr;
            UINT  nVertexCnt = 0;
            LoadMeshVertex(pszMeshFile, nVertexCnt, pstVertices, pnIndices);
            nIndexCnt = nVertexCnt;

            g_stBufferResSesc.Width = nVertexCnt * sizeof(ST_GRS_VERTEX);
            //���� Vertex Buffer ��ʹ��Upload��ʽ��
            GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
                &g_stUploadHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &g_stBufferResSesc
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS(&pIVB)));
            GRS_SET_D3D12_DEBUGNAME_COMPTR(pIVB);

            //ʹ��map-memcpy-unmap�󷨽����ݴ������㻺�����
            UINT8* pVertexDataBegin = nullptr;

            GRS_THROW_IF_FAILED(pIVB->Map(0, nullptr, reinterpret_cast<void**>(&pVertexDataBegin)));
            memcpy(pVertexDataBegin, pstVertices, nVertexCnt * sizeof(ST_GRS_VERTEX));
            pIVB->Unmap(0, nullptr);

            //���� Index Buffer ��ʹ��Upload��ʽ��
            g_stBufferResSesc.Width = nIndexCnt * sizeof(UINT);
            GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
                &g_stUploadHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &g_stBufferResSesc
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS(&pIIB)));
            GRS_SET_D3D12_DEBUGNAME_COMPTR(pIIB);

            UINT8* pIndexDataBegin = nullptr;
            GRS_THROW_IF_FAILED(pIIB->Map(0, nullptr, reinterpret_cast<void**>(&pIndexDataBegin)));
            memcpy(pIndexDataBegin, pnIndices, nIndexCnt * sizeof(UINT));
            pIIB->Unmap(0, nullptr);

            //����Vertex Buffer View
            stVBV[0].BufferLocation = pIVB->GetGPUVirtualAddress();
            stVBV[0].StrideInBytes = sizeof(ST_GRS_VERTEX);
            stVBV[0].SizeInBytes = nVertexCnt * sizeof(ST_GRS_VERTEX);

            //����Index Buffer View
            stIBV.BufferLocation = pIIB->GetGPUVirtualAddress();
            stIBV.Format = DXGI_FORMAT_R32_UINT;
            stIBV.SizeInBytes = nIndexCnt * sizeof(UINT);

            GRS_SAFE_FREE(pstVertices);
            GRS_SAFE_FREE(pnIndices);

            // ���ÿʵ������
            //���� Per Instance Data ��ʹ��Upload��ʽ�� iRowCnts�� * iColCnts�и�ʵ��
            g_stBufferResSesc.Width = iRowCnts * iColCnts * sizeof(ST_GRS_PER_INSTANCE);
            GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
                &g_stUploadHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &g_stBufferResSesc
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS(&pIInstanceData)));
            GRS_SET_D3D12_DEBUGNAME_COMPTR(pIInstanceData);

            ST_GRS_PER_INSTANCE* pPerInstanceData = nullptr;
            GRS_THROW_IF_FAILED(pIInstanceData->Map(0, nullptr, reinterpret_cast<void**>(&pPerInstanceData)));

            // ˫��ѭ�������С��м���ÿ��ʾ��������
            int   iColHalf = iColCnts / 2;
            int   iRowHalf = iRowCnts / 2;
            float fDeltaPos = 2.5f;
            float fDeltaX = 0.0f;
            float fDeltaY = 0.0f;
            float fDeltaMetallic = 1.0f / iColCnts;
            float fDeltaRoughness = 1.0f / iRowCnts;
            float fMinValue = 0.04f; //�����Ⱥʹֲڶȵ���Сֵ�������߶�Ϊ0ʱ��û��������
            float fDeltaAO = 0.92f;
            XMFLOAT4 v4Albedo = { 1.00f,0.86f,0.57f,0.0f };

            for ( int iRow = 0; iRow < iRowCnts; iRow++ )
            {// ��ѭ��
                for ( int iCol = 0; iCol < iColCnts; iCol++ )
                {// ��ѭ��
                    // Ϊ�ԣ�0,0,0��Ϊ���ģ������Ͻ������½�����
                    fDeltaX = (iCol - iColHalf) * fDeltaPos;
                    fDeltaY = (iRowHalf - iRow) * fDeltaPos;
                    XMStoreFloat4x4(&pPerInstanceData[iRow * iColCnts + iCol].mxModel2World
                        , XMMatrixTranslation(fDeltaX, fDeltaY, 1.0f));
                    // �����Ȱ��б�С    
                    pPerInstanceData[iRow * iColCnts + iCol].mfMetallic += max(1.0f - (iCol * fDeltaMetallic), fMinValue);

                    // �ֲڶȰ��б��
                    pPerInstanceData[iRow * iColCnts + iCol].mfRoughness += max(iRow * fDeltaRoughness, fMinValue);

                    pPerInstanceData[iRow * iColCnts + iCol].mv4Albedo = v4Albedo;
                    pPerInstanceData[iRow * iColCnts + iCol].mfAO = fDeltaAO;
                }
            }
            pIInstanceData->Unmap(0, nullptr);

            //����Per Instance Data �� Vertex Buffer View
            stVBV[1].BufferLocation = pIInstanceData->GetGPUVirtualAddress();
            stVBV[1].StrideInBytes = sizeof(ST_GRS_PER_INSTANCE);
            stVBV[1].SizeInBytes = iRowCnts * iColCnts * sizeof(ST_GRS_PER_INSTANCE);
        }

        // 10�������Դ���Ͻṹ
        {
            stBeginResBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            stBeginResBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            stBeginResBarrier.Transition.pResource = g_stGPUStatus.m_pIARenderTargets[g_stGPUStatus.m_nFrameIndex].Get();
            stBeginResBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            stBeginResBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            stBeginResBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            stEneResBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            stEneResBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            stEneResBarrier.Transition.pResource = g_stGPUStatus.m_pIARenderTargets[g_stGPUStatus.m_nFrameIndex].Get();
            stEneResBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            stEneResBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            stEneResBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        }

        DWORD dwRet = 0;
        BOOL bExit = FALSE;
        D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle = g_stGPUStatus.m_pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
        D3D12_CPU_DESCRIPTOR_HANDLE stDSVHandle = g_stGPUStatus.m_pIDSVHeap->GetCPUDescriptorHandleForHeapStart();

        FLOAT fStartTime = (FLOAT)::GetTickCount64();
        FLOAT fCurrentTime = fStartTime;
        FLOAT fTimeInSeconds = (fCurrentTime - fStartTime) / 1000.0f;
        FLOAT fDeltaTime = fCurrentTime - fStartTime;

        // �ȹرպ�����һ�£�ʹ��Ϣѭ������������ת
        GRS_THROW_IF_FAILED(pIMainCMDList->Close());
        SetEvent(g_stGPUStatus.m_hEventFence);

        // ��ʼ��ʽ��Ϣѭ��ǰ����ʾ������
        ShowWindow(g_stGPUStatus.m_hWnd, nCmdShow);
        UpdateWindow(g_stGPUStatus.m_hWnd);

        MSG msg = {};
        // 11����Ϣѭ��
        while ( !bExit )
        {
            dwRet = ::MsgWaitForMultipleObjects(1, &g_stGPUStatus.m_hEventFence, FALSE, INFINITE, QS_ALLINPUT);
            switch ( dwRet - WAIT_OBJECT_0 )
            {
            case 0:
            {
                //-----------------------------------------------------------------------------------------------------
                // OnUpdate()
                // Animation Calculate etc.
                //-----------------------------------------------------------------------------------------------------
                // Model Matrix Calc
                // -----
                // ����
                XMMATRIX mxModel = XMMatrixScaling(g_fScaling, g_fScaling, g_fScaling);

                // -----
                // ��ת ��ʱľ����ת
                // -----    

                // -----
                // λ��
                mxModel = XMMatrixMultiply(mxModel, XMMatrixTranslation(0.0f, 0.0f, 0.0f));

                // World Matrix
                XMMATRIX mxWorld = XMMatrixIdentity();

                // ��һ��ע�͵����൱���Ȳ�Ҫȫ�ֵ�����ռ�任
                // mxWorld = XMMatrixMultiply( mxModel, mxWorld );

                XMStoreFloat4x4(&pstCBMVP->mxWorld, mxWorld);

                // View Matrix
                XMMATRIX mxView = XMMatrixLookAtLH(g_v4EyePos, g_v4LookAt, g_v4UpDir);
                XMStoreFloat4x4(&pstCBMVP->mxView, mxView);

                // Projection Matrix

                XMMATRIX mxProjection = XMMatrixPerspectiveFovLH(XM_PIDIV4
                    , (FLOAT)g_stGPUStatus.m_iWndWidth / (FLOAT)g_stGPUStatus.m_iWndHeight
                    , 0.1f
                    , 10000.0f);
                XMStoreFloat4x4(&pstCBMVP->mxProjection, mxProjection);

                // View Matrix * Projection Matrix
                XMMATRIX mxViewProj = XMMatrixMultiply(mxView, mxProjection);
                XMStoreFloat4x4(&pstCBMVP->mxViewProj, mxViewProj);

                // World Matrix * View Matrix * Projection Matrix
                XMMATRIX mxMVP = XMMatrixMultiply(mxWorld, mxViewProj);
                XMStoreFloat4x4(&pstCBMVP->mxMVP, mxMVP);

                // Camera Position
                XMStoreFloat4(&pstCBCameraPos->m_v4CameraPos, g_v4EyePos);

                // Lights
                {
                    float fLightZ = -5.0f;
                    pstCBLights->m_v4LightPos[0] = { 0.0f,0.0f,fLightZ,1.0f };
                    //pstCBLights->m_v4LightColors[0] = { 23.47f,21.31f,20.79f, 1.0f };
                    pstCBLights->m_v4LightColors[0] = { 50.0f,50.0f,50.0f, 1.0f };

                    pstCBLights->m_v4LightPos[1] = { 0.0f,1.0f,fLightZ,1.0f };
                    pstCBLights->m_v4LightColors[1] = { 25.0f,0.0f,0.0f,1.0f };

                    pstCBLights->m_v4LightPos[2] = { 1.0f,-1.0f,fLightZ,1.0f };
                    pstCBLights->m_v4LightColors[2] = { 0.0f,25.0f,0.0f,1.0f };

                    pstCBLights->m_v4LightPos[3] = { -1.0f,-1.0f,fLightZ,1.0f };
                    pstCBLights->m_v4LightColors[3] = { 0.0f,0.0f,25.0f,1.0f };
                }
                //-----------------------------------------------------------------------------------------------------
                // OnRender()
                //----------------------------------------------------------------------------------------------------- 
                //�����������Resetһ��
                GRS_THROW_IF_FAILED(pIMainCMDAlloc->Reset());
                GRS_THROW_IF_FAILED(pIMainCMDList->Reset(pIMainCMDAlloc.Get(), nullptr));

                g_stGPUStatus.m_nFrameIndex = g_stGPUStatus.m_pISwapChain3->GetCurrentBackBufferIndex();

                pIMainCMDList->RSSetViewports(1, &g_stGPUStatus.m_stViewPort);
                pIMainCMDList->RSSetScissorRects(1, &g_stGPUStatus.m_stScissorRect);

                // ͨ����Դ�����ж��󻺳��Ѿ��л���Ͽ��Կ�ʼ��Ⱦ��
                stBeginResBarrier.Transition.pResource = g_stGPUStatus.m_pIARenderTargets[g_stGPUStatus.m_nFrameIndex].Get();
                pIMainCMDList->ResourceBarrier(1, &stBeginResBarrier);

                // ������ȾĿ��
                stRTVHandle = g_stGPUStatus.m_pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
                stRTVHandle.ptr += (size_t)g_stGPUStatus.m_nFrameIndex * g_stGPUStatus.m_nRTVDescriptorSize;
                pIMainCMDList->OMSetRenderTargets(1, &stRTVHandle, FALSE, &stDSVHandle);

                // ������¼�����������ʼ��һ֡����Ⱦ
                pIMainCMDList->ClearRenderTargetView(stRTVHandle, faClearColor, 0, nullptr);
                pIMainCMDList->ClearDepthStencilView(g_stGPUStatus.m_pIDSVHeap->GetCPUDescriptorHandleForHeapStart()
                    , D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
                //-----------------------------------------------------------------------------------------------------
                // Draw !
                //-----------------------------------------------------------------------------------------------------
                // ���ø�ǩ��
                pIMainCMDList->SetGraphicsRootSignature(pIRootSignature.Get());
                // ���ù���״̬����
                pIMainCMDList->SetPipelineState(pIPSOModel.Get());
                // ע������ʹ�õ���Ⱦ�ַ����������б�Ҳ����ͨ����Mesh����
                pIMainCMDList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                // ����һ��������������Slot
                pIMainCMDList->IASetVertexBuffers(0, cnSlotCount, stVBV);
                pIMainCMDList->IASetIndexBuffer(&stIBV);

                ID3D12DescriptorHeap* ppHeaps[] = { pICBVSRVHeap.Get() };
                pIMainCMDList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

                D3D12_GPU_DESCRIPTOR_HANDLE stGPUSRVHandle = { pICBVSRVHeap->GetGPUDescriptorHandleForHeapStart() };
                // ����CBV
                pIMainCMDList->SetGraphicsRootDescriptorTable(0, stGPUSRVHandle);

                // Draw Call��һ���Ի��ƶ��ʵ����
                pIMainCMDList->DrawIndexedInstanced(nIndexCnt, iRowCnts * iColCnts, 0, 0, 0);
                //-----------------------------------------------------------------------------------------------------

                //��һ����Դ���ϣ�����ȷ����Ⱦ�Ѿ����������ύ����ȥ��ʾ��
                stEneResBarrier.Transition.pResource = g_stGPUStatus.m_pIARenderTargets[g_stGPUStatus.m_nFrameIndex].Get();
                pIMainCMDList->ResourceBarrier(1, &stEneResBarrier);

                //�ر������б�����ȥִ����
                GRS_THROW_IF_FAILED(pIMainCMDList->Close());

                //ִ�������б�
                ID3D12CommandList* ppCommandLists[] = { pIMainCMDList.Get() };
                pIMainCMDQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

                //�ύ����
                // ���֧��Tearing ��ô����Tearing��ʽ���ֻ���
                UINT nPresentFlags = (g_stGPUStatus.m_bSupportTearing && !g_stGPUStatus.m_bFullScreen) ? DXGI_PRESENT_ALLOW_TEARING : 0;
                GRS_THROW_IF_FAILED(g_stGPUStatus.m_pISwapChain3->Present(0, nPresentFlags));

                //��ʼͬ��GPU��CPU��ִ�У��ȼ�¼Χ�����ֵ
                const UINT64 n64CurrentFenceValue = g_stGPUStatus.m_n64FenceValue;
                GRS_THROW_IF_FAILED(pIMainCMDQueue->Signal(g_stGPUStatus.m_pIFence.Get(), n64CurrentFenceValue));
                g_stGPUStatus.m_n64FenceValue++;
                GRS_THROW_IF_FAILED(g_stGPUStatus.m_pIFence->SetEventOnCompletion(n64CurrentFenceValue, g_stGPUStatus.m_hEventFence));
                //-----------------------------------------------------------------------------------------------------
            }
            break;
            case 1:
            {// ������Ϣ
                while ( ::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) )
                {
                    if ( WM_QUIT != msg.message )
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
            default:
                break;
            }
        }
    }
    catch ( CAtlException& e )
    {//������COM�쳣
        e;
    }
    catch ( ... )
    {

    }
    return 0;
}

void OnSize(UINT width, UINT height, bool minimized)
{
    if ( (width == g_stGPUStatus.m_iWndWidth && height == g_stGPUStatus.m_iWndHeight)
        || minimized
        || (nullptr == g_stGPUStatus.m_pISwapChain3)
        || (0 == width)
        || (0 == height) )
    {
        return;
    }

    try
    {
        // 1����Ϣѭ����·
        if ( WAIT_OBJECT_0 != ::WaitForSingleObject(g_stGPUStatus.m_hEventFence, INFINITE) )
        {
            GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
        }

        // 2���ͷŻ�ȡ�ĺ󻺳����ӿ�
        for ( UINT i = 0; i < g_nFrameBackBufCount; i++ )
        {
            g_stGPUStatus.m_pIARenderTargets[i].Reset();

        }

        // 3�����������ô�С
        DXGI_SWAP_CHAIN_DESC stSWDesc = {};
        g_stGPUStatus.m_pISwapChain3->GetDesc(&stSWDesc);

        HRESULT hr = g_stGPUStatus.m_pISwapChain3->ResizeBuffers(g_nFrameBackBufCount
            , width
            , height
            , stSWDesc.BufferDesc.Format
            , stSWDesc.Flags);

        GRS_THROW_IF_FAILED(hr);

        // 4���õ���ǰ�󻺳�����
        g_stGPUStatus.m_nFrameIndex = g_stGPUStatus.m_pISwapChain3->GetCurrentBackBufferIndex();

        // 5������任��ĳߴ�
        g_stGPUStatus.m_iWndWidth = width;
        g_stGPUStatus.m_iWndHeight = height;

        // 6������RTV��������
        ComPtr<ID3D12Device> pID3D12Device;
        ComPtr<ID3D12Device4> pID3D12Device4;
        GRS_THROW_IF_FAILED(g_stGPUStatus.m_pIRTVHeap->GetDevice(IID_PPV_ARGS(&pID3D12Device)));
        GRS_THROW_IF_FAILED(pID3D12Device.As(&pID3D12Device4));
        {
            D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle
                = g_stGPUStatus.m_pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
            for ( UINT i = 0; i < g_nFrameBackBufCount; i++ )
            {
                GRS_THROW_IF_FAILED(g_stGPUStatus.m_pISwapChain3->GetBuffer(i
                    , IID_PPV_ARGS(&g_stGPUStatus.m_pIARenderTargets[i])));

                GRS_SET_D3D12_DEBUGNAME_INDEXED_COMPTR(g_stGPUStatus.m_pIARenderTargets, i);

                pID3D12Device4->CreateRenderTargetView(g_stGPUStatus.m_pIARenderTargets[i].Get()
                    , nullptr
                    , stRTVHandle);

                stRTVHandle.ptr += g_stGPUStatus.m_nRTVDescriptorSize;
            }
        }

        // 7��������Ȼ��弰��Ȼ�����������
        {
            g_stDSTex2DDesc.Width = g_stGPUStatus.m_iWndWidth;
            g_stDSTex2DDesc.Height = g_stGPUStatus.m_iWndHeight;

            // �ͷ���Ȼ�����
            g_stGPUStatus.m_pIDepthStencilBuffer.Reset();

            //ʹ����ʽĬ�϶Ѵ���һ��������建������
            //��Ϊ��������Ȼ�������һֱ��ʹ�ã����õ����岻������ֱ��ʹ����ʽ�ѣ�ͼ����
            GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
                &g_stDefautHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &g_stDSTex2DDesc
                , D3D12_RESOURCE_STATE_DEPTH_WRITE
                , &g_stDepthOptimizedClearValue
                , IID_PPV_ARGS(&g_stGPUStatus.m_pIDepthStencilBuffer)
            ));

            // Ȼ��ֱ���ؽ�������
            pID3D12Device4->CreateDepthStencilView(g_stGPUStatus.m_pIDepthStencilBuffer.Get()
                , &g_stDepthStencilDesc
                , g_stGPUStatus.m_pIDSVHeap->GetCPUDescriptorHandleForHeapStart());
        }

        // 8�������ӿڴ�С����ΰ�Χ�д�С
        g_stGPUStatus.m_stViewPort.TopLeftX = 0.0f;
        g_stGPUStatus.m_stViewPort.TopLeftY = 0.0f;
        g_stGPUStatus.m_stViewPort.Width = g_stGPUStatus.m_iWndWidth;
        g_stGPUStatus.m_stViewPort.Height = g_stGPUStatus.m_iWndHeight;

        g_stGPUStatus.m_stScissorRect.left = static_cast<LONG>(g_stGPUStatus.m_stViewPort.TopLeftX);
        g_stGPUStatus.m_stScissorRect.right = static_cast<LONG>(g_stGPUStatus.m_stViewPort.TopLeftX + g_stGPUStatus.m_stViewPort.Width);
        g_stGPUStatus.m_stScissorRect.top = static_cast<LONG>(g_stGPUStatus.m_stViewPort.TopLeftY);
        g_stGPUStatus.m_stScissorRect.bottom = static_cast<LONG>(g_stGPUStatus.m_stViewPort.TopLeftY + g_stGPUStatus.m_stViewPort.Height);

        // 9���������¼�״̬������Ϣ��·��������Ϣѭ��        
        SetEvent(g_stGPUStatus.m_hEventFence);
    }
    catch ( CAtlException& e )
    {//������COM�쳣
        e;
    }
    catch ( ... )
    {

    }

}

void SwitchFullScreenWindow()
{
    if ( g_stGPUStatus.m_bFullScreen )
    {// ��ԭΪ���ڻ�
        SetWindowLong(g_stGPUStatus.m_hWnd, GWL_STYLE, g_stGPUStatus.m_dwWndStyle);

        SetWindowPos(
            g_stGPUStatus.m_hWnd,
            HWND_NOTOPMOST,
            g_stGPUStatus.m_rtWnd.left,
            g_stGPUStatus.m_rtWnd.top,
            g_stGPUStatus.m_rtWnd.right - g_stGPUStatus.m_rtWnd.left,
            g_stGPUStatus.m_rtWnd.bottom - g_stGPUStatus.m_rtWnd.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);

        ShowWindow(g_stGPUStatus.m_hWnd, SW_NORMAL);
    }
    else
    {// ȫ����ʾ
        // ���洰�ڵ�ԭʼ�ߴ�
        GetWindowRect(g_stGPUStatus.m_hWnd, &g_stGPUStatus.m_rtWnd);

        // ʹ�����ޱ߿��Ա�ͻ�������������Ļ��  
        SetWindowLong(g_stGPUStatus.m_hWnd
            , GWL_STYLE
            , g_stGPUStatus.m_dwWndStyle
            & ~(WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_THICKFRAME));

        RECT stFullscreenWindowRect = {};
        ComPtr<IDXGIOutput> pIOutput;
        DXGI_OUTPUT_DESC stOutputDesc = {};
        HRESULT _hr = g_stGPUStatus.m_pISwapChain3->GetContainingOutput(&pIOutput);
        if ( SUCCEEDED(g_stGPUStatus.m_pISwapChain3->GetContainingOutput(&pIOutput)) &&
            SUCCEEDED(pIOutput->GetDesc(&stOutputDesc)) )
        {// ʹ����ʾ�������ʾ�ߴ�
            stFullscreenWindowRect = stOutputDesc.DesktopCoordinates;
        }
        else
        {// ��ȡϵͳ���õķֱ���
            DEVMODE stDevMode = {};
            stDevMode.dmSize = sizeof(DEVMODE);
            EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &stDevMode);

            stFullscreenWindowRect = {
                stDevMode.dmPosition.x,
                stDevMode.dmPosition.y,
                stDevMode.dmPosition.x + static_cast<LONG>(stDevMode.dmPelsWidth),
                stDevMode.dmPosition.y + static_cast<LONG>(stDevMode.dmPelsHeight)
            };
        }

        // ���ô������
        SetWindowPos(
            g_stGPUStatus.m_hWnd,
            HWND_TOPMOST,
            stFullscreenWindowRect.left,
            stFullscreenWindowRect.top,
            stFullscreenWindowRect.right,
            stFullscreenWindowRect.bottom,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);

        ShowWindow(g_stGPUStatus.m_hWnd, SW_MAXIMIZE);
    }

    g_stGPUStatus.m_bFullScreen = !g_stGPUStatus.m_bFullScreen;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch ( message )
    {
    case WM_DESTROY:
    {
        // ��·һ����Ϣѭ��
        if ( WAIT_OBJECT_0 == ::WaitForSingleObject(g_stGPUStatus.m_hEventFence, INFINITE)
            && g_stGPUStatus.m_bSupportTearing
            && g_stGPUStatus.m_pISwapChain3 )
        {// ֧��Tearingʱ���Զ��˳���ȫ��״̬
            g_stGPUStatus.m_pISwapChain3->SetFullscreenState(FALSE, nullptr);
        }
        PostQuitMessage(0);
    }
    break;
    case WM_KEYDOWN:
    {
        USHORT n16KeyCode = (wParam & 0xFF);
        if ( VK_ESCAPE == n16KeyCode )
        {// Esc���˳�
            ::DestroyWindow(hWnd);
        }
        if ( VK_TAB == n16KeyCode )
        {// ��Tab��
        }

        if ( VK_SPACE == n16KeyCode )
        {// ��Space��

        }

        if ( VK_F3 == n16KeyCode )
        {// F3��

        }

        //-------------------------------------------------------------------------------
        // ���������λ���ƶ�����
        float fDelta = 0.2f;
        XMVECTOR vDelta = { 0.0f,0.0f,fDelta,0.0f };
        // Z-��
        if ( VK_UP == n16KeyCode || 'w' == n16KeyCode || 'W' == n16KeyCode )
        {
            g_v4EyePos = XMVectorAdd(g_v4EyePos, vDelta);
            g_v4LookAt = XMVectorAdd(g_v4LookAt, vDelta);
        }

        if ( VK_DOWN == n16KeyCode || 's' == n16KeyCode || 'S' == n16KeyCode )
        {
            g_v4EyePos = XMVectorSubtract(g_v4EyePos, vDelta);
            g_v4LookAt = XMVectorSubtract(g_v4LookAt, vDelta);
        }

        // X-��
        vDelta = { fDelta,0.0f,0.0f,0.0f };
        if ( VK_LEFT == n16KeyCode || 'a' == n16KeyCode || 'A' == n16KeyCode )
        {
            g_v4EyePos = XMVectorAdd(g_v4EyePos, vDelta);
            g_v4LookAt = XMVectorAdd(g_v4LookAt, vDelta);
        }

        if ( VK_RIGHT == n16KeyCode || 'd' == n16KeyCode || 'D' == n16KeyCode )
        {
            g_v4EyePos = XMVectorSubtract(g_v4EyePos, vDelta);
            g_v4LookAt = XMVectorSubtract(g_v4LookAt, vDelta);
        }

        // Y-��
        vDelta = { 0.0f,fDelta,0.0f,0.0f };
        if ( VK_PRIOR == n16KeyCode || 'r' == n16KeyCode || 'R' == n16KeyCode )
        {
            g_v4EyePos = XMVectorAdd(g_v4EyePos, vDelta);
            g_v4LookAt = XMVectorAdd(g_v4LookAt, vDelta);
        }

        if ( VK_NEXT == n16KeyCode || 'f' == n16KeyCode || 'F' == n16KeyCode )
        {
            g_v4EyePos = XMVectorSubtract(g_v4EyePos, vDelta);
            g_v4LookAt = XMVectorSubtract(g_v4LookAt, vDelta);
        }


        if ( VK_ADD == n16KeyCode || VK_OEM_PLUS == n16KeyCode )
        {// + ��

        }

        if ( VK_SUBTRACT == n16KeyCode || VK_OEM_MINUS == n16KeyCode )
        {// - ��
        }
    }
    break;
    case WM_SIZE:
    {
        ::GetClientRect(hWnd, &g_stGPUStatus.m_rtWndClient);
        OnSize(g_stGPUStatus.m_rtWndClient.right - g_stGPUStatus.m_rtWndClient.left
            , g_stGPUStatus.m_rtWndClient.bottom - g_stGPUStatus.m_rtWndClient.top
            , wParam == SIZE_MINIMIZED);
        // ��������ע�͵��ˣ���ȻWin10����ᱨĪ�������UWP�쳣
        //return 0; // ������ϵͳ�������� 
    }
    break;
    case WM_GETMINMAXINFO:
    {// ���ƴ�����С��СΪ1024*768
        MINMAXINFO* pMMInfo = (MINMAXINFO*)lParam;
        pMMInfo->ptMinTrackSize.x = 1024;
        pMMInfo->ptMinTrackSize.y = 768;
        return 0;
    }
    break;
    case WM_SYSKEYDOWN:
    {
        if ( (wParam == VK_RETURN) && (lParam & (1 << 29)) )
        {// ����Alt + Entry
            if ( g_stGPUStatus.m_bSupportTearing )
            {
                SwitchFullScreenWindow();
                return 0;
            }
        }
    }
    break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

BOOL LoadMeshVertex(const CHAR* pszMeshFileName, UINT& nVertexCnt, ST_GRS_VERTEX*& ppVertex, UINT*& ppIndices)
{
    ifstream fin;
    char input;
    BOOL bRet = TRUE;
    try
    {
        fin.open(pszMeshFileName);
        if ( fin.fail() )
        {
            AtlThrow(E_FAIL);
        }
        fin.get(input);
        while ( input != ':' )
        {
            fin.get(input);
        }
        fin >> nVertexCnt;

        fin.get(input);
        while ( input != ':' )
        {
            fin.get(input);
        }
        fin.get(input);
        fin.get(input);

        ppVertex = (ST_GRS_VERTEX*)GRS_CALLOC(nVertexCnt * sizeof(ST_GRS_VERTEX));
        ppIndices = (UINT*)GRS_CALLOC(nVertexCnt * sizeof(UINT));

        for ( UINT i = 0; i < nVertexCnt; i++ )
        {
            fin >> ppVertex[i].m_v4Position.x >> ppVertex[i].m_v4Position.y >> ppVertex[i].m_v4Position.z;
            ppVertex[i].m_v4Position.w = 1.0f;      //��ĵ�4ά������Ϊ1
            fin >> ppVertex[i].m_v2UV.x >> ppVertex[i].m_v2UV.y;
            fin >> ppVertex[i].m_v4Normal.x >> ppVertex[i].m_v4Normal.y >> ppVertex[i].m_v4Normal.z;
            ppVertex[i].m_v4Normal.w = 0.0f;        //�������ĵ�4ά������Ϊ0
            ppIndices[i] = i;
        }
    }
    catch ( CAtlException& e )
    {
        e;
        bRet = FALSE;
    }
    return bRet;
}