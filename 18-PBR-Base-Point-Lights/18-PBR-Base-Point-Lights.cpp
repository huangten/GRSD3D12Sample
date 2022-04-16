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

#include "../Commons/GRS_Mem.h"
#include "../Commons/GRS_D3D12_Utility.h"

using namespace std;
using namespace DirectX;
using namespace Microsoft;
using namespace Microsoft::WRL;

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define GRS_SLEEP(dwMilliseconds)  WaitForSingleObject(GetCurrentThread(),dwMilliseconds)
#define GRS_THROW_IF_FAILED(hr) {HRESULT _hr = (hr);if (FAILED(_hr)){ ATLTRACE("Error: 0x%08x\n",_hr); AtlThrow(_hr); }}
#define GRS_SAFE_RELEASE(p) if(nullptr != (p)){(p)->Release();(p)=nullptr;}
//���������ϱ߽�����㷨 �ڴ�����г��� ���ס
#define GRS_UPPER(A,B) ((size_t)(((A)+((B)-1))&~((B) - 1)))
//��ȡ������
#define GRS_UPPER_DIV(A,B) ((UINT)(((A)+((B)-1))/(B)))

#define GRS_WND_CLASS _T("GRS Game Window Class")
#define GRS_WND_TITLE _T("GRS DirectX12 PBR Base With Point Light Sample")

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

#define GRS_LIGHT_COUNT 8
// lights
struct ST_CB_LIGHTS //: register( b2 )
{
    XMFLOAT4 m_v4LightPos[GRS_LIGHT_COUNT];
    XMFLOAT4 m_v4LightClr[GRS_LIGHT_COUNT];
};

// PBR material parameters
struct ST_CB_PBR_MATERIAL //: register( b3 )
{
    XMFLOAT3   m_v3Albedo;      // ������
    float      m_fMetallic;     // ������
    float      m_fRoughness;    // �ֲڶ�
    float      m_fAO;           // �������ڱ�
};

struct ST_GRS_VERTEX
{
    XMFLOAT4 m_v4Position;		//Position
    XMFLOAT4 m_v4Normal;		//Normal
    XMFLOAT2 m_v2UV;		    //Texcoord
};

TCHAR g_pszAppPath[MAX_PATH] = {};

XMVECTOR g_v4EyePos = XMVectorSet( 0.0f, 0.0f, -3.0f, 0.0f );	//�۾�λ��
XMVECTOR g_v4LookAt = XMVectorSet( 0.0f, 0.0f, 2.0f, 0.0f );		//�۾�������λ��
XMVECTOR g_v4UpDir = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );		    //ͷ�����Ϸ�λ��

// ����ϵ��
float g_fScaling = 1.0f;

// ���ʲ���
float g_fMetallic = 0.01f;      // q-a ������
float g_fRoughness = 0.01f;     // w-s ������
float g_fAO = 0.03f;            // e-d ������

UINT        g_nCurAlbedo = 0;   // Tab���л�
XMFLOAT3    g_pv3Albedo[] =
{
    {0.97f,0.96f,0.91f}     // �� Silver 
    ,{0.91f,0.92f,0.92f}    // �� Aluminum
    ,{0.76f,0.73f,0.69f}    // �� Titanium
    ,{0.77f,0.78f,0.78f}    // �� Iron
    ,{0.83f,0.81f,0.78f}    // �� Platinum
    ,{1.00f,0.85f,0.57f}    // �� Gold
    ,{0.98f,0.90f,0.59f}    // ��ͭ Brass
    ,{0.97f,0.74f,0.62f}    // ͭ Copper
};
const UINT g_nAlbedoCount = _countof( g_pv3Albedo );

// ������ת�Ľ��ٶȣ���λ������/��
double g_fPalstance = 10.0f * XM_PI / 180.0f;

BOOL LoadMeshVertex( const CHAR* pszMeshFileName, UINT& nVertexCnt, ST_GRS_VERTEX*& ppVertex, UINT*& ppIndices );
LRESULT CALLBACK WndProc( HWND, UINT, WPARAM, LPARAM );

int APIENTRY _tWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR    lpCmdLine, int nCmdShow )
{
    int									iWidth = 1280;
    int									iHeight = 800;
    HWND								hWnd = nullptr;
    MSG									msg = {};

    const UINT						    nFrameBackBufCount = 3u;
    UINT								nFrameIndex = 0;
    DXGI_FORMAT				            emRenderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT                         emDepthStencilFormat = DXGI_FORMAT_D32_FLOAT;
    const float						    faClearColor[] = { 0.17647f, 0.549f, 0.941176f, 1.0f };

    UINT								nRTVDescriptorSize = 0U;
    UINT								nCBVSRVDescriptorSize = 0;
    UINT								nSamplerDescriptorSize = 0;

    D3D12_VIEWPORT			            stViewPort = { 0.0f, 0.0f, static_cast<float>( iWidth ), static_cast<float>( iHeight ), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
    D3D12_RECT					        stScissorRect = { 0, 0, static_cast<LONG>( iWidth ), static_cast<LONG>( iHeight ) };

    D3D_FEATURE_LEVEL					emFeatureLevel = D3D_FEATURE_LEVEL_12_1;

    ComPtr<IDXGIFactory6>				pIDXGIFactory6;
    ComPtr<ID3D12Device4>				pID3D12Device4;

    ComPtr<ID3D12CommandQueue>			pIMainCMDQueue;
    ComPtr<ID3D12CommandAllocator>		pIMainCMDAlloc;
    ComPtr<ID3D12GraphicsCommandList>	pIMainCMDList;

    ComPtr<ID3D12Fence>					pIFence;
    UINT64								n64FenceValue = 0ui64;
    HANDLE								hEventFence = nullptr;

    ComPtr<IDXGISwapChain3>				pISwapChain3;
    ComPtr<ID3D12DescriptorHeap>		pIRTVHeap;
    ComPtr<ID3D12Resource>				pIARenderTargets[nFrameBackBufCount];
    ComPtr<ID3D12DescriptorHeap>		pIDSVHeap;
    ComPtr<ID3D12Resource>				pIDepthStencilBuffer;

    const UINT                          nConstBufferCount = 4;  // ��ʾ������Ҫ4��Const Buffer

    ComPtr<ID3D12RootSignature>		    pIRootSignature;
    ComPtr<ID3D12PipelineState>			pIPSOModel;

    ComPtr<ID3D12DescriptorHeap>		pICBVSRVHeap;

    UINT                                nIndexCnt = 0;
    ComPtr<ID3D12Resource>				pIVB;
    ComPtr<ID3D12Resource>				pIIB;

    D3D12_VERTEX_BUFFER_VIEW		    stVBV = {};
    D3D12_INDEX_BUFFER_VIEW			    stIBV = {};

    ComPtr<ID3D12Resource>			    pICBMVP;
    ST_GRS_CB_MVP*                      pstCBMVP = nullptr;
    ComPtr<ID3D12Resource>			    pICBCameraPos;
    ST_CB_CAMERA*                       pstCBCameraPos = nullptr;
    ComPtr<ID3D12Resource>			    pICBLights;
    ST_CB_LIGHTS*                       pstCBLights = nullptr;
    ComPtr<ID3D12Resource>			    pICBPBRMaterial;
    ST_CB_PBR_MATERIAL*                 pstCBPBRMaterial = nullptr;

    D3D12_HEAP_PROPERTIES stDefaultHeapProps = {};
    stDefaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    stDefaultHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    stDefaultHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    stDefaultHeapProps.CreationNodeMask = 0;
    stDefaultHeapProps.VisibleNodeMask = 0;

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

    USES_CONVERSION;
    try
    {
        ::CoInitialize( nullptr );  //for WIC & COM

        // 0���õ���ǰ�Ĺ���Ŀ¼����������ʹ�����·�������ʸ�����Դ�ļ�
        {
            if ( 0 == ::GetModuleFileName( nullptr, g_pszAppPath, MAX_PATH ) )
            {
                GRS_THROW_IF_FAILED( HRESULT_FROM_WIN32( GetLastError() ) );
            }

            WCHAR* lastSlash = _tcsrchr( g_pszAppPath, _T( '\\' ) );
            if ( lastSlash )
            {//ɾ��Exe�ļ���
                *( lastSlash ) = _T( '\0' );
            }

            lastSlash = _tcsrchr( g_pszAppPath, _T( '\\' ) );
            if ( lastSlash )
            {//ɾ��x64·��
                *( lastSlash ) = _T( '\0' );
            }

            lastSlash = _tcsrchr( g_pszAppPath, _T( '\\' ) );
            if ( lastSlash )
            {//ɾ��Debug �� Release·��
                *( lastSlash + 1 ) = _T( '\0' );
            }
        }

        // 1����������
        {
            WNDCLASSEX wcex = {};
            wcex.cbSize = sizeof( WNDCLASSEX );
            wcex.style = CS_GLOBALCLASS;
            wcex.lpfnWndProc = WndProc;
            wcex.cbClsExtra = 0;
            wcex.cbWndExtra = 0;
            wcex.hInstance = hInstance;
            wcex.hCursor = LoadCursor( nullptr, IDC_ARROW );
            wcex.hbrBackground = (HBRUSH) GetStockObject( NULL_BRUSH );		//��ֹ���ĵı����ػ�
            wcex.lpszClassName = GRS_WND_CLASS;
            RegisterClassEx( &wcex );

            DWORD dwWndStyle = WS_OVERLAPPED | WS_SYSMENU;
            RECT rtWnd = { 0, 0, iWidth, iHeight };
            AdjustWindowRect( &rtWnd, dwWndStyle, FALSE );

            // ���㴰�ھ��е���Ļ����
            INT posX = ( GetSystemMetrics( SM_CXSCREEN ) - rtWnd.right - rtWnd.left ) / 2;
            INT posY = ( GetSystemMetrics( SM_CYSCREEN ) - rtWnd.bottom - rtWnd.top ) / 2;

            hWnd = CreateWindowW( GRS_WND_CLASS
                , GRS_WND_TITLE
                , dwWndStyle
                , posX
                , posY
                , rtWnd.right - rtWnd.left
                , rtWnd.bottom - rtWnd.top
                , nullptr
                , nullptr
                , hInstance
                , nullptr );

            if ( !hWnd )
            {
                GRS_THROW_IF_FAILED( HRESULT_FROM_WIN32( GetLastError() ) );
            }
        }

        // 2������DXGI Factory����
        {
            UINT nDXGIFactoryFlags = 0U;

#if defined(_DEBUG)
            // ����ʾ��ϵͳ�ĵ���֧��
            ComPtr<ID3D12Debug> debugController;
            if ( SUCCEEDED( D3D12GetDebugInterface( IID_PPV_ARGS( &debugController ) ) ) )
            {
                debugController->EnableDebugLayer();
                // �򿪸��ӵĵ���֧��
                nDXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
            }
#endif
            ComPtr<IDXGIFactory5> pIDXGIFactory5;
            GRS_THROW_IF_FAILED( CreateDXGIFactory2( nDXGIFactoryFlags, IID_PPV_ARGS( &pIDXGIFactory5 ) ) );
            GRS_SET_DXGI_DEBUGNAME_COMPTR( pIDXGIFactory5 );

            //��ȡIDXGIFactory6�ӿ�
            GRS_THROW_IF_FAILED( pIDXGIFactory5.As( &pIDXGIFactory6 ) );
            GRS_SET_DXGI_DEBUGNAME_COMPTR( pIDXGIFactory6 );
        }

        // 3��ö�������������豸
        { // ֱ��ö��������ܵ��Կ�������ʾ��
            ComPtr<IDXGIAdapter1> pIAdapter1;
            GRS_THROW_IF_FAILED( pIDXGIFactory6->EnumAdapterByGpuPreference( 0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS( &pIAdapter1 ) ) );
            GRS_SET_DXGI_DEBUGNAME_COMPTR( pIAdapter1 );

            // ����D3D12.1���豸
            GRS_THROW_IF_FAILED( D3D12CreateDevice( pIAdapter1.Get(), emFeatureLevel, IID_PPV_ARGS( &pID3D12Device4 ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pID3D12Device4 );

            DXGI_ADAPTER_DESC1 stAdapterDesc = {};
            TCHAR pszWndTitle[MAX_PATH] = {};
            GRS_THROW_IF_FAILED( pIAdapter1->GetDesc1( &stAdapterDesc ) );
            ::GetWindowText( hWnd, pszWndTitle, MAX_PATH );
            StringCchPrintf( pszWndTitle, MAX_PATH, _T( "%s (GPU[0]:%s)" ), pszWndTitle, stAdapterDesc.Description );
            ::SetWindowText( hWnd, pszWndTitle );

            // �õ�ÿ��������Ԫ�صĴ�С
            nRTVDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_RTV );
            nSamplerDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER );
            nCBVSRVDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
        }

        // 4�������������&�����б�
        {
            D3D12_COMMAND_QUEUE_DESC stQueueDesc = {};
            stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommandQueue( &stQueueDesc, IID_PPV_ARGS( &pIMainCMDQueue ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIMainCMDQueue );

            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS( &pIMainCMDAlloc ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIMainCMDAlloc );

            // ����ͼ�������б�
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommandList(
                0, D3D12_COMMAND_LIST_TYPE_DIRECT, pIMainCMDAlloc.Get(), nullptr, IID_PPV_ARGS( &pIMainCMDList ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIMainCMDList );
        }

        // 5����������������Ȼ��塢�������ѡ�������
        {
            DXGI_SWAP_CHAIN_DESC1 stSwapChainDesc = {};
            stSwapChainDesc.BufferCount = nFrameBackBufCount;
            stSwapChainDesc.Width = 0;//iWidth;
            stSwapChainDesc.Height = 0;// iHeight;
            stSwapChainDesc.Format = emRenderTargetFormat;
            stSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            stSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            stSwapChainDesc.SampleDesc.Count = 1;
            stSwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
            stSwapChainDesc.Scaling = DXGI_SCALING_NONE;

            // ��������Ҫ������У�Present����Ҫִ��
            ComPtr<IDXGISwapChain1> pISwapChain1;
            GRS_THROW_IF_FAILED( pIDXGIFactory6->CreateSwapChainForHwnd(
                pIMainCMDQueue.Get(), hWnd, &stSwapChainDesc, nullptr, nullptr, &pISwapChain1 ) );

            GRS_SET_DXGI_DEBUGNAME_COMPTR( pISwapChain1 );

            GRS_THROW_IF_FAILED( pISwapChain1.As( &pISwapChain3 ) );

            GRS_SET_DXGI_DEBUGNAME_COMPTR( pISwapChain3 );

            //�õ���ǰ�󻺳�������ţ�Ҳ������һ����Ҫ������ʾ�Ļ����������
            //ע��˴�ʹ���˸߰汾��SwapChain�ӿڵĺ���
            nFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

            //����RTV(��ȾĿ����ͼ)��������(����ѵĺ���Ӧ�����Ϊ������߹̶���СԪ�صĹ̶���С�Դ��)
            {
                D3D12_DESCRIPTOR_HEAP_DESC stRTVHeapDesc = {};
                stRTVHeapDesc.NumDescriptors = nFrameBackBufCount;
                stRTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
                stRTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

                GRS_THROW_IF_FAILED( pID3D12Device4->CreateDescriptorHeap( &stRTVHeapDesc, IID_PPV_ARGS( &pIRTVHeap ) ) );
                GRS_SET_D3D12_DEBUGNAME_COMPTR( pIRTVHeap );
            }

           //����RTV��������
            {
                D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle
                    = pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
                for ( UINT i = 0; i < nFrameBackBufCount; i++ )
                {
                    GRS_THROW_IF_FAILED( pISwapChain3->GetBuffer( i, IID_PPV_ARGS( &pIARenderTargets[i] ) ) );
                    GRS_SET_D3D12_DEBUGNAME_INDEXED_COMPTR( pIARenderTargets, i );
                    pID3D12Device4->CreateRenderTargetView( pIARenderTargets[i].Get(), nullptr, stRTVHandle );
                    stRTVHandle.ptr += nRTVDescriptorSize;
                }
            }

            // �ر�ALT+ENTER���л�ȫ���Ĺ��ܣ���Ϊ����û��ʵ��OnSize���������ȹر�
            // ע�⽫�������Alt+Enter��ϼ��Ķ����ŵ����������Ϊֱ�����ʱ��DXGI Factoryͨ��Create Swapchain������֪����HWND���ĸ�����
            // ��Ϊ��ϼ��Ĵ�����Windows�ж��ǻ��ڴ��ڹ��̵ģ�û�д��ھ���Ͳ�֪�����ĸ������Լ����ڹ��̣�֮ǰ�����ξ�û��������
            GRS_THROW_IF_FAILED( pIDXGIFactory6->MakeWindowAssociation( hWnd, DXGI_MWA_NO_ALT_ENTER ) );


            // ������Ȼ��塢��Ȼ�����������
            D3D12_DEPTH_STENCIL_VIEW_DESC stDepthStencilDesc = {};
            stDepthStencilDesc.Format = emDepthStencilFormat;
            stDepthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            stDepthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

            D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
            depthOptimizedClearValue.Format = emDepthStencilFormat;
            depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
            depthOptimizedClearValue.DepthStencil.Stencil = 0;

            D3D12_RESOURCE_DESC stDSTex2DDesc = {};
            stDSTex2DDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            stDSTex2DDesc.Alignment = 0;
            stDSTex2DDesc.Width = iWidth;
            stDSTex2DDesc.Height = iHeight;
            stDSTex2DDesc.DepthOrArraySize = 1;
            stDSTex2DDesc.MipLevels = 0;
            stDSTex2DDesc.Format = emDepthStencilFormat;
            stDSTex2DDesc.SampleDesc.Count = 1;
            stDSTex2DDesc.SampleDesc.Quality = 0;
            stDSTex2DDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            stDSTex2DDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

            //ʹ����ʽĬ�϶Ѵ���һ��������建������
            //��Ϊ��������Ȼ�������һֱ��ʹ�ã����õ����岻������ֱ��ʹ����ʽ�ѣ�ͼ����
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommittedResource(
                &stDefaultHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &stDSTex2DDesc
                , D3D12_RESOURCE_STATE_DEPTH_WRITE
                , &depthOptimizedClearValue
                , IID_PPV_ARGS( &pIDepthStencilBuffer )
            ) );

            //==============================================================================================
            D3D12_DESCRIPTOR_HEAP_DESC stDSVHeapDesc = {};
            stDSVHeapDesc.NumDescriptors = 1;
            stDSVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            stDSVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateDescriptorHeap( &stDSVHeapDesc, IID_PPV_ARGS( &pIDSVHeap ) ) );

            pID3D12Device4->CreateDepthStencilView( pIDepthStencilBuffer.Get(), &stDepthStencilDesc, pIDSVHeap->GetCPUDescriptorHandleForHeapStart() );
        }

        // 6������Χ��
        if ( TRUE )
        {
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( &pIFence ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIFence );

            n64FenceValue = 1;

            // ����һ��Eventͬ���������ڵȴ�Χ���¼�֪ͨ
            hEventFence = CreateEvent( nullptr, FALSE, FALSE, nullptr );
            if ( hEventFence == nullptr )
            {
                GRS_THROW_IF_FAILED( HRESULT_FROM_WIN32( GetLastError() ) );
            }
        }

        // 7������Shader������Ⱦ����״̬����
        {
            // �ȴ�����ǩ��
            D3D12_FEATURE_DATA_ROOT_SIGNATURE stFeatureData = {};
            // ����Ƿ�֧��V1.1�汾�ĸ�ǩ��
            stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
            if ( FAILED( pID3D12Device4->CheckFeatureSupport( D3D12_FEATURE_ROOT_SIGNATURE, &stFeatureData, sizeof( stFeatureData ) ) ) )
            {// 1.0�� ֱ�Ӷ��쳣�˳���
                GRS_THROW_IF_FAILED( E_NOTIMPL );
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
            stRootSignatureDesc.Desc_1_1.NumParameters = _countof( stRootParameters1 );
            stRootSignatureDesc.Desc_1_1.pParameters = stRootParameters1;
            stRootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
            stRootSignatureDesc.Desc_1_1.pStaticSamplers = nullptr;

            ComPtr<ID3DBlob> pISignatureBlob;
            ComPtr<ID3DBlob> pIErrorBlob;

            HRESULT _hr = D3D12SerializeVersionedRootSignature( &stRootSignatureDesc, &pISignatureBlob, &pIErrorBlob );

            if ( FAILED( _hr ) )
            {
                ATLTRACE( "Create Root Signature Error��%s\n", (CHAR*) ( pIErrorBlob ? pIErrorBlob->GetBufferPointer() : "δ֪����" ) );
                AtlThrow( _hr );
            }

            GRS_THROW_IF_FAILED( pID3D12Device4->CreateRootSignature( 0, pISignatureBlob->GetBufferPointer()
                , pISignatureBlob->GetBufferSize(), IID_PPV_ARGS( &pIRootSignature ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIRootSignature );

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
            StringCchPrintf( pszShaderFileName, MAX_PATH, _T( "%s18-PBR-Base-Point-Lights\\Shader\\VS.hlsl" ), g_pszAppPath );
            HRESULT hr = D3DCompileFromFile( pszShaderFileName, nullptr, nullptr, "VSMain", "vs_5_1", compileFlags, 0, &pIVSModel, &pIErrorBlob );

            if ( FAILED( hr ) )
            {
                ATLTRACE( "���� Vertex Shader��\"%s\" ��������%s\n"
                    , T2A( pszShaderFileName )
                    , pIErrorBlob ? pIErrorBlob->GetBufferPointer() : "�޷���ȡ�ļ���" );
                GRS_THROW_IF_FAILED( hr );
            }
            pIErrorBlob.Reset();
            StringCchPrintf( pszShaderFileName, MAX_PATH, _T( "%s18-PBR-Base-Point-Lights\\Shader\\PS_PBR_Base_Point_Lights.hlsl" ), g_pszAppPath );
            hr = D3DCompileFromFile( pszShaderFileName, nullptr, nullptr, "PSMain", "ps_5_1", compileFlags, 0, &pIPSModel, &pIErrorBlob );
            if ( FAILED( hr ) )
            {
                ATLTRACE( "���� Pixel Shader��\"%s\" ��������%s\n"
                    , T2A( pszShaderFileName )
                    , pIErrorBlob ? pIErrorBlob->GetBufferPointer() : "�޷���ȡ�ļ���" );
                GRS_THROW_IF_FAILED( hr );
            }

            D3D12_INPUT_ELEMENT_DESC stIALayoutSphere[] =
            {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "NORMAL",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            };

           // ���� graphics pipeline state object (PSO)����
            D3D12_GRAPHICS_PIPELINE_STATE_DESC stPSODesc = {};
            stPSODesc.InputLayout = { stIALayoutSphere, _countof( stIALayoutSphere ) };

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

            stPSODesc.DSVFormat = emDepthStencilFormat;
            stPSODesc.DepthStencilState.DepthEnable = TRUE;
            stPSODesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;//������Ȼ���д�빦��
            stPSODesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;     //��Ȳ��Ժ�������ֵΪ��ͨ����Ȳ��ԣ�
            stPSODesc.DepthStencilState.StencilEnable = FALSE;

            stPSODesc.SampleMask = UINT_MAX;
            stPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            stPSODesc.NumRenderTargets = 1;
            stPSODesc.RTVFormats[0] = emRenderTargetFormat;
            stPSODesc.SampleDesc.Count = 1;

            GRS_THROW_IF_FAILED( pID3D12Device4->CreateGraphicsPipelineState( &stPSODesc, IID_PPV_ARGS( &pIPSOModel ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIPSOModel );
        }

        // 8������ SRV CBV Sample��
        if ( TRUE )
        {
            //��������ͼ��������CBV����������һ������������
            D3D12_DESCRIPTOR_HEAP_DESC stSRVHeapDesc = {};

            stSRVHeapDesc.NumDescriptors = nConstBufferCount; // nConstBufferCount * CBV + nTextureCount * SRV
            stSRVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            stSRVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

            GRS_THROW_IF_FAILED( pID3D12Device4->CreateDescriptorHeap( &stSRVHeapDesc, IID_PPV_ARGS( &pICBVSRVHeap ) ) );

            GRS_SET_D3D12_DEBUGNAME_COMPTR( pICBVSRVHeap );
        }

        // 9��������������Ͷ�Ӧ������
        {
            // MVP Const Buffer
            stBufferResSesc.Width = GRS_UPPER( sizeof( ST_GRS_CB_MVP ), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT );
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommittedResource(
                &stUploadHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &stBufferResSesc //ע�⻺��ߴ�����Ϊ256�߽�����С
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS( &pICBMVP ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pICBMVP );

            // Map ֮��Ͳ���Unmap�� ֱ�Ӹ������ݽ�ȥ ����ÿ֡������map-copy-unmap�˷�ʱ����
            GRS_THROW_IF_FAILED( pICBMVP->Map( 0, nullptr, reinterpret_cast<void**>( &pstCBMVP ) ) );

            stBufferResSesc.Width = GRS_UPPER( sizeof( ST_CB_CAMERA ), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT );
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommittedResource(
                &stUploadHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &stBufferResSesc //ע�⻺��ߴ�����Ϊ256�߽�����С
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS( &pICBCameraPos ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pICBCameraPos );

            // Map ֮��Ͳ���Unmap�� ֱ�Ӹ������ݽ�ȥ ����ÿ֡������map-copy-unmap�˷�ʱ����
            GRS_THROW_IF_FAILED( pICBCameraPos->Map( 0, nullptr, reinterpret_cast<void**>( &pstCBCameraPos ) ) );

            stBufferResSesc.Width = GRS_UPPER( sizeof( ST_CB_LIGHTS ), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT );
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommittedResource(
                &stUploadHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &stBufferResSesc //ע�⻺��ߴ�����Ϊ256�߽�����С
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS( &pICBLights ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pICBLights );

            // Map ֮��Ͳ���Unmap�� ֱ�Ӹ������ݽ�ȥ ����ÿ֡������map-copy-unmap�˷�ʱ����
            GRS_THROW_IF_FAILED( pICBLights->Map( 0, nullptr, reinterpret_cast<void**>( &pstCBLights ) ) );

            stBufferResSesc.Width = GRS_UPPER( sizeof( ST_CB_PBR_MATERIAL ), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT );
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommittedResource(
                &stUploadHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &stBufferResSesc //ע�⻺��ߴ�����Ϊ256�߽�����С
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS( &pICBPBRMaterial ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pICBPBRMaterial );

            // Map ֮��Ͳ���Unmap�� ֱ�Ӹ������ݽ�ȥ ����ÿ֡������map-copy-unmap�˷�ʱ����
            GRS_THROW_IF_FAILED( pICBPBRMaterial->Map( 0, nullptr, reinterpret_cast<void**>( &pstCBPBRMaterial ) ) );

            // Create Const Buffer View 
            D3D12_CPU_DESCRIPTOR_HANDLE stCBVHandle = pICBVSRVHeap->GetCPUDescriptorHandleForHeapStart();

            // ������һ��CBV ������� �Ӿ��� ͸�Ӿ��� 
            D3D12_CONSTANT_BUFFER_VIEW_DESC stCBVDesc = {};
            stCBVDesc.BufferLocation = pICBMVP->GetGPUVirtualAddress();
            stCBVDesc.SizeInBytes = (UINT) GRS_UPPER( sizeof( ST_GRS_CB_MVP ), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT );
            pID3D12Device4->CreateConstantBufferView( &stCBVDesc, stCBVHandle );

            stCBVHandle.ptr += nCBVSRVDescriptorSize;
            stCBVDesc.BufferLocation = pICBCameraPos->GetGPUVirtualAddress();
            stCBVDesc.SizeInBytes = (UINT) GRS_UPPER( sizeof( ST_CB_CAMERA ), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT );
            pID3D12Device4->CreateConstantBufferView( &stCBVDesc, stCBVHandle );

            stCBVHandle.ptr += nCBVSRVDescriptorSize;
            stCBVDesc.BufferLocation = pICBLights->GetGPUVirtualAddress();
            stCBVDesc.SizeInBytes = (UINT) GRS_UPPER( sizeof( ST_CB_LIGHTS ), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT );
            pID3D12Device4->CreateConstantBufferView( &stCBVDesc, stCBVHandle );

            stCBVHandle.ptr += nCBVSRVDescriptorSize;
            stCBVDesc.BufferLocation = pICBPBRMaterial->GetGPUVirtualAddress();
            stCBVDesc.SizeInBytes = (UINT) GRS_UPPER( sizeof( ST_CB_PBR_MATERIAL ), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT );
            pID3D12Device4->CreateConstantBufferView( &stCBVDesc, stCBVHandle );
        }

        // 10������D3D12��ģ����Դ������������
        {
            CHAR pszMeshFile[MAX_PATH] = {};
            StringCchPrintfA( pszMeshFile, MAX_PATH, "%sAssets\\sphere.txt", T2A( g_pszAppPath ) );

            ST_GRS_VERTEX* pstVertices = nullptr;
            UINT* pnIndices = nullptr;
            UINT  nVertexCnt = 0;
            LoadMeshVertex( pszMeshFile, nVertexCnt, pstVertices, pnIndices );
            nIndexCnt = nVertexCnt;

            stBufferResSesc.Width = nVertexCnt * sizeof( ST_GRS_VERTEX );
            //���� Vertex Buffer ��ʹ��Upload��ʽ��
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommittedResource(
                &stUploadHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &stBufferResSesc
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS( &pIVB ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIVB );

            //ʹ��map-memcpy-unmap�󷨽����ݴ������㻺�����
            UINT8* pVertexDataBegin = nullptr;
            D3D12_RANGE stReadRange = { 0, 0 };		// We do not intend to read from this resource on the CPU.

            GRS_THROW_IF_FAILED( pIVB->Map( 0, &stReadRange, reinterpret_cast<void**>( &pVertexDataBegin ) ) );
            memcpy( pVertexDataBegin, pstVertices, nVertexCnt * sizeof( ST_GRS_VERTEX ) );
            pIVB->Unmap( 0, nullptr );

            //���� Index Buffer ��ʹ��Upload��ʽ��
            stBufferResSesc.Width = nIndexCnt * sizeof( UINT );
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommittedResource(
                &stUploadHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &stBufferResSesc
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS( &pIIB ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIIB );

            UINT8* pIndexDataBegin = nullptr;
            GRS_THROW_IF_FAILED( pIIB->Map( 0, &stReadRange, reinterpret_cast<void**>( &pIndexDataBegin ) ) );
            memcpy( pIndexDataBegin, pnIndices, nIndexCnt * sizeof( UINT ) );
            pIIB->Unmap( 0, nullptr );

            //����Vertex Buffer View
            stVBV.BufferLocation = pIVB->GetGPUVirtualAddress();
            stVBV.StrideInBytes = sizeof( ST_GRS_VERTEX );
            stVBV.SizeInBytes = nVertexCnt * sizeof( ST_GRS_VERTEX );

            //����Index Buffer View
            stIBV.BufferLocation = pIIB->GetGPUVirtualAddress();
            stIBV.Format = DXGI_FORMAT_R32_UINT;
            stIBV.SizeInBytes = nIndexCnt * sizeof( UINT );

            GRS_SAFE_FREE( pstVertices );
            GRS_SAFE_FREE( pnIndices );
        }

        DWORD dwRet = 0;
        BOOL bExit = FALSE;
        D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle = pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
        D3D12_CPU_DESCRIPTOR_HANDLE stDSVHandle = pIDSVHeap->GetCPUDescriptorHandleForHeapStart();

        GRS_THROW_IF_FAILED( pIMainCMDList->Close() );
        ::SetEvent( hEventFence );

        FLOAT fStartTime = ( FLOAT )::GetTickCount64();
        FLOAT fCurrentTime = fStartTime;
        //������ת�Ƕ���Ҫ�ı���
        double dModelRotationYAngle = 0.0f;

        ShowWindow( hWnd, nCmdShow );
        UpdateWindow( hWnd );

        // 16����Ϣѭ��
        while ( !bExit )
        {
            dwRet = ::MsgWaitForMultipleObjects( 1, &hEventFence, FALSE, INFINITE, QS_ALLINPUT );
            switch ( dwRet - WAIT_OBJECT_0 )
            {
            case 0:
            {
                //-----------------------------------------------------------------------------------------------------
                // OnUpdate()
                // Model Matrix
                // -----
                // ����
                XMMATRIX mxModel = XMMatrixScaling( g_fScaling, g_fScaling, g_fScaling );
                // -----
                // ��ת
                fCurrentTime = ( float )::GetTickCount64();
                //������ת�ĽǶȣ���ת�Ƕ�(����) = ʱ��(��) * ���ٶ�(����/��)
                //�����������൱�ھ�����Ϸ��Ϣѭ���е�OnUpdate��������Ҫ��������
                dModelRotationYAngle += ( ( fCurrentTime - fStartTime ) / 1000.0f ) * g_fPalstance;
                fStartTime = fCurrentTime;
                //��ת�Ƕ���2PI���ڵı�����ȥ����������ֻ�������0���ȿ�ʼ��С��2PI�Ļ��ȼ���
                if ( dModelRotationYAngle > XM_2PI )
                {
                    dModelRotationYAngle = fmod( dModelRotationYAngle, XM_2PI );
                }
                //���� World ���� �����Ǹ���ת����
                mxModel = XMMatrixMultiply( mxModel, XMMatrixRotationY( static_cast<float>( dModelRotationYAngle ) ) );

                // -----
                // λ��
                mxModel = XMMatrixMultiply( mxModel, XMMatrixTranslation( 0.0f, 0.0f, 0.0f ) );

                // World Matrix
                XMMATRIX mxWorld = XMMatrixIdentity();
                mxWorld = XMMatrixMultiply( mxModel, mxWorld );
                XMStoreFloat4x4( &pstCBMVP->mxWorld, mxWorld );

                // View Matrix
                XMMATRIX mxView = XMMatrixLookAtLH( g_v4EyePos, g_v4LookAt, g_v4UpDir );
                XMStoreFloat4x4( &pstCBMVP->mxView, mxView );

                // Projection Matrix
                XMMATRIX mxProjection = XMMatrixPerspectiveFovLH( XM_PIDIV4, (FLOAT) iWidth / (FLOAT) iHeight, 0.1f, 10000.0f );
                XMStoreFloat4x4( &pstCBMVP->mxProjection, mxProjection );

                // View Matrix * Projection Matrix
                XMMATRIX mxViewProj = XMMatrixMultiply( mxView, mxProjection );
                XMStoreFloat4x4( &pstCBMVP->mxViewProj, mxViewProj );

                // World Matrix * View Matrix * Projection Matrix
                XMMATRIX mxMVP = XMMatrixMultiply( mxWorld, mxViewProj );
                XMStoreFloat4x4( &pstCBMVP->mxMVP, mxMVP );

                // Camera Position
                XMStoreFloat4( &pstCBCameraPos->m_v4CameraPos, g_v4EyePos );

                // Lights
                {
                    float fZ = -3.0f;
                    // 4��ǰ�õ�
                    pstCBLights->m_v4LightPos[0] = { 0.0f,0.0f,fZ,1.0f };
                    pstCBLights->m_v4LightClr[0] = { 23.47f,21.31f,20.79f, 1.0f };

                    pstCBLights->m_v4LightPos[1] = { 0.0f,3.0f,fZ,1.0f };
                    pstCBLights->m_v4LightClr[1] = { 53.47f,41.31f,40.79f, 1.0f };

                    pstCBLights->m_v4LightPos[2] = { 3.0f,-3.0f,fZ,1.0f };
                    pstCBLights->m_v4LightClr[2] = { 23.47f,21.31f,20.79f, 1.0f };

                    pstCBLights->m_v4LightPos[3] = { -3.0f,-3.0f,fZ,1.0f };
                    pstCBLights->m_v4LightClr[3] = { 23.47f,21.31f,20.79f, 1.0f };

                    // ������λ��
                    fZ = 0.0f;
                    float fY = 5.0f;
                    pstCBLights->m_v4LightPos[4] = { 0.0f,0.0f,fZ,1.0f };
                    pstCBLights->m_v4LightClr[4] = { 23.47f,21.31f,20.79f, 1.0f };

                    pstCBLights->m_v4LightPos[5] = { 0.0f,-fY,fZ,1.0f };
                    pstCBLights->m_v4LightClr[5] = { 53.47f,41.31f,40.79f, 1.0f };

                    pstCBLights->m_v4LightPos[6] = { fY,fY,fZ,1.0f };
                    pstCBLights->m_v4LightClr[6] = { 23.47f,21.31f,20.79f, 1.0f };

                    pstCBLights->m_v4LightPos[7] = { -fY,fY,fZ,1.0f };
                    pstCBLights->m_v4LightClr[7] = { 23.47f,21.31f,20.79f, 1.0f };
                }

                // PBR Material          
                pstCBPBRMaterial->m_v3Albedo = g_pv3Albedo[g_nCurAlbedo];
                pstCBPBRMaterial->m_fMetallic = g_fMetallic; // 0.04 - 1.0 ֮��
                pstCBPBRMaterial->m_fRoughness = g_fRoughness;
                pstCBPBRMaterial->m_fAO = g_fAO;
                //-----------------------------------------------------------------------------------------------------

                //-----------------------------------------------------------------------------------------------------
                // OnRender()
                //----------------------------------------------------------------------------------------------------- 
                //�����������Resetһ��
                GRS_THROW_IF_FAILED( pIMainCMDAlloc->Reset() );
                //Reset�����б�������ָ�������������PSO����
                GRS_THROW_IF_FAILED( pIMainCMDList->Reset( pIMainCMDAlloc.Get(), pIPSOModel.Get() ) );

                nFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

                pIMainCMDList->RSSetViewports( 1, &stViewPort );
                pIMainCMDList->RSSetScissorRects( 1, &stScissorRect );

                // ͨ����Դ�����ж��󻺳��Ѿ��л���Ͽ��Կ�ʼ��Ⱦ��
                stResStateTransBarrier.Transition.pResource = pIARenderTargets[nFrameIndex].Get();
                stResStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
                stResStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
                pIMainCMDList->ResourceBarrier( 1, &stResStateTransBarrier );

                //��ʼ��¼����
                stRTVHandle = pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
                stRTVHandle.ptr += (size_t) nFrameIndex * nRTVDescriptorSize;

                //������ȾĿ��
                pIMainCMDList->OMSetRenderTargets( 1, &stRTVHandle, FALSE, &stDSVHandle );

                // ������¼�����������ʼ��һ֡����Ⱦ
                pIMainCMDList->ClearRenderTargetView( stRTVHandle, faClearColor, 0, nullptr );
                pIMainCMDList->ClearDepthStencilView( pIDSVHeap->GetCPUDescriptorHandleForHeapStart()
                    , D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr );

                // ��Ⱦ����
                pIMainCMDList->SetGraphicsRootSignature( pIRootSignature.Get() );

                // ע������ʹ�õ���Ⱦ�ַ����������б�Ҳ����ͨ����Mesh����
                pIMainCMDList->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

                pIMainCMDList->IASetVertexBuffers( 0, 1, &stVBV );
                pIMainCMDList->IASetIndexBuffer( &stIBV );

                ID3D12DescriptorHeap* ppHeaps[] = { pICBVSRVHeap.Get() };
                pIMainCMDList->SetDescriptorHeaps( _countof( ppHeaps ), ppHeaps );

                D3D12_GPU_DESCRIPTOR_HANDLE stGPUSRVHandle = { pICBVSRVHeap->GetGPUDescriptorHandleForHeapStart() };
                // ����CBV
                pIMainCMDList->SetGraphicsRootDescriptorTable( 0, stGPUSRVHandle );

                // Draw Call
                pIMainCMDList->DrawIndexedInstanced( nIndexCnt, 1, 0, 0, 0 );

                //��һ����Դ���ϣ�����ȷ����Ⱦ�Ѿ����������ύ����ȥ��ʾ��
                stResStateTransBarrier.Transition.pResource = pIARenderTargets[nFrameIndex].Get();
                stResStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
                stResStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
                pIMainCMDList->ResourceBarrier( 1, &stResStateTransBarrier );

                //�ر������б�����ȥִ����
                GRS_THROW_IF_FAILED( pIMainCMDList->Close() );

                //ִ�������б�
                ID3D12CommandList* ppCommandLists[] = { pIMainCMDList.Get() };
                pIMainCMDQueue->ExecuteCommandLists( _countof( ppCommandLists ), ppCommandLists );

                //�ύ����
                GRS_THROW_IF_FAILED( pISwapChain3->Present( 1, 0 ) );

                //��ʼͬ��GPU��CPU��ִ�У��ȼ�¼Χ�����ֵ
                const UINT64 n64CurrentFenceValue = n64FenceValue;
                GRS_THROW_IF_FAILED( pIMainCMDQueue->Signal( pIFence.Get(), n64CurrentFenceValue ) );
                n64FenceValue++;
                GRS_THROW_IF_FAILED( pIFence->SetEventOnCompletion( n64CurrentFenceValue, hEventFence ) );
                //-----------------------------------------------------------------------------------------------------
            }
            break;
            case 1:
            {
                while ( ::PeekMessage( &msg, nullptr, 0, 0, PM_REMOVE ) )
                {
                    if ( WM_QUIT != msg.message )
                    {
                        ::TranslateMessage( &msg );
                        ::DispatchMessage( &msg );
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

BOOL LoadMeshVertex( const CHAR* pszMeshFileName, UINT& nVertexCnt, ST_GRS_VERTEX*& ppVertex, UINT*& ppIndices )
{
    ifstream fin;
    char input;
    BOOL bRet = TRUE;
    try
    {
        fin.open( pszMeshFileName );
        if ( fin.fail() )
        {
            AtlThrow( E_FAIL );
        }
        fin.get( input );
        while ( input != ':' )
        {
            fin.get( input );
        }
        fin >> nVertexCnt;

        fin.get( input );
        while ( input != ':' )
        {
            fin.get( input );
        }
        fin.get( input );
        fin.get( input );

        ppVertex = (ST_GRS_VERTEX*) GRS_CALLOC( nVertexCnt * sizeof( ST_GRS_VERTEX ) );
        ppIndices = (UINT*) GRS_CALLOC( nVertexCnt * sizeof( UINT ) );

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

LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    switch ( message )
    {
    case WM_DESTROY:
    {
        PostQuitMessage( 0 );
    }
    break;
    case WM_KEYDOWN:
    {
        USHORT n16KeyCode = ( wParam & 0xFF );

        if ( VK_TAB == n16KeyCode )
        {// ��Tab��
            g_nCurAlbedo += 1;
            if ( g_nCurAlbedo >= g_nAlbedoCount )
            {
                g_nCurAlbedo = 0;
            }
        }

        float fDelta = 0.2f;
        XMVECTOR vDelta = { 0.0f,0.0f,fDelta,0.0f };
        // Z-��
        if ( VK_UP == n16KeyCode )
        {
            g_v4EyePos = XMVectorAdd( g_v4EyePos, vDelta );
            g_v4LookAt = XMVectorAdd( g_v4LookAt, vDelta );
        }

        if ( VK_DOWN == n16KeyCode )
        {
            g_v4EyePos = XMVectorSubtract( g_v4EyePos, vDelta );
            g_v4LookAt = XMVectorSubtract( g_v4LookAt, vDelta );
        }
        // X-��
        vDelta = { fDelta,0.0f,0.0f,0.0f };
        if ( VK_LEFT == n16KeyCode )
        {
            g_v4EyePos = XMVectorAdd( g_v4EyePos, vDelta );
            g_v4LookAt = XMVectorAdd( g_v4LookAt, vDelta );
        }

        if ( VK_RIGHT == n16KeyCode )
        {
            g_v4EyePos = XMVectorSubtract( g_v4EyePos, vDelta );
            g_v4LookAt = XMVectorSubtract( g_v4LookAt, vDelta );
        }

        // Y-��
        vDelta = { 0.0f,fDelta,0.0f,0.0f };
        if ( VK_PRIOR == n16KeyCode )
        {
            g_v4EyePos = XMVectorAdd( g_v4EyePos, vDelta );
            g_v4LookAt = XMVectorAdd( g_v4LookAt, vDelta );
        }

        if ( VK_NEXT == n16KeyCode )
        {
            g_v4EyePos = XMVectorSubtract( g_v4EyePos, vDelta );
            g_v4LookAt = XMVectorSubtract( g_v4LookAt, vDelta );
        }

        // ����
        fDelta = 0.2f;
        if ( VK_ADD == n16KeyCode || VK_OEM_PLUS == n16KeyCode )
        {
            g_fScaling += fDelta;
        }

        if ( VK_SUBTRACT == n16KeyCode || VK_OEM_MINUS == n16KeyCode )
        {
            g_fScaling -= fDelta;
            if ( g_fScaling <= fDelta )
            {
                g_fScaling = fDelta;
            }
        }

        // ������
        fDelta = 0.04f;
        if ( 'q' == n16KeyCode || 'Q' == n16KeyCode )
        {
            g_fMetallic += fDelta;
            if ( g_fMetallic > 1.0f )
            {
                g_fMetallic = 1.0f;
            }
            ATLTRACE( L"Metallic = %11.6f\n", g_fMetallic );
        }

        if ( 'a' == n16KeyCode || 'A' == n16KeyCode )
        {
            g_fMetallic -= fDelta;
            if ( g_fMetallic < fDelta )
            {
                g_fMetallic = fDelta;
            }
            ATLTRACE( L"Metallic = %11.6f\n", g_fMetallic );
        }

        // �ֲڶ�
        fDelta = 0.05f;
        if ( 'w' == n16KeyCode || 'W' == n16KeyCode )
        {
            g_fRoughness += fDelta;
            if ( g_fRoughness > 1.0f )
            {
                g_fRoughness = 1.0f;
            }
            ATLTRACE( L"Roughness = %11.6f\n", g_fRoughness );
        }

        if ( 's' == n16KeyCode || 'S' == n16KeyCode )
        {
            g_fRoughness -= fDelta;
            if ( g_fRoughness < 0.0f )
            {
                g_fRoughness = 0.0f;
            }
            ATLTRACE( L"Roughness = %11.6f\n", g_fRoughness );
        }

        // �����ڵ�
        fDelta = 0.1f;
        if ( 'e' == n16KeyCode || 'E' == n16KeyCode )
        {
            g_fAO += fDelta;
            if ( g_fAO > 1.0f )
            {
                g_fAO = 1.0f;
            }
            ATLTRACE( L"Ambient Occlusion = %11.6f\n", g_fAO );
        }

        if ( 'd' == n16KeyCode || 'D' == n16KeyCode )
        {
            g_fAO -= fDelta;
            if ( g_fAO < 0.0f )
            {
                g_fAO = 0.0f;
            }
            ATLTRACE( L"Ambient Occlusion = %11.6f\n", g_fAO );
        }

    }
    break;

    default:
        return DefWindowProc( hWnd, message, wParam, lParam );
    }

    return DefWindowProc( hWnd, message, wParam, lParam );
}
