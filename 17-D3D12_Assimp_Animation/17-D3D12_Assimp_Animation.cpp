#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // �� Windows ͷ���ų�����ʹ�õ�����
#include <windows.h>
#include <tchar.h>
#include <commdlg.h>
#include <wrl.h> //���WTL֧�� ����ʹ��COM
#include <strsafe.h>

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

#include "../Commons/GRS_Assimp_Loader.h"
#include "../Commons/GRS_D3D12_Utility.h"
#include "../Commons/GRS_Texture_Loader.h"

using namespace DirectX;
using namespace Microsoft;
using namespace Microsoft::WRL;

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

#pragma comment(lib, "assimp-vc142-mtd.lib")

#define GRS_SLEEP(dwMilliseconds)  WaitForSingleObject(GetCurrentThread(),dwMilliseconds)
#define GRS_THROW_IF_FAILED(hr) {HRESULT _hr = (hr);if (FAILED(_hr)){ ATLTRACE("Error: 0x%08x\n",_hr); AtlThrow(_hr); }}
//���������ϱ߽�����㷨 �ڴ�����г��� ���ס
#define GRS_UPPER(A,B) ((size_t)(((A)+((B)-1))&~((B) - 1)))
//��ȡ������
#define GRS_UPPER_DIV(A,B) ((UINT)(((A)+((B)-1))/(B)))

#define GRS_WND_CLASS _T("GRS Game Window Class")
#define GRS_WND_TITLE _T("GRS DirectX12 Assimp Import Model & Animation & Render")

struct ST_GRS_CB_MVP
{
    XMFLOAT4X4 mxWorld;                   //�������������ʵ��Model->World��ת������
    XMFLOAT4X4 mxView;                    //�Ӿ���
    XMFLOAT4X4 mxProjection;              //ͶӰ����
    XMFLOAT4X4 mxViewProj;                //�Ӿ���*ͶӰ
    XMFLOAT4X4 mxMVP;                     //����*�Ӿ���*ͶӰ
};

struct ST_GRS_CB_BONES
{
    XMFLOAT4X4 mxBones[GRS_MAX_BONES];    //������������ɫ�塱 ���256��"���ͷ"���ڴ���Դ�࣬���ԣ�
};

TCHAR g_pszAppPath[MAX_PATH] = {};
CHAR  g_pszAppPathA[MAX_PATH] = {};
CHAR  g_pszModelFile[MAX_PATH] = {};

ST_GRS_MESH_DATA g_stMeshData = {};

XMVECTOR g_v4EyePos = XMVectorSet( 0.0f, 20.0f, -100.0f, 0.0f );	//�۾�λ��
XMVECTOR g_v4LookAt = XMVectorSet( 0.0f, 10.0f, 0.2f, 0.0f );		//�۾�������λ��
XMVECTOR g_v4UpDir = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );		//ͷ�����Ϸ�λ��

BOOL g_bWireFrame = FALSE;
FLOAT g_fScaling = 1.0f;

LRESULT CALLBACK WndProc( HWND, UINT, WPARAM, LPARAM );

int APIENTRY _tWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR    lpCmdLine, int nCmdShow )
{
    int									iWidth = 1280;
    int									iHeight = 800;
    HWND								hWnd = nullptr;
    MSG									msg = {};

    const UINT						    nFrameBackBufCount = 3u;
    UINT							    nFrameIndex = 0;
    DXGI_FORMAT				            emRenderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT                         emDepthStencilFormat = DXGI_FORMAT_D32_FLOAT;
    const float						    faClearColor[] = { 0.17647f, 0.549f, 0.941176f, 1.0f };

    UINT								nRTVDescriptorSize = 0U;
    UINT								nSamplerDescriptorSize = 0;
    UINT								nCBVSRVDescriptorSize = 0;

    D3D12_VIEWPORT			            stViewPort = { 0.0f, 0.0f, static_cast<float>( iWidth ), static_cast<float>( iHeight ), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
    D3D12_RECT					        stScissorRect = { 0, 0, static_cast<LONG>( iWidth ), static_cast<LONG>( iHeight ) };

    D3D_FEATURE_LEVEL					emFeatureLevel = D3D_FEATURE_LEVEL_12_1;

    ComPtr<IDXGIFactory6>				pIDXGIFactory6;
    ComPtr<ID3D12Device4>				pID3D12Device4;

    ComPtr<ID3D12CommandQueue>			pIMainCMDQueue;
    ComPtr<ID3D12CommandAllocator>		pIMainCMDAlloc;
    ComPtr<ID3D12GraphicsCommandList>	pIMainCMDList;

    ComPtr<ID3D12CommandQueue>			pICopyCMDQueue;
    ComPtr<ID3D12CommandAllocator>		pICopyCMDAlloc;
    ComPtr<ID3D12GraphicsCommandList>	pICopyCMDList;

    ComPtr<ID3D12Fence>					pIFence;
    UINT64								n64FenceValue = 0ui64;
    HANDLE								hEventFence = nullptr;
    D3D12_RESOURCE_BARRIER				stBeginResBarrier = {};
    D3D12_RESOURCE_BARRIER				stEneResBarrier = {};


    ComPtr<IDXGISwapChain3>				pISwapChain3;
    ComPtr<ID3D12DescriptorHeap>		pIRTVHeap;
    ComPtr<ID3D12Resource>				pIARenderTargets[nFrameBackBufCount];
    ComPtr<ID3D12DescriptorHeap>		pIDSVHeap;
    ComPtr<ID3D12Resource>				pIDepthStencilBuffer;

    ComPtr<ID3DBlob>					pIVSModel;
    ComPtr<ID3DBlob>					pIPSModel;
    ComPtr<ID3D12RootSignature>		    pIRootSignature;
    ComPtr<ID3D12PipelineState>			pIPSOModel;
    ComPtr<ID3D12PipelineState>			pIPSOWireFrame;

    ComPtr<ID3D12DescriptorHeap>		pICBVSRVHeap;
    ComPtr<ID3D12DescriptorHeap>		pISamplerHeap;

    ComPtr<ID3D12Heap>					pIUploadHeapModel;
    ComPtr<ID3D12Heap>					pIDefaultHeapModel;

    ComPtr<ID3D12Heap>					pIUploadHeapIndices;
    ComPtr<ID3D12Heap>					pIDefaultHeapIndices;

    ComPtr<ID3D12Resource>				pIVBPositionsUp;
    ComPtr<ID3D12Resource>				pIVBNormalsUp;
    ComPtr<ID3D12Resource>				pIVBTexCoordsUp;
    ComPtr<ID3D12Resource>				pIVBBoneIndicesUp;
    ComPtr<ID3D12Resource>				pIIBIndicesUp;

    ComPtr<ID3D12Resource>				pIVBPositions;
    ComPtr<ID3D12Resource>				pIVBNormals;
    ComPtr<ID3D12Resource>				pIVBTexCoords;
    ComPtr<ID3D12Resource>				pIVBBoneIndices;
    ComPtr<ID3D12Resource>				pIIBIndices;

    // ���㻺����ͼ��ע�Ȿ��������4��slot�����붥�����ݣ�������Ҫ4����ͼ
    // ��Slot���Կ����ݶ���������һ����Ҫ�ļ��ɣ�Ŀǰ�����ģ�͵���ⶼ�ǰ��������ͷֱ���ÿ������
    // ͬʱ�ܶ���Ϸ����Ҳ��֧�ֶ�slot�����붥�����ݵ�

    D3D12_VERTEX_BUFFER_VIEW			staVBV[g_ncSlotCnt] = {};
    D3D12_INDEX_BUFFER_VIEW				stIBV = {};

    ComPtr<ID3D12Resource>				pICBMVP;
    ComPtr<ID3D12Resource>				pICBBones;

    ST_GRS_CB_MVP*                      pstCBMVP = nullptr;
    ST_GRS_CB_BONES*                    pstBones = nullptr;

    // �������飬һ��ģ���л�������������ʾ����Ŀǰ��ʹ�û���������
    CAtlArray<ComPtr<ID3D12Resource>>	arTexture;
    CAtlArray<ComPtr<ID3D12Resource>>	arTextureUp;

    USES_CONVERSION;

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

            // ����һ��MBCS��·����Assimpʹ��
            StringCchCopyA( g_pszAppPathA, MAX_PATH, T2A( g_pszAppPath ) );
        }

        // 1������ģ��
        {
            g_stMeshData.m_strFileName.Format( "%Assets\\The3DModel\\hero.x", g_pszAppPathA );

            OPENFILENAMEA ofn = {};
            ofn.lStructSize = sizeof( ofn );
            ofn.hwndOwner = ::GetConsoleWindow();
            ofn.lpstrFilter = "x �ļ�\0*.x;*.X\0OBJ�ļ�\0*.obj\0FBX�ļ�\0*.fbx\0�����ļ�\0*.*\0";
            ofn.lpstrFile = g_stMeshData.m_strFileName.GetBuffer();
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrTitle = "��ѡ��һ��3Dģ���ļ�.......";
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

            if ( !GetOpenFileNameA( &ofn ) )
            {
                g_stMeshData.m_strFileName.Format( "%Assets\\The3DModel\\hero.x", g_pszAppPathA );
            }

            if ( !LoadMesh( g_stMeshData.m_strFileName, g_stMeshData ) )
            {
                GRS_THROW_IF_FAILED( HRESULT_FROM_WIN32( GetLastError() ) );
            }
        }

        // 2����������
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

        // 3������DXGI Factory����
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

        // 4��ö�������������豸
        {//ѡ��NUMA�ܹ��Ķ���������3D�豸����,��ʱ�Ȳ�֧�ּ����ˣ���Ȼ������޸���Щ��Ϊ
            ComPtr<IDXGIAdapter1> pIAdapter1;
            GRS_THROW_IF_FAILED( pIDXGIFactory6->EnumAdapterByGpuPreference( 0
                , DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
                , IID_PPV_ARGS( &pIAdapter1 ) ) );
            GRS_SET_DXGI_DEBUGNAME_COMPTR( pIAdapter1 );

            // ����D3D12.1���豸
            GRS_THROW_IF_FAILED( D3D12CreateDevice( pIAdapter1.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS( &pID3D12Device4 ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pID3D12Device4 );


            DXGI_ADAPTER_DESC1 stAdapterDesc = {};
            TCHAR pszWndTitle[MAX_PATH] = {};
            GRS_THROW_IF_FAILED( pIAdapter1->GetDesc1( &stAdapterDesc ) );
            ::GetWindowText( hWnd, pszWndTitle, MAX_PATH );
            StringCchPrintf( pszWndTitle
                , MAX_PATH
                , _T( "%s(GPU[0]:%s)" )
                , pszWndTitle
                , stAdapterDesc.Description );
            ::SetWindowText( hWnd, pszWndTitle );

            // �õ�ÿ��������Ԫ�صĴ�С
            nRTVDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_RTV );
            nSamplerDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER );
            nCBVSRVDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
        }

        // 5�������������&�����б�
        {
            D3D12_COMMAND_QUEUE_DESC stQueueDesc = {};
            stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommandQueue( &stQueueDesc, IID_PPV_ARGS( &pIMainCMDQueue ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIMainCMDQueue );

            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT
                , IID_PPV_ARGS( &pIMainCMDAlloc ) ) );

            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIMainCMDAlloc );

            // ����ͼ�������б�
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommandList(
                0
                , D3D12_COMMAND_LIST_TYPE_DIRECT
                , pIMainCMDAlloc.Get()
                , nullptr
                , IID_PPV_ARGS( &pIMainCMDList ) ) );

            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIMainCMDList );

             // ����Copy�������
            stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommandQueue( &stQueueDesc, IID_PPV_ARGS( &pICopyCMDQueue ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pICopyCMDQueue );

            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_COPY
                , IID_PPV_ARGS( &pICopyCMDAlloc ) ) );

            GRS_SET_D3D12_DEBUGNAME_COMPTR( pICopyCMDAlloc );

            // ����Copy�����б�
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommandList(
                0
                , D3D12_COMMAND_LIST_TYPE_COPY
                , pICopyCMDAlloc.Get()
                , nullptr
                , IID_PPV_ARGS( &pICopyCMDList ) ) );

            GRS_SET_D3D12_DEBUGNAME_COMPTR( pICopyCMDList );
        }

        // 6������������
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

            ComPtr<IDXGISwapChain1> pISwapChain1;

            GRS_THROW_IF_FAILED( pIDXGIFactory6->CreateSwapChainForHwnd(
                pIMainCMDQueue.Get(),		// ��������Ҫ������У�Present����Ҫִ��
                hWnd,
                &stSwapChainDesc,
                nullptr,
                nullptr,
                &pISwapChain1
            ) );

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

                GRS_THROW_IF_FAILED( pID3D12Device4->CreateDescriptorHeap(
                    &stRTVHeapDesc
                    , IID_PPV_ARGS( &pIRTVHeap ) ) );

                GRS_SET_D3D12_DEBUGNAME_COMPTR( pIRTVHeap );

            }

            //����RTV��������
            {
                D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle
                    = pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
                for ( UINT i = 0; i < nFrameBackBufCount; i++ )
                {
                    GRS_THROW_IF_FAILED( pISwapChain3->GetBuffer( i
                        , IID_PPV_ARGS( &pIARenderTargets[i] ) ) );

                    GRS_SET_D3D12_DEBUGNAME_INDEXED_COMPTR( pIARenderTargets, i );

                    pID3D12Device4->CreateRenderTargetView( pIARenderTargets[i].Get()
                        , nullptr
                        , stRTVHandle );

                    stRTVHandle.ptr += nRTVDescriptorSize;
                }
            }

            // �ر�ALT+ENTER���л�ȫ���Ĺ��ܣ���Ϊ����û��ʵ��OnSize���������ȹر�
            // ע�⽫�������Alt+Enter��ϼ��Ķ����ŵ����������Ϊֱ�����ʱ��DXGI Factoryͨ��Create Swapchain������֪����HWND���ĸ�����
            // ��Ϊ��ϼ��Ĵ�����Windows�ж��ǻ��ڴ��ڹ��̵ģ�û�д��ھ���Ͳ�֪�����ĸ������Լ����ڹ��̣�֮ǰ�����ξ�û��������
            GRS_THROW_IF_FAILED( pIDXGIFactory6->MakeWindowAssociation( hWnd, DXGI_MWA_NO_ALT_ENTER ) );
        }

        // 7��������Ȼ��弰��Ȼ�����������
        {
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
                &stDefautHeapProps
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

            pID3D12Device4->CreateDepthStencilView( pIDepthStencilBuffer.Get()
                , &stDepthStencilDesc
                , pIDSVHeap->GetCPUDescriptorHandleForHeapStart() );
        }

        // 8������Χ��
        {
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateFence( 0
                , D3D12_FENCE_FLAG_NONE
                , IID_PPV_ARGS( &pIFence ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIFence );

            n64FenceValue = 1;

            // ����һ��Eventͬ���������ڵȴ�Χ���¼�֪ͨ
            hEventFence = CreateEvent( nullptr, FALSE, FALSE, nullptr );
            if ( hEventFence == nullptr )
            {
                GRS_THROW_IF_FAILED( HRESULT_FROM_WIN32( GetLastError() ) );
            }
        }

        // 9������Shader������Ⱦ����״̬����
        {
            // �ȴ�����ǩ��
            D3D12_FEATURE_DATA_ROOT_SIGNATURE stFeatureData = {};
            // ����Ƿ�֧��V1.1�汾�ĸ�ǩ��
            stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
            if ( FAILED( pID3D12Device4->CheckFeatureSupport( D3D12_FEATURE_ROOT_SIGNATURE, &stFeatureData, sizeof( stFeatureData ) ) ) )
            {
                stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
            }

            D3D12_DESCRIPTOR_RANGE1 stDSPRanges1[3] = {};

            stDSPRanges1[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
            stDSPRanges1[0].NumDescriptors = 2;
            stDSPRanges1[0].BaseShaderRegister = 0;
            stDSPRanges1[0].RegisterSpace = 0;
            stDSPRanges1[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
            stDSPRanges1[0].OffsetInDescriptorsFromTableStart = 0;

            // ��GPU��ִ��SetGraphicsRootDescriptorTable�����ǲ��޸������б��е�SRV��������ǿ���ʹ��Ĭ��Rang��Ϊ:
            // D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE

            stDSPRanges1[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            stDSPRanges1[1].NumDescriptors = 1;
            stDSPRanges1[1].BaseShaderRegister = 0;
            stDSPRanges1[1].RegisterSpace = 0;
            stDSPRanges1[1].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
            stDSPRanges1[1].OffsetInDescriptorsFromTableStart = 0;

            stDSPRanges1[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
            stDSPRanges1[2].NumDescriptors = 1;
            stDSPRanges1[2].BaseShaderRegister = 0;
            stDSPRanges1[2].RegisterSpace = 0;
            stDSPRanges1[2].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
            stDSPRanges1[2].OffsetInDescriptorsFromTableStart = 0;

            D3D12_ROOT_PARAMETER1 stRootParameters1[3] = {};
            stRootParameters1[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            stRootParameters1[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            stRootParameters1[0].DescriptorTable.NumDescriptorRanges = 1;
            stRootParameters1[0].DescriptorTable.pDescriptorRanges = &stDSPRanges1[0];

            stRootParameters1[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            stRootParameters1[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
            stRootParameters1[1].DescriptorTable.NumDescriptorRanges = 1;
            stRootParameters1[1].DescriptorTable.pDescriptorRanges = &stDSPRanges1[1];

            stRootParameters1[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            stRootParameters1[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
            stRootParameters1[2].DescriptorTable.NumDescriptorRanges = 1;
            stRootParameters1[2].DescriptorTable.pDescriptorRanges = &stDSPRanges1[2];

            D3D12_VERSIONED_ROOT_SIGNATURE_DESC stRootSignatureDesc = {};

            if ( D3D_ROOT_SIGNATURE_VERSION_1_1 == stFeatureData.HighestVersion )
            {
                stRootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
                stRootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
                stRootSignatureDesc.Desc_1_1.NumParameters = _countof( stRootParameters1 );
                stRootSignatureDesc.Desc_1_1.pParameters = stRootParameters1;
                stRootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
                stRootSignatureDesc.Desc_1_1.pStaticSamplers = nullptr;
            }
            else
            {
                D3D12_DESCRIPTOR_RANGE stDSPRanges[3] = {};

                stDSPRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
                stDSPRanges[0].NumDescriptors = 2;
                stDSPRanges[0].BaseShaderRegister = 0;
                stDSPRanges[0].RegisterSpace = 0;
                stDSPRanges[0].OffsetInDescriptorsFromTableStart = 0;

                stDSPRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                stDSPRanges[1].NumDescriptors = 1;
                stDSPRanges[1].BaseShaderRegister = 0;
                stDSPRanges[1].RegisterSpace = 0;
                stDSPRanges[1].OffsetInDescriptorsFromTableStart = 0;

                stDSPRanges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
                stDSPRanges[2].NumDescriptors = 1;
                stDSPRanges[2].BaseShaderRegister = 0;
                stDSPRanges[2].RegisterSpace = 0;
                stDSPRanges[2].OffsetInDescriptorsFromTableStart = 0;

                D3D12_ROOT_PARAMETER stRootParameters[3] = {};
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

                stRootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_0;
                stRootSignatureDesc.Desc_1_0.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
                stRootSignatureDesc.Desc_1_0.NumParameters = _countof( stRootParameters );
                stRootSignatureDesc.Desc_1_0.pParameters = stRootParameters;
                stRootSignatureDesc.Desc_1_0.NumStaticSamplers = 0;
                stRootSignatureDesc.Desc_1_0.pStaticSamplers = nullptr;
            }

            ComPtr<ID3DBlob> pISignatureBlob;
            ComPtr<ID3DBlob> pIErrorBlob;

            HRESULT _hr = D3D12SerializeVersionedRootSignature( &stRootSignatureDesc
                , &pISignatureBlob
                , &pIErrorBlob );

            if ( FAILED( _hr ) )
            {
                ATLTRACE( "Create Root Signature Error��%s\n", (CHAR*) pIErrorBlob->GetBufferPointer() );
                AtlThrow( _hr );
            }

            GRS_THROW_IF_FAILED( pID3D12Device4->CreateRootSignature( 0
                , pISignatureBlob->GetBufferPointer()
                , pISignatureBlob->GetBufferSize()
                , IID_PPV_ARGS( &pIRootSignature ) ) );

            // �����Ժ�����ʽ����Shader
            UINT compileFlags = 0;
#if defined(_DEBUG)
            // Enable better shader debugging with the graphics debugging tools.
            compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
            //����Ϊ�о�����ʽ	   
            compileFlags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

            TCHAR pszShaderFileName[MAX_PATH] = {};
            StringCchPrintf( pszShaderFileName, MAX_PATH
                , _T( "%s17-D3D12_Assimp_Animation\\Shader\\simple_bones_animation_multitex.hlsl" )
                , g_pszAppPath );
            ComPtr<ID3DBlob> pIErrorMsg;
            HRESULT hr = D3DCompileFromFile( pszShaderFileName, nullptr, nullptr
                , "VSMain", "vs_5_0", compileFlags, 0, &pIVSModel, &pIErrorMsg );

            if ( FAILED( hr ) )
            {
                ATLTRACE( "���� Vertex Shader��\"%s\" ��������%s\n"
                    , T2A( pszShaderFileName )
                    , pIErrorMsg ? pIErrorMsg->GetBufferPointer() : "�޷���ȡ�ļ���" );
                GRS_THROW_IF_FAILED( hr );
            }
            pIErrorMsg.Reset();

            hr = D3DCompileFromFile( pszShaderFileName, nullptr, nullptr
                , "PSMain", "ps_5_0", compileFlags, 0, &pIPSModel, &pIErrorMsg );
            if ( FAILED( hr ) )
            {
                ATLTRACE( "���� Pixel Shader��\"%s\" ��������%s\n"
                    , T2A( pszShaderFileName )
                    , pIErrorMsg ? pIErrorMsg->GetBufferPointer() : "�޷���ȡ�ļ���" );
                GRS_THROW_IF_FAILED( hr );
            }

            // ���崫����ߵ����ݽṹ������ʹ���˶�Slot��ʽ��ע��Slot���÷�
            D3D12_INPUT_ELEMENT_DESC stIALayout[] =
            {
                { "POSITION",	  0, DXGI_FORMAT_R32G32B32A32_FLOAT,   0,	 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "NORMAL",		  0, DXGI_FORMAT_R32G32B32A32_FLOAT,   1,	 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "TEXCOORD",	  0, DXGI_FORMAT_R32G32_FLOAT,         2,	 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT,    3,	 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT,   3,	16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
            };

            // ���� graphics pipeline state object (PSO)����
            D3D12_GRAPHICS_PIPELINE_STATE_DESC stPSODesc = {};
            stPSODesc.InputLayout = { stIALayout, _countof( stIALayout ) };

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

            GRS_THROW_IF_FAILED( pID3D12Device4->CreateGraphicsPipelineState( &stPSODesc
                , IID_PPV_ARGS( &pIPSOModel ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIPSOModel );

            // ������ʾ����Ⱦ����
            stPSODesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateGraphicsPipelineState( &stPSODesc
                , IID_PPV_ARGS( &pIPSOWireFrame ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIPSOWireFrame );
        }

        // 10������ SRV CBV Sample��
        if( TRUE )
        {
            //��������ͼ��������CBV����������һ������������
            D3D12_DESCRIPTOR_HEAP_DESC stSRVHeapDesc = {};
            UINT nTextureCount = (UINT) g_stMeshData.m_mapTextrueName2Index.GetCount();
            stSRVHeapDesc.NumDescriptors = 2 + ( ( nTextureCount > 0 ) ? nTextureCount : 1 ); // 2 CBV + n SRV
            stSRVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            stSRVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

            GRS_THROW_IF_FAILED( pID3D12Device4->CreateDescriptorHeap( &stSRVHeapDesc, IID_PPV_ARGS( &pICBVSRVHeap ) ) );

            GRS_SET_D3D12_DEBUGNAME_COMPTR( pICBVSRVHeap );

            D3D12_DESCRIPTOR_HEAP_DESC stSamplerHeapDesc = {};
            stSamplerHeapDesc.NumDescriptors = 1;
            stSamplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
            stSamplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

            GRS_THROW_IF_FAILED( pID3D12Device4->CreateDescriptorHeap( &stSamplerHeapDesc, IID_PPV_ARGS( &pISamplerHeap ) ) );

            GRS_SET_D3D12_DEBUGNAME_COMPTR( pISamplerHeap );
        }

        // 11������D3D12��ģ����Դ������������
        {
            size_t szAlign = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
            // �������еĻ����С
            size_t szPositions   = g_stMeshData.m_arPositions.GetCount() * sizeof( g_stMeshData.m_arPositions[0] );
            size_t szNormals     = g_stMeshData.m_arNormals.GetCount() * sizeof( g_stMeshData.m_arNormals[0] );
            size_t szTexCoords   = g_stMeshData.m_arTexCoords.GetCount() * sizeof( g_stMeshData.m_arTexCoords[0] );
            size_t szBoneIndices = g_stMeshData.m_arBoneIndices.GetCount() * sizeof( g_stMeshData.m_arBoneIndices[0] );
            size_t szIndices     = g_stMeshData.m_arIndices.GetCount() * sizeof( g_stMeshData.m_arIndices[0] );

            // ��Ҫ�Ļ����С+64k-1 ʹ�øպ���64k�߽��Сʱ�����Զ����64k��������ֹCreatePlacedResource����
            size_t szVBBuffer = GRS_UPPER( szPositions, szAlign ) 
                + GRS_UPPER( szNormals, szAlign )
                + GRS_UPPER( szTexCoords, szAlign )
                + GRS_UPPER( szBoneIndices, szAlign )
                + szAlign - 1;             
           
            D3D12_HEAP_DESC stUploadHeapDesc = {  };
            // �ϴ������;�����ͨ�Ļ��壬���԰ڷ���������
            stUploadHeapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
            // ʵ�����ݴ�С��5*64K�߽�����С����Ϊ��5��Buffer
            stUploadHeapDesc.SizeInBytes = GRS_UPPER( szVBBuffer, szAlign );
            // ע���ϴ��ѿ϶���Buffer���ͣ����Բ�ָ�����뷽ʽ����Ĭ����64k�߽����
            stUploadHeapDesc.Alignment = 0;
            stUploadHeapDesc.Properties = stUploadHeapProps;
 
            // �����������ݵ��ϴ���
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateHeap( &stUploadHeapDesc, IID_PPV_ARGS( &pIUploadHeapModel ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIUploadHeapModel );

            size_t szOffset = 0;
            BYTE* pData = nullptr;
            // Positions Upload Buffer
            stBufferResSesc.Width = GRS_UPPER( szPositions, szAlign );
            GRS_THROW_IF_FAILED( pID3D12Device4->CreatePlacedResource(
                pIUploadHeapModel.Get()
                , szOffset
                , &stBufferResSesc
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS( &pIVBPositionsUp ) ) );

            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIVBPositionsUp );

            GRS_THROW_IF_FAILED( pIVBPositionsUp->Map( 0, nullptr, reinterpret_cast<void**>( &pData ) ) );
            // ��һ�� Copy��CPU Memory -> Share Memory ʵ���Ͼ������ڴ��е��ڣ�ֻ�������ַ��ͬ
            // ���ƻ�����ôһ�죬���Copy�������ܱ����滻ΪCPU�����ַӳ��ı������˵Vulkan���Ѿ�������������
            memcpy( pData, g_stMeshData.m_arPositions.GetData(), szPositions );
            pIVBPositionsUp->Unmap( 0, nullptr );
            pData = nullptr;

            // Normals Upload Buffer
            szOffset += stBufferResSesc.Width;
            stBufferResSesc.Width = GRS_UPPER( szNormals, szAlign );

            GRS_THROW_IF_FAILED( pID3D12Device4->CreatePlacedResource(
                pIUploadHeapModel.Get()
                , szOffset
                , &stBufferResSesc
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS( &pIVBNormalsUp ) ) );

            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIVBNormalsUp );

            GRS_THROW_IF_FAILED( pIVBNormalsUp->Map( 0, nullptr, reinterpret_cast<void**>( &pData ) ) );
            // ��һ�� Copy��CPU Memory -> Share Memory ʵ���Ͼ������ڴ��е��ڣ�ֻ�������ַ��ͬ
            // ���ƻ�����ôһ�죬���Copy�������ܱ����滻ΪCPU�����ַӳ��ı������˵Vulkan���Ѿ�������������
            memcpy( pData, g_stMeshData.m_arNormals.GetData(), szNormals );
            pIVBNormalsUp->Unmap( 0, nullptr );
            pData = nullptr;

            // TexCoords Upload Buffer
            szOffset += stBufferResSesc.Width;
            stBufferResSesc.Width = GRS_UPPER( szTexCoords, szAlign );

            GRS_THROW_IF_FAILED( pID3D12Device4->CreatePlacedResource(
                pIUploadHeapModel.Get()
                , szOffset
                , &stBufferResSesc
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS( &pIVBTexCoordsUp ) ) );

            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIVBTexCoordsUp );

            GRS_THROW_IF_FAILED( pIVBTexCoordsUp->Map( 0, nullptr, reinterpret_cast<void**>( &pData ) ) );
            // ��һ�� Copy��CPU Memory -> Share Memory ʵ���Ͼ������ڴ��е��ڣ�ֻ�������ַ��ͬ
            // ���ƻ�����ôһ�죬���Copy�������ܱ����滻ΪCPU�����ַӳ��ı������˵Vulkan���Ѿ�������������
            memcpy( pData, g_stMeshData.m_arTexCoords.GetData(), szTexCoords );
            pIVBTexCoordsUp->Unmap( 0, nullptr );
            pData = nullptr;

            // Bone Indices Upload Buffer
            szOffset += stBufferResSesc.Width;
            stBufferResSesc.Width = GRS_UPPER( szBoneIndices, szAlign );

            GRS_THROW_IF_FAILED( pID3D12Device4->CreatePlacedResource(
                pIUploadHeapModel.Get()
                , szOffset
                , &stBufferResSesc
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS( &pIVBBoneIndicesUp ) ) );

            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIVBBoneIndicesUp );

            GRS_THROW_IF_FAILED( pIVBBoneIndicesUp->Map( 0, nullptr, reinterpret_cast<void**>( &pData ) ) );
            // ��һ�� Copy��CPU Memory -> Share Memory ʵ���Ͼ������ڴ��е��ڣ�ֻ�������ַ��ͬ
            // ���ƻ�����ôһ�죬���Copy�������ܱ����滻ΪCPU�����ַӳ��ı������˵Vulkan���Ѿ�������������
            memcpy( pData, g_stMeshData.m_arBoneIndices.GetData(), szBoneIndices );
            pIVBBoneIndicesUp->Unmap( 0, nullptr );
            pData = nullptr;

            // ����Ĭ�϶ѣ��Դ��еĶѣ�
            D3D12_HEAP_DESC stDefaultHeapDesc = {};
            // ��С���ϴ���һ��
            stDefaultHeapDesc.SizeInBytes = stUploadHeapDesc.SizeInBytes;
            stDefaultHeapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
            // ָ���ѵĶ��뷽ʽ������ʹ����Ĭ�ϵ�64K�߽���룬��Ϊ����ʵ�ʷŵ��Ƕ�������
            stDefaultHeapDesc.Alignment = szAlign;
            stDefaultHeapDesc.Properties = stDefautHeapProps;

            // Vertex Data Default Heap
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateHeap( &stDefaultHeapDesc, IID_PPV_ARGS( &pIDefaultHeapModel ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIDefaultHeapModel );

            // Positions Default Buffer
            szOffset = 0;
            stBufferResSesc.Width = GRS_UPPER( szPositions, szAlign );
            GRS_THROW_IF_FAILED( pID3D12Device4->CreatePlacedResource(
                pIDefaultHeapModel.Get()
                , szOffset
                , &stBufferResSesc
                , D3D12_RESOURCE_STATE_COPY_DEST
                , nullptr
                , IID_PPV_ARGS( &pIVBPositions ) ) );

            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIVBPositions );

            // �ڶ���Copy�����Ե�ʱ��Share Memory -> Video Memory 
            // ����ֻ�Ǽ�¼���������֮�����Copy Engine��Excute
            pICopyCMDList->CopyBufferRegion( pIVBPositions.Get(), 0, pIVBPositionsUp.Get(), 0, szPositions );
            // Ȼ��������Դ���ϣ�ͬ��! ��ȷ�ϸ��Ʋ������            
            stResStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            stResStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
            stResStateTransBarrier.Transition.pResource = pIVBPositions.Get();
            pICopyCMDList->ResourceBarrier( 1, &stResStateTransBarrier );

            // Normals Default Buffer
            szOffset += stBufferResSesc.Width;
            stBufferResSesc.Width = GRS_UPPER( szNormals, szAlign );
            GRS_THROW_IF_FAILED( pID3D12Device4->CreatePlacedResource(
                pIDefaultHeapModel.Get()
                , szOffset
                , &stBufferResSesc
                , D3D12_RESOURCE_STATE_COPY_DEST
                , nullptr
                , IID_PPV_ARGS( &pIVBNormals ) ) );

            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIVBNormals );

            // �ڶ���Copy�����Ե�ʱ��Share Memory -> Video Memory 
            // ����ֻ�Ǽ�¼���������֮�����Copy Engine��Excute
            pICopyCMDList->CopyBufferRegion( pIVBNormals.Get(), 0, pIVBNormalsUp.Get(), 0, szNormals );
            // Ȼ��������Դ���ϣ�ͬ��! ��ȷ�ϸ��Ʋ������
            stResStateTransBarrier.Transition.pResource = pIVBNormals.Get();
            pICopyCMDList->ResourceBarrier( 1, &stResStateTransBarrier );

            // TexCoords Default Buffer
            szOffset += stBufferResSesc.Width;
            stBufferResSesc.Width = GRS_UPPER( szTexCoords, szAlign );
            GRS_THROW_IF_FAILED( pID3D12Device4->CreatePlacedResource(
                pIDefaultHeapModel.Get()
                , szOffset
                , &stBufferResSesc
                , D3D12_RESOURCE_STATE_COPY_DEST
                , nullptr
                , IID_PPV_ARGS( &pIVBTexCoords ) ) );

            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIVBTexCoords );

            // �ڶ���Copy�����Ե�ʱ��Share Memory -> Video Memory 
            // ����ֻ�Ǽ�¼���������֮�����Copy Engine��Excute
            pICopyCMDList->CopyBufferRegion( pIVBTexCoords.Get(), 0, pIVBTexCoordsUp.Get(), 0, szTexCoords );
            // Ȼ��������Դ���ϣ�ͬ��! ��ȷ�ϸ��Ʋ������
            stResStateTransBarrier.Transition.pResource = pIVBTexCoords.Get();
            pICopyCMDList->ResourceBarrier( 1, &stResStateTransBarrier );

            // Bone Indices Default Buffer
            szOffset += stBufferResSesc.Width;
            stBufferResSesc.Width = GRS_UPPER( szBoneIndices, szAlign );
            GRS_THROW_IF_FAILED( pID3D12Device4->CreatePlacedResource(
                pIDefaultHeapModel.Get()
                , szOffset
                , &stBufferResSesc
                , D3D12_RESOURCE_STATE_COPY_DEST
                , nullptr
                , IID_PPV_ARGS( &pIVBBoneIndices ) ) );

            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIVBBoneIndices );

            // �ڶ���Copy�����Ե�ʱ��Share Memory -> Video Memory 
            // ����ֻ�Ǽ�¼���������֮�����Copy Engine��Excute
            pICopyCMDList->CopyBufferRegion( pIVBBoneIndices.Get(), 0, pIVBBoneIndicesUp.Get(), 0, szBoneIndices );
            // Ȼ��������Դ���ϣ�ͬ��! ��ȷ�ϸ��Ʋ������
            stResStateTransBarrier.Transition.pResource = pIVBBoneIndices.Get();
            pICopyCMDList->ResourceBarrier( 1, &stResStateTransBarrier );


            // Positions Buffer View
            staVBV[0].BufferLocation = pIVBPositions->GetGPUVirtualAddress();
            staVBV[0].SizeInBytes = (UINT) szPositions;
            staVBV[0].StrideInBytes = sizeof( g_stMeshData.m_arPositions[0] );

            // Normals Buffer View
            staVBV[1].BufferLocation = pIVBNormals->GetGPUVirtualAddress();
            staVBV[1].SizeInBytes = (UINT) szNormals;
            staVBV[1].StrideInBytes = sizeof( g_stMeshData.m_arNormals[0] );

            // TexCoords Buffer View
            staVBV[2].BufferLocation = pIVBTexCoords->GetGPUVirtualAddress();
            staVBV[2].SizeInBytes = (UINT) szTexCoords;
            staVBV[2].StrideInBytes = sizeof( g_stMeshData.m_arTexCoords[0] );

            // BoneIndices Buffer View
            staVBV[3].BufferLocation = pIVBBoneIndices->GetGPUVirtualAddress();
            staVBV[3].SizeInBytes = (UINT) szBoneIndices;
            staVBV[3].StrideInBytes = sizeof( g_stMeshData.m_arBoneIndices[0] );

            // Indices Upload Buffer
            // Indices �ϴ���
            stUploadHeapDesc.SizeInBytes = GRS_UPPER( szIndices, szAlign );

            GRS_THROW_IF_FAILED( pID3D12Device4->CreateHeap( &stUploadHeapDesc, IID_PPV_ARGS( &pIUploadHeapIndices ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIUploadHeapIndices );

            stBufferResSesc.Width = GRS_UPPER( szIndices, szAlign );

            GRS_THROW_IF_FAILED( pID3D12Device4->CreatePlacedResource(
                pIUploadHeapIndices.Get()
                , 0
                , &stBufferResSesc
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS( &pIIBIndicesUp ) ) );

            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIIBIndicesUp );

            GRS_THROW_IF_FAILED( pIIBIndicesUp->Map( 0, nullptr, reinterpret_cast<void**>( &pData ) ) );
            // ��һ�� Copy��CPU Memory -> Share Memory ʵ���Ͼ������ڴ��е��ڣ�ֻ�������ַ��ͬ
            // ���ƻ�����ôһ�죬���Copy�������ܱ����滻ΪCPU�����ַӳ��ı������˵Vulkan���Ѿ�������������
            memcpy( pData, g_stMeshData.m_arIndices.GetData(), szIndices );
            pIIBIndicesUp->Unmap( 0, nullptr );
            pData = nullptr;

            // Indices Default Buffer
            // Indices Data Default Heap
            stDefaultHeapDesc.SizeInBytes = GRS_UPPER( szIndices, szAlign );

            GRS_THROW_IF_FAILED( pID3D12Device4->CreateHeap( &stDefaultHeapDesc, IID_PPV_ARGS( &pIDefaultHeapIndices ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIDefaultHeapIndices );

            stBufferResSesc.Width = GRS_UPPER( szIndices, szAlign );
            GRS_THROW_IF_FAILED( pID3D12Device4->CreatePlacedResource(
                pIDefaultHeapIndices.Get()
                , 0
                , &stBufferResSesc
                , D3D12_RESOURCE_STATE_COPY_DEST
                , nullptr
                , IID_PPV_ARGS( &pIIBIndices ) ) );

            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIIBIndices );

            // �ڶ���Copy�����Ե�ʱ��Share Memory -> Video Memory 
            // ����ֻ�Ǽ�¼���������֮�����Copy Engine��Excute
            pICopyCMDList->CopyBufferRegion( pIIBIndices.Get(), 0, pIIBIndicesUp.Get(), 0, szIndices );
            // Ȼ��������Դ���ϣ�ͬ��! ��ȷ�ϸ��Ʋ������
            stResStateTransBarrier.Transition.pResource = pIIBIndices.Get();
            pICopyCMDList->ResourceBarrier( 1, &stResStateTransBarrier );
            // Indices Buffer View
            stIBV.BufferLocation = pIIBIndices->GetGPUVirtualAddress();
            stIBV.SizeInBytes = (UINT) szIndices;
            // 32bit ? 16bit Index
            stIBV.Format = ( sizeof( g_stMeshData.m_arIndices[0] ) > 2 ) ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;

            // Const Buffers
            D3D12_HEAP_PROPERTIES stUploadHeapProp = {  };

            stUploadHeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
            stUploadHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            stUploadHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            stUploadHeapProp.CreationNodeMask = 0;
            stUploadHeapProp.VisibleNodeMask = 0;

            D3D12_RESOURCE_DESC stConstBufferDesc = {};

            stConstBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            stConstBufferDesc.Alignment = szAlign;
            stConstBufferDesc.Width = GRS_UPPER( sizeof( ST_GRS_CB_MVP ), szAlign );
            stConstBufferDesc.Height = 1;
            stConstBufferDesc.DepthOrArraySize = 1;
            stConstBufferDesc.MipLevels = 1;
            stConstBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
            stConstBufferDesc.SampleDesc.Count = 1;
            stConstBufferDesc.SampleDesc.Quality = 0;
            stConstBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            stConstBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

            // MVP Const Buffer
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommittedResource(
                &stUploadHeapProp
                , D3D12_HEAP_FLAG_NONE
                , &stConstBufferDesc //ע�⻺��ߴ�����Ϊ256�߽�����С
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS( &pICBMVP ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pICBMVP );

            // Map ֮��Ͳ���Unmap�� ֱ�Ӹ������ݽ�ȥ ����ÿ֡������map-copy-unmap�˷�ʱ����
            GRS_THROW_IF_FAILED( pICBMVP->Map( 0, nullptr, reinterpret_cast<void**>( &pstCBMVP ) ) );

            // Bones Const Buffer
            stConstBufferDesc.Width = GRS_UPPER( sizeof( ST_GRS_CB_BONES ), szAlign );

            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommittedResource(
                &stUploadHeapProp
                , D3D12_HEAP_FLAG_NONE
                , &stConstBufferDesc //ע�⻺��ߴ�����Ϊ256�߽�����С
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS( &pICBBones ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pICBBones );

            // Map ֮��Ͳ���Unmap�� ֱ�Ӹ������ݽ�ȥ ����ÿ֡������map-copy-unmap�˷�ʱ����
            GRS_THROW_IF_FAILED( pICBBones->Map( 0, nullptr, reinterpret_cast<void**>( &pstBones ) ) );

            // Create Const Buffer View 
            D3D12_CPU_DESCRIPTOR_HANDLE stCBVHandle = { pICBVSRVHeap->GetCPUDescriptorHandleForHeapStart() };

            // ������һ��CBV ������� �Ӿ��� ͸�Ӿ��� 
            size_t szBufferViewAlign = 256;
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation = pICBMVP->GetGPUVirtualAddress();
            cbvDesc.SizeInBytes = (UINT) GRS_UPPER( sizeof( ST_GRS_CB_MVP ), szBufferViewAlign );

            pID3D12Device4->CreateConstantBufferView( &cbvDesc, stCBVHandle );

            stCBVHandle.ptr += nCBVSRVDescriptorSize;

            // �����ڶ���CBV ��ͷ��ɫ��
            cbvDesc.BufferLocation = pICBBones->GetGPUVirtualAddress();
            cbvDesc.SizeInBytes = (UINT) GRS_UPPER( sizeof( ST_GRS_CB_BONES ), szBufferViewAlign );

            pID3D12Device4->CreateConstantBufferView( &cbvDesc, stCBVHandle );

        }

        // 12����������
        {
            // ��ģ���ļ�����·�����������ļ�·��
            WCHAR pszModelPath[MAX_PATH] = {};
            StringCchCopyW( pszModelPath, MAX_PATH, A2W( g_stMeshData.m_strFileName ) );

            WCHAR* lastSlash = _tcsrchr( pszModelPath, _T( '\\' ) );
            if ( lastSlash )
            {// ɾ��ģ���ļ���
                *( lastSlash ) = _T( '\0' );
            }

            WCHAR pszTexcuteFileName[MAX_PATH] = {};
            ID3D12Resource* pITex = nullptr;
            ID3D12Resource* pITexUp = nullptr;
            D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};

            stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            stSRVDesc.Texture2D.MipLevels = 1;

            D3D12_CPU_DESCRIPTOR_HANDLE stSRVHandle = pICBVSRVHeap->GetCPUDescriptorHandleForHeapStart() ;
            // ���ϵ�������ʼ�������SRV
            stSRVHandle.ptr += (size_t) 2 * nCBVSRVDescriptorSize;

            //stMeshData.m_mapIndex2TextureName.SetAt(i, strTextureFileName);

            if ( g_stMeshData.m_mapTextrueName2Index.GetCount() <= 0 )
            {// ����������ʱ����Ĭ������
                StringCchPrintfW( pszTexcuteFileName
                    , MAX_PATH
                    , L"D:\\Projects_2018_08\\GRSD3D12Samples\\Assimp_Study\\asset\\default.jpg" );
                g_stMeshData.m_mapTextrueName2Index.SetAt( W2A( pszTexcuteFileName ), 0 );
            }

            POSITION pos;
            CStringA strTextureName;
            UINT nTextureIndex = 0;
            pos = g_stMeshData.m_mapTextrueName2Index.GetStartPosition();

            while ( nullptr != pos )
            {
                strTextureName = g_stMeshData.m_mapTextrueName2Index.GetKeyAt( pos );
                nTextureIndex = g_stMeshData.m_mapTextrueName2Index.GetNextValue( pos );

                StringCchPrintfW( pszTexcuteFileName, MAX_PATH, L"%s\\%s", pszModelPath, A2W( strTextureName ) );

                if ( ( INVALID_FILE_ATTRIBUTES == ::GetFileAttributes( pszTexcuteFileName ) ) )
                {// ���������ڻ����Ҳ�������ʱ����Ĭ������
                    StringCchPrintfW( pszTexcuteFileName
                        , MAX_PATH
                        , L"D:\\Projects_2018_08\\GRSD3D12Samples\\Assimp_Study\\asset\\default.jpg" );
                }

                ATLTRACE( _T( "��������\"%s\"\n" ), pszTexcuteFileName );

                if ( !LoadTextureFromFile( pszTexcuteFileName, pICopyCMDList.Get(), pITexUp, pITex ) )
                {
                    ATLTRACE( _T( "����\"%s\" ����ʧ��!\n" ), pszTexcuteFileName );
                    AtlThrowLastWin32();
                }

                arTexture.Add( ComPtr<ID3D12Resource>( pITex ) );
                arTextureUp.Add( ComPtr<ID3D12Resource>( pITexUp ) );

                GRS_SetD3D12DebugNameIndexed( pITex, L"pITex", nTextureIndex );
                GRS_SetD3D12DebugNameIndexed( pITexUp, L"pITex", nTextureIndex );

                stSRVDesc.Format = pITex->GetDesc().Format;
                D3D12_CPU_DESCRIPTOR_HANDLE stSRVHandleTmp = stSRVHandle;
                stSRVHandleTmp.ptr += nTextureIndex * nCBVSRVDescriptorSize;
                pID3D12Device4->CreateShaderResourceView( pITex, &stSRVDesc, stSRVHandleTmp );
            }

            // Create Sample
            D3D12_CPU_DESCRIPTOR_HANDLE hSamplerHeap = pISamplerHeap->GetCPUDescriptorHandleForHeapStart();

            D3D12_SAMPLER_DESC stSamplerDesc = {};
            stSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;

            stSamplerDesc.MinLOD = 0;
            stSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
            stSamplerDesc.MipLODBias = 0.0f;
            stSamplerDesc.MaxAnisotropy = 1;
            stSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;

            // Sampler 1
            stSamplerDesc.BorderColor[0] = 1.0f;
            stSamplerDesc.BorderColor[1] = 1.0f;
            stSamplerDesc.BorderColor[2] = 1.0f;
            stSamplerDesc.BorderColor[3] = 1.0f;
            stSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            stSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            stSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

            pID3D12Device4->CreateSampler( &stSamplerDesc, hSamplerHeap );
        }

        // 13���ϴ�
        {
            GRS_THROW_IF_FAILED( pICopyCMDList->Close() );
            //ִ�������б�
            ID3D12CommandList* ppCommandLists[] = { pICopyCMDList.Get() };
            pICopyCMDQueue->ExecuteCommandLists( _countof( ppCommandLists ), ppCommandLists );

            //��ʼͬ��GPU��CPU��ִ�У��ȼ�¼Χ�����ֵ
            const UINT64 n64CurrentFenceValue = n64FenceValue;
            GRS_THROW_IF_FAILED( pICopyCMDQueue->Signal( pIFence.Get(), n64CurrentFenceValue ) );
            n64FenceValue++;
            GRS_THROW_IF_FAILED( pIFence->SetEventOnCompletion( n64CurrentFenceValue, hEventFence ) );
        }

        // 14�������Դ���Ͻṹ
        {
            stBeginResBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            stBeginResBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            stBeginResBarrier.Transition.pResource = pIARenderTargets[nFrameIndex].Get();
            stBeginResBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            stBeginResBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            stBeginResBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            stEneResBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            stEneResBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            stEneResBarrier.Transition.pResource = pIARenderTargets[nFrameIndex].Get();
            stEneResBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            stEneResBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            stEneResBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        }

        DWORD dwRet = 0;
        BOOL bExit = FALSE;
        D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle = pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
        D3D12_CPU_DESCRIPTOR_HANDLE stDSVHandle = pIDSVHeap->GetCPUDescriptorHandleForHeapStart();

        GRS_THROW_IF_FAILED( pIMainCMDList->Close() );

        FLOAT fStartTime = ( FLOAT )::GetTickCount64();
        FLOAT fCurrentTime = fStartTime;
        FLOAT fTimeInSeconds = ( fCurrentTime - fStartTime ) / 1000.0f;
        CGRSARMatrix arBoneMatrixs;

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
                // Animation Calculate
                if ( g_stMeshData.m_paiModel->HasAnimations() )
                {
                    fCurrentTime = ( FLOAT )::GetTickCount64();
                    //fTimeInSeconds = (fCurrentTime - fStartTime) / 1000.0f;

                    fTimeInSeconds = ( fCurrentTime - fStartTime ) / 5.0f;

                    arBoneMatrixs.RemoveAll();

                    CalcAnimation( g_stMeshData, fTimeInSeconds, arBoneMatrixs );

                    memcpy( pstBones->mxBones, arBoneMatrixs.GetData(), arBoneMatrixs.GetCount() * sizeof( arBoneMatrixs[0] ) );
                }

                // Model Matrix
                // 1 -------------------------
                // ����x����ת
                //XMMATRIX mxModel = XMMatrixRotationX(-XM_2PI / 4.0f);
                //// �������ƶ�
                //float fyUp = g_stMeshData.m_paiModel->mMeshes[0]->mAABB.mMax.y - g_stMeshData.m_paiModel->mMeshes[0]->mAABB.mMin.y;
                //float fzBack = g_stMeshData.m_paiModel->mMeshes[0]->mAABB.mMax.z - g_stMeshData.m_paiModel->mMeshes[0]->mAABB.mMin.z;
                //mxModel = XMMatrixMultiply(mxModel, XMMatrixTranslation(0.0f, fyUp, fzBack));

                // 2 -------------------------
                //XMMATRIX mxModel = g_stMeshData.m_mxModel;

                // 3 -------------------------
                // Model Matrix (����ֻ�ǽ������ţ�
                XMMATRIX mxModel = XMMatrixScaling( g_fScaling, g_fScaling, g_fScaling );
                // World Matrix
                XMMATRIX mxWorld = XMMatrixIdentity();
                mxWorld = XMMatrixMultiply( mxModel, mxWorld );
                XMStoreFloat4x4( &pstCBMVP->mxWorld, mxWorld );

                // View Matrix
                XMMATRIX mxView = XMMatrixLookAtLH( g_v4EyePos, g_v4LookAt, g_v4UpDir );
                XMStoreFloat4x4( &pstCBMVP->mxView, mxView );

                // Projection Matrix
                XMMATRIX mxProjection = XMMatrixPerspectiveFovLH( XM_PIDIV4, (FLOAT) iWidth / (FLOAT) iHeight, 0.1f, 1000.0f );
                XMStoreFloat4x4( &pstCBMVP->mxProjection, mxProjection );

                // View Matrix * Projection Matrix
                XMMATRIX mxViewProj = XMMatrixMultiply( mxView, mxProjection );
                XMStoreFloat4x4( &pstCBMVP->mxViewProj, mxViewProj );

                // World Matrix * View Matrix * Projection Matrix
                XMMATRIX mxMVP = XMMatrixMultiply( mxWorld, mxViewProj );
                XMStoreFloat4x4( &pstCBMVP->mxMVP, mxMVP );
                //-----------------------------------------------------------------------------------------------------

                //-----------------------------------------------------------------------------------------------------
                // OnRender()
                //----------------------------------------------------------------------------------------------------- 
                //�����������Resetһ��
                GRS_THROW_IF_FAILED( pIMainCMDAlloc->Reset() );
                //Reset�����б�������ָ�������������PSO����
                if ( g_bWireFrame )
                {// ������ʾ
                    GRS_THROW_IF_FAILED( pIMainCMDList->Reset( pIMainCMDAlloc.Get(), pIPSOWireFrame.Get() ) );
                }
                else
                {
                    GRS_THROW_IF_FAILED( pIMainCMDList->Reset( pIMainCMDAlloc.Get(), pIPSOModel.Get() ) );
                }

                nFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

                pIMainCMDList->RSSetViewports( 1, &stViewPort );
                pIMainCMDList->RSSetScissorRects( 1, &stScissorRect );

                // ͨ����Դ�����ж��󻺳��Ѿ��л���Ͽ��Կ�ʼ��Ⱦ��
                stBeginResBarrier.Transition.pResource = pIARenderTargets[nFrameIndex].Get();
                pIMainCMDList->ResourceBarrier( 1, &stBeginResBarrier );

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

                // Multi Input Slot ��һ�ַ�ʽ IASetVertexBuffers
                pIMainCMDList->IASetVertexBuffers( 0, g_ncSlotCnt, staVBV );

                // Multi Input Slot �ڶ��ַ�ʽ IASetVertexBuffers
                //for (UINT i = 0; i < g_ncSlotCnt; i++)
                //{
                //	pIMainCMDList->IASetVertexBuffers(i, 1, &staVBV[i]);
                //}		

                pIMainCMDList->IASetIndexBuffer( &stIBV );

                ID3D12DescriptorHeap* ppHeaps[] = { pICBVSRVHeap.Get(),pISamplerHeap.Get() };
                pIMainCMDList->SetDescriptorHeaps( _countof( ppHeaps ), ppHeaps );

                D3D12_GPU_DESCRIPTOR_HANDLE stGPUSRVHandle = { pICBVSRVHeap->GetGPUDescriptorHandleForHeapStart() };
                // ����CBV
                pIMainCMDList->SetGraphicsRootDescriptorTable( 0, stGPUSRVHandle );

                // ����SRV
                stGPUSRVHandle.ptr += ( 2 * nCBVSRVDescriptorSize );
                //pIMainCMDList->SetGraphicsRootDescriptorTable(1, stGPUSRVHandle);

                // ����Sample
                pIMainCMDList->SetGraphicsRootDescriptorTable( 2, pISamplerHeap->GetGPUDescriptorHandleForHeapStart() );

                // Draw Call
                for ( UINT i = 0; i < g_stMeshData.m_arSubMeshInfo.GetCount(); i++ )
                {
                    UINT nHeapIndex = 0;
                    D3D12_GPU_DESCRIPTOR_HANDLE stHSRV = stGPUSRVHandle;
                    if ( g_stMeshData.m_mapTextureIndex2HeapIndex.Lookup( g_stMeshData.m_arSubMeshInfo[i].m_nMaterialIndex, nHeapIndex ) )
                    {
                        stHSRV.ptr += ( (size_t) nHeapIndex * nCBVSRVDescriptorSize );
                    }

                    pIMainCMDList->SetGraphicsRootDescriptorTable( 1, stHSRV );

                    pIMainCMDList->DrawIndexedInstanced( g_stMeshData.m_arSubMeshInfo[i].m_nNumIndices
                        , 1
                        , g_stMeshData.m_arSubMeshInfo[i].m_nBaseIndex
                        , g_stMeshData.m_arSubMeshInfo[i].m_nBaseVertex
                        , 0 );
                }

                //pIMainCMDList->DrawIndexedInstanced(g_stMeshData.m_arSubMeshInfo[0].m_nNumIndices
                //	, 1
                //	, g_stMeshData.m_arSubMeshInfo[0].m_nBaseIndex
                //	, g_stMeshData.m_arSubMeshInfo[0].m_nBaseVertex
                //	, 0);

                //��һ����Դ���ϣ�����ȷ����Ⱦ�Ѿ����������ύ����ȥ��ʾ��
                stEneResBarrier.Transition.pResource = pIARenderTargets[nFrameIndex].Get();
                pIMainCMDList->ResourceBarrier( 1, &stEneResBarrier );

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

        arTexture.RemoveAll();
        arTextureUp.RemoveAll();
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

LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    switch ( message )
    {
    case WM_DESTROY:
        PostQuitMessage( 0 );
        break;
    case WM_KEYDOWN:
    {
        USHORT n16KeyCode = ( wParam & 0xFF );

        if ( VK_TAB == n16KeyCode )
        {// ��Tab���л�ѡ��ģ���ļ�

            //OPENFILENAMEA ofn = {};
            //ofn.lStructSize = sizeof( ofn );
            //ofn.hwndOwner = hWnd;
            //ofn.lpstrFilter = "x �ļ�\0*.x;*.X\0OBJ�ļ�\0*.obj\0FBX�ļ�\0*.fbx\0�����ļ�\0*.*\0";
            //ofn.lpstrFile = g_pszModelFile;
            //ofn.nMaxFile = MAX_PATH;
            //ofn.lpstrTitle = "��ѡ��һ��Ҫ��ʾ��3Dģ���ļ�...";
            //ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

            //if ( !GetOpenFileNameA( &ofn ) )
            //{
            //    StringCchPrintfA( g_pszModelFile, MAX_PATH, "%sasset\\rgbgame\\hero.x", g_pszAppPathA );
            //}
        }

        if ( VK_SPACE == n16KeyCode )
        {// �л���������
            if ( g_stMeshData.m_paiModel->HasAnimations() )
            {
                g_stMeshData.m_nCurrentAnimIndex = ++g_stMeshData.m_nCurrentAnimIndex % g_stMeshData.m_paiModel->mNumAnimations;
                ATLTRACE( "�л�����������(%u):%s\n"
                    , g_stMeshData.m_nCurrentAnimIndex
                    , g_stMeshData.m_paiModel->mAnimations[g_stMeshData.m_nCurrentAnimIndex]->mName.C_Str() );
            }
        }

        if ( VK_F3 == n16KeyCode )
        {// F3���л�������ʾ
            g_bWireFrame = !g_bWireFrame;
        }

        float fDelta = 0.2f;

        XMVECTOR vDelta = { 0.0f,0.0f,fDelta,0.0f };
        // Z-��
        if ( VK_UP == n16KeyCode || 'w' == n16KeyCode || 'W' == n16KeyCode )
        {
            g_v4EyePos = XMVectorAdd( g_v4EyePos, vDelta );
            g_v4LookAt = XMVectorAdd( g_v4LookAt, vDelta );
        }

        if ( VK_DOWN == n16KeyCode || 's' == n16KeyCode || 'S' == n16KeyCode )
        {
            g_v4EyePos = XMVectorSubtract( g_v4EyePos, vDelta );
            g_v4LookAt = XMVectorSubtract( g_v4LookAt, vDelta );
        }

        // X-��
        vDelta = { fDelta,0.0f,0.0f,0.0f };
        if ( VK_LEFT == n16KeyCode || 'a' == n16KeyCode || 'A' == n16KeyCode )
        {
            g_v4EyePos = XMVectorAdd( g_v4EyePos, vDelta );
            g_v4LookAt = XMVectorAdd( g_v4LookAt, vDelta );
        }

        if ( VK_RIGHT == n16KeyCode || 'd' == n16KeyCode || 'D' == n16KeyCode )
        {
            g_v4EyePos = XMVectorSubtract( g_v4EyePos, vDelta );
            g_v4LookAt = XMVectorSubtract( g_v4LookAt, vDelta );
        }

        // Y-��
        vDelta = { 0.0f,fDelta,0.0f,0.0f };
        if ( VK_PRIOR == n16KeyCode || 'r' == n16KeyCode || 'R' == n16KeyCode )
        {
            g_v4EyePos = XMVectorAdd( g_v4EyePos, vDelta );
            g_v4LookAt = XMVectorAdd( g_v4LookAt, vDelta );
        }

        if ( VK_NEXT == n16KeyCode || 'f' == n16KeyCode || 'F' == n16KeyCode )
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
            if ( g_fScaling <= 0.0f )
            {
                g_fScaling = 1.0f;
            }
        }
    }
    break;

    default:
        return DefWindowProc( hWnd, message, wParam, lParam );
    }

    return DefWindowProc( hWnd, message, wParam, lParam );
}