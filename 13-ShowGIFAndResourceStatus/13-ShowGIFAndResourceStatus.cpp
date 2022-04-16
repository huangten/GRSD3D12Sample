#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // �� Windows ͷ���ų�����ʹ�õ�����
#include <windows.h>
#include <commdlg.h>
#include <tchar.h>
#include <strsafe.h>
#include <atlconv.h>	//For A2T Macro
#include <wrl.h>		//���WTL֧�� ����ʹ��COM
#include <fstream>  //for ifstream
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <d3d12.h>       //for d3d12
#include <d3dcompiler.h>
#if defined(_DEBUG)
#include <dxgidebug.h>
#endif
#include <wincodec.h>   //for WIC

using namespace std;
using namespace Microsoft;
using namespace Microsoft::WRL;
using namespace DirectX;

//linker
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

#ifndef GRS_BLOCK
#define GRS_WND_CLASS_NAME _T("GRS Game Window Class")
#define GRS_WND_TITLE	_T("GRS D3D12 Sample DirectComputer Show GIF & Resource Status")

#define GRS_THROW_IF_FAILED(hr) {HRESULT _hr = (hr);if (FAILED(_hr)){ throw CGRSCOMException(_hr); }}

//�¶���ĺ�������ȡ������
#define GRS_UPPER_DIV(A,B) ((UINT)(((A)+((B)-1))/(B)))
//���������ϱ߽�����㷨 �ڴ�����г��� ���ס
#define GRS_UPPER(A,B) ((UINT)(((A)+((B)-1))&~(B - 1)))

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

#define GRS_SET_DXGI_DEBUGNAME(x)								GRS_SetDXGIDebugName(x, L#x)
#define GRS_SET_DXGI_DEBUGNAME_INDEXED(x, n)			GRS_SetDXGIDebugNameIndexed(x[n], L#x, n)

#define GRS_SET_DXGI_DEBUGNAME_COMPTR(x)							GRS_SetDXGIDebugName(x.Get(), L#x)
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

//////////////////////////////////////////////////////////////////////////////////////////////////////////
struct WICTranslate
{
	GUID wic;
	DXGI_FORMAT format;
};

static WICTranslate g_WICFormats[] =
{//WIC��ʽ��DXGI���ظ�ʽ�Ķ�Ӧ���ñ��еĸ�ʽΪ��֧�ֵĸ�ʽ
	{ GUID_WICPixelFormat128bppRGBAFloat,       DXGI_FORMAT_R32G32B32A32_FLOAT },

	{ GUID_WICPixelFormat64bppRGBAHalf,         DXGI_FORMAT_R16G16B16A16_FLOAT },
	{ GUID_WICPixelFormat64bppRGBA,             DXGI_FORMAT_R16G16B16A16_UNORM },

	{ GUID_WICPixelFormat32bppRGBA,             DXGI_FORMAT_R8G8B8A8_UNORM },
	{ GUID_WICPixelFormat32bppBGRA,             DXGI_FORMAT_B8G8R8A8_UNORM }, // DXGI 1.1
	{ GUID_WICPixelFormat32bppBGR,              DXGI_FORMAT_B8G8R8X8_UNORM }, // DXGI 1.1

	{ GUID_WICPixelFormat32bppRGBA1010102XR,    DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM }, // DXGI 1.1
	{ GUID_WICPixelFormat32bppRGBA1010102,      DXGI_FORMAT_R10G10B10A2_UNORM },

	{ GUID_WICPixelFormat16bppBGRA5551,         DXGI_FORMAT_B5G5R5A1_UNORM },
	{ GUID_WICPixelFormat16bppBGR565,           DXGI_FORMAT_B5G6R5_UNORM },

	{ GUID_WICPixelFormat32bppGrayFloat,        DXGI_FORMAT_R32_FLOAT },
	{ GUID_WICPixelFormat16bppGrayHalf,         DXGI_FORMAT_R16_FLOAT },
	{ GUID_WICPixelFormat16bppGray,             DXGI_FORMAT_R16_UNORM },
	{ GUID_WICPixelFormat8bppGray,              DXGI_FORMAT_R8_UNORM },

	{ GUID_WICPixelFormat8bppAlpha,             DXGI_FORMAT_A8_UNORM },
};

// WIC ���ظ�ʽת����.
struct WICConvert
{
	GUID source;
	GUID target;
};

static WICConvert g_WICConvert[] =
{
	// Ŀ���ʽһ������ӽ��ı�֧�ֵĸ�ʽ
	{ GUID_WICPixelFormatBlackWhite,            GUID_WICPixelFormat8bppGray }, // DXGI_FORMAT_R8_UNORM

	{ GUID_WICPixelFormat1bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat2bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat4bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat8bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM

	{ GUID_WICPixelFormat2bppGray,              GUID_WICPixelFormat8bppGray }, // DXGI_FORMAT_R8_UNORM
	{ GUID_WICPixelFormat4bppGray,              GUID_WICPixelFormat8bppGray }, // DXGI_FORMAT_R8_UNORM

	{ GUID_WICPixelFormat16bppGrayFixedPoint,   GUID_WICPixelFormat16bppGrayHalf }, // DXGI_FORMAT_R16_FLOAT
	{ GUID_WICPixelFormat32bppGrayFixedPoint,   GUID_WICPixelFormat32bppGrayFloat }, // DXGI_FORMAT_R32_FLOAT

	{ GUID_WICPixelFormat16bppBGR555,           GUID_WICPixelFormat16bppBGRA5551 }, // DXGI_FORMAT_B5G5R5A1_UNORM

	{ GUID_WICPixelFormat32bppBGR101010,        GUID_WICPixelFormat32bppRGBA1010102 }, // DXGI_FORMAT_R10G10B10A2_UNORM

	{ GUID_WICPixelFormat24bppBGR,              GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat24bppRGB,              GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat32bppPBGRA,            GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat32bppPRGBA,            GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM

	{ GUID_WICPixelFormat48bppRGB,              GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat48bppBGR,              GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat64bppBGRA,             GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat64bppPRGBA,            GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat64bppPBGRA,            GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM

	{ GUID_WICPixelFormat48bppRGBFixedPoint,    GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
	{ GUID_WICPixelFormat48bppBGRFixedPoint,    GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
	{ GUID_WICPixelFormat64bppRGBAFixedPoint,   GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
	{ GUID_WICPixelFormat64bppBGRAFixedPoint,   GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
	{ GUID_WICPixelFormat64bppRGBFixedPoint,    GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
	{ GUID_WICPixelFormat48bppRGBHalf,          GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
	{ GUID_WICPixelFormat64bppRGBHalf,          GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT

	{ GUID_WICPixelFormat128bppPRGBAFloat,      GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
	{ GUID_WICPixelFormat128bppRGBFloat,        GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
	{ GUID_WICPixelFormat128bppRGBAFixedPoint,  GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
	{ GUID_WICPixelFormat128bppRGBFixedPoint,   GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
	{ GUID_WICPixelFormat32bppRGBE,             GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT

	{ GUID_WICPixelFormat32bppCMYK,             GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat64bppCMYK,             GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat40bppCMYKAlpha,        GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat80bppCMYKAlpha,        GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM

	{ GUID_WICPixelFormat32bppRGB,              GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat64bppRGB,              GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat64bppPRGBAHalf,        GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
};

bool GetTargetPixelFormat(const GUID* pSourceFormat, GUID* pTargetFormat)
{//���ȷ�����ݵ���ӽ���ʽ���ĸ�
	*pTargetFormat = *pSourceFormat;
	for (size_t i = 0; i < _countof(g_WICConvert); ++i)
	{
		if (InlineIsEqualGUID(g_WICConvert[i].source, *pSourceFormat))
		{
			*pTargetFormat = g_WICConvert[i].target;
			return true;
		}
	}
	return false;
}

DXGI_FORMAT GetDXGIFormatFromPixelFormat(const GUID* pPixelFormat)
{//���ȷ�����ն�Ӧ��DXGI��ʽ����һ��
	for (size_t i = 0; i < _countof(g_WICFormats); ++i)
	{
		if (InlineIsEqualGUID(g_WICFormats[i].wic, *pPixelFormat))
		{
			return g_WICFormats[i].format;
		}
	}
	return DXGI_FORMAT_UNKNOWN;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif

struct ST_GRS_VERTEX
{
	XMFLOAT4 m_v4Position;		//Position
	XMFLOAT2 m_vTxc;		//Texcoord
	XMFLOAT3 m_vNor;		//Normal
};

struct ST_GRS_FRAME_MVP_BUFFER
{
	XMFLOAT4X4 m_MVP;			//�����Model-view-projection(MVP)����.
};

//--------------------------------------------------------------------------------------------------
//GIF Image Information struct and macro

#define GRS_ARGB_A(ARGBColor) (((float)((ARGBColor & 0xFF000000) >> 24))/255.0f)
#define GRS_ARGB_R(ARGBColor) (((float)((ARGBColor & 0x00FF0000) >> 16))/255.0f)
#define GRS_ARGB_G(ARGBColor) (((float)((ARGBColor & 0x0000FF00) >>  8))/255.0f)
#define GRS_ARGB_B(ARGBColor) (((float)( ARGBColor & 0x000000FF))/255.0f)

struct ST_GRS_GIF_FRAME_PARAM
{
	XMFLOAT4 m_c4BkColor;
	UINT	 m_nLeftTop[2];
	UINT	 m_nFrameWH[2];
	UINT	 m_nFrame;
	UINT 	 m_nDisposal;
};

enum EM_GRS_DISPOSAL_METHODS
{
	DM_UNDEFINED = 0,
	DM_NONE = 1,
	DM_BACKGROUND = 2,
	DM_PREVIOUS = 3
};
struct ST_GRS_GIF
{
	ComPtr<IWICBitmapDecoder>	m_pIWICDecoder;
	UINT						m_nTotalLoopCount;
	UINT						m_nLoopNumber;
	BOOL						m_bHasLoop;
	UINT						m_nFrames;
	UINT						m_nCurrentFrame;
	WICColor					m_nBkColor;
	UINT						m_nWidth;
	UINT						m_nHeight;
	UINT						m_nPixelWidth;
	UINT						m_nPixelHeight;
};

struct ST_GRS_RECT_F
{
	float m_fLeft;
	float m_fTop;
	float m_fRight;
	float m_fBottom;
};

struct ST_GRS_GIF_FRAME
{
	ComPtr<IWICBitmapFrameDecode>	m_pIWICGIFFrame;
	ComPtr<IWICBitmapSource>		m_pIWICGIFFrameBMP;
	DXGI_FORMAT						m_enTextureFormat;
	UINT							m_nFrameDisposal;
	UINT							m_nFrameDelay;
	UINT						    m_nLeftTop[2];
	UINT							m_nFrameWH[2];
	//ST_GRS_RECT_F					m_rtGIFFrame;
};
//--------------------------------------------------------------------------------------------------

//��ʼ��Ĭ���������λ��
XMVECTOR g_v4EyePos = XMVectorSet(0.0f, 2.0f, -5.0f, 0.0f); //�۾�λ��
XMVECTOR g_v4LookAt = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);    //�۾�������λ��
XMVECTOR g_v4UpDir = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);    //ͷ�����Ϸ�λ��

double g_fPalstance = 10.0f * XM_PI / 180.0f;	//������ת�Ľ��ٶȣ���λ������/��

//GIF �ļ���Ϣ
TCHAR						 g_pszAppPath[MAX_PATH] = {};
WCHAR						 g_pszTexcuteFileName[MAX_PATH] = {};
ST_GRS_GIF				     g_stGIF = {};

ComPtr<ID3D12Device4>		 g_pID3D12Device4;
ComPtr<IWICImagingFactory>	 g_pIWICFactory;
ComPtr<ID3D12Resource>		 g_pIRWTexture;
ComPtr<ID3D12DescriptorHeap> g_pICSSRVHeap;	  // Computer Shader CBV SRV Heap
HANDLE						 g_hEventFence = nullptr;
BOOL LoadGIF(LPCWSTR pszGIFFileName, IWICImagingFactory* pIWICFactory, ST_GRS_GIF& g_stGIF, const WICColor& nDefaultBkColor = 0u);
BOOL LoadGIFFrame(IWICImagingFactory* pIWICFactory, ST_GRS_GIF& g_stGIF, ST_GRS_GIF_FRAME& stGIFFrame);
BOOL UploadGIFFrame(ID3D12Device4* pID3D12Device4, ID3D12GraphicsCommandList* pICMDList, IWICImagingFactory* pIWICFactory, ST_GRS_GIF_FRAME& stGIFFrame, ID3D12Resource** pIGIFFrame, ID3D12Resource** pIGIFFrameUpload);

BOOL LoadMeshVertex(const CHAR* pszMeshFileName, UINT& nVertexCnt, ST_GRS_VERTEX*& ppVertex, UINT*& ppIndices);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);


int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	try
	{
		GRS_THROW_IF_FAILED(::CoInitialize(nullptr));  //for WIC & COM
		HWND												hWnd = nullptr;
		MSG													msg = {};

		int													iWidth = 1024;
		int													iHeight = 768;

		const UINT										nFrameBackBufCount = 3u;
		UINT												nCurrentFrameIndex = 0;
		DXGI_FORMAT								emRenderTarget = DXGI_FORMAT_R8G8B8A8_UNORM;
		const float										c4ClearColor[] = { 0.2f, 0.5f, 1.0f, 1.0f };

		ComPtr<IDXGIFactory5>				pIDXGIFactory5;
		ComPtr<IDXGIFactory6>				pIDXGIFactory6;
		ComPtr<IDXGIAdapter1>				pIAdapter1;


		ComPtr<ID3D12CommandQueue>			pICMDQueue;  //Direct Command Queue (3D Engine & Other Engine)
		ComPtr<ID3D12CommandQueue>			pICSQueue;	 //Computer Command Queue (Computer Engine)

		ComPtr<IDXGISwapChain1>						pISwapChain1;
		ComPtr<IDXGISwapChain3>						pISwapChain3;
		ComPtr<ID3D12Resource>						pIARenderTargets[nFrameBackBufCount];
		ComPtr<ID3D12DescriptorHeap>				pIRTVHeap;

		ComPtr<ID3D12Fence>								pIFence;

		UINT64														n64FenceValue = 0ui64;

		ComPtr<ID3D12CommandAllocator>		pICMDAlloc;
		ComPtr<ID3D12GraphicsCommandList>	pICMDList;

		ComPtr<ID3D12CommandAllocator>		pICSAlloc;		// Computer Command Alloc
		ComPtr<ID3D12GraphicsCommandList>	pICSList;		// Computer Command List

		ComPtr<ID3D12RootSignature>		pIRootSignature;
		ComPtr<ID3D12PipelineState>			pIPipelineState;

		ComPtr<ID3D12RootSignature>		pIRSComputer;	// Computer Shader Root Signature
		ComPtr<ID3D12PipelineState>			pIPSOComputer;  // Computer Pipeline Status Object

		UINT													nRTVDescriptorSize = 0U;
		UINT													nSRVDescriptorSize = 0U;

		ComPtr<ID3D12Resource>				pICBVRes;
		ComPtr<ID3D12Resource>				pIVBRes;
		ComPtr<ID3D12Resource>				pIIBRes;
		ComPtr<ID3D12Resource>				pITexture;
		ComPtr<ID3D12Resource>				pITextureUpload;
		ComPtr<ID3D12DescriptorHeap>		pISRVHeap;

		ComPtr<ID3D12Resource>				pICBGIFFrameInfo; // Computer Shader Const Buffer 

		SIZE_T													szGIFFrameInfo = GRS_UPPER(sizeof(ST_GRS_GIF_FRAME_PARAM), 256);
		ST_GRS_GIF_FRAME_PARAM* pstGIFFrameInfo = nullptr;

		D3D12_VIEWPORT								stViewPort = { 0.0f, 0.0f, static_cast<float>(iWidth), static_cast<float>(iHeight), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
		D3D12_RECT										stScissorRect = { 0, 0, static_cast<LONG>(iWidth), static_cast<LONG>(iHeight) };

		ST_GRS_FRAME_MVP_BUFFER* pMVPBuffer = nullptr;
		SIZE_T													szMVPBuffer = GRS_UPPER(sizeof(ST_GRS_FRAME_MVP_BUFFER), 256);

		UINT													nVertexCnt = 0;
		D3D12_VERTEX_BUFFER_VIEW			stVertexBufferView = {};
		D3D12_INDEX_BUFFER_VIEW				stIndexBufferView = {};

		ST_GRS_GIF_FRAME								stGIFFrame = {};

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
			RECT rtWnd = { 0, 0, iWidth, iHeight };
			AdjustWindowRect(&rtWnd, dwWndStyle, FALSE);

			// ���㴰�ھ��е���Ļ����
			INT posX = (GetSystemMetrics(SM_CXSCREEN) - rtWnd.right - rtWnd.left) / 2;
			INT posY = (GetSystemMetrics(SM_CYSCREEN) - rtWnd.bottom - rtWnd.top) / 2;

			hWnd = CreateWindowW(GRS_WND_CLASS_NAME
				, GRS_WND_TITLE
				, dwWndStyle
				, posX
				, posY
				, rtWnd.right - rtWnd.left
				, rtWnd.bottom - rtWnd.top
				, nullptr
				, nullptr
				, hInstance
				, nullptr);

			if (!hWnd)
			{
				return FALSE;
			}
		}

		// ����DXGI Factory����		
		{
			UINT nDXGIFactoryFlags = 0U;
#if defined(_DEBUG)
			// ����ʾ��ϵͳ�ĵ���֧��
			{
				ComPtr<ID3D12Debug> debugController;
				if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
				{
					debugController->EnableDebugLayer();
					// �򿪸��ӵĵ���֧��
					nDXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
				}
			}
#endif
			GRS_THROW_IF_FAILED(CreateDXGIFactory2(nDXGIFactoryFlags, IID_PPV_ARGS(&pIDXGIFactory5)));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pIDXGIFactory5);
			//��ȡIDXGIFactory6�ӿ�
			GRS_THROW_IF_FAILED(pIDXGIFactory5.As(&pIDXGIFactory6));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pIDXGIFactory6);
		}

		// ö����������������������豸����
		{
			GRS_THROW_IF_FAILED(pIDXGIFactory6->EnumAdapterByGpuPreference(0
				, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
				, IID_PPV_ARGS(&pIAdapter1)));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pIAdapter1);
			// ����D3D12.1���豸
			GRS_THROW_IF_FAILED(D3D12CreateDevice(pIAdapter1.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&g_pID3D12Device4)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(g_pID3D12Device4);

			DXGI_ADAPTER_DESC1 stAdapterDesc = {};
			GRS_THROW_IF_FAILED(pIAdapter1->GetDesc1(&stAdapterDesc));

			TCHAR pszWndTitle[MAX_PATH] = {};
			GRS_THROW_IF_FAILED(pIAdapter1->GetDesc1(&stAdapterDesc));
			::GetWindowText(hWnd, pszWndTitle, MAX_PATH);
			StringCchPrintf(pszWndTitle
				, MAX_PATH
				, _T("%s (GPU:%s)")
				, pszWndTitle
				, stAdapterDesc.Description);
			::SetWindowText(hWnd, pszWndTitle);
		}

		// �����������
		{
			D3D12_COMMAND_QUEUE_DESC stQueueDesc = {};
			stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			//����ֱ���������
			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateCommandQueue(&stQueueDesc, IID_PPV_ARGS(&pICMDQueue)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICMDQueue);

			stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;

			//���������������
			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateCommandQueue(&stQueueDesc, IID_PPV_ARGS(&pICSQueue)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICSQueue);
		}

		// ���������б�&������
		{
			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&pICMDAlloc)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICMDAlloc);
			// ����ͼ�������б�
			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pICMDAlloc.Get(), pIPipelineState.Get(), IID_PPV_ARGS(&pICMDList)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICMDList);

			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&pICSAlloc)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICSAlloc);
			// �������������б�
			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, pICSAlloc.Get(), nullptr, IID_PPV_ARGS(&pICSList)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICSList);

		}

		// ����������
		{
			DXGI_SWAP_CHAIN_DESC1 stSwapChainDesc = {};
			stSwapChainDesc.BufferCount = nFrameBackBufCount;
			stSwapChainDesc.Width = iWidth;
			stSwapChainDesc.Height = iHeight;
			stSwapChainDesc.Format = emRenderTarget;
			stSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			stSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			stSwapChainDesc.SampleDesc.Count = 1;

			GRS_THROW_IF_FAILED(pIDXGIFactory5->CreateSwapChainForHwnd(
				pICMDQueue.Get(),
				hWnd,
				&stSwapChainDesc,
				nullptr,
				nullptr,
				&pISwapChain1
			));

			GRS_SET_DXGI_DEBUGNAME_COMPTR(pISwapChain1);

			//�õ���ǰ�󻺳�������ţ�Ҳ������һ����Ҫ������ʾ�Ļ����������
			//ע��˴�ʹ���˸߰汾��SwapChain�ӿڵĺ���
			GRS_THROW_IF_FAILED(pISwapChain1.As(&pISwapChain3));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pISwapChain3);
			nCurrentFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

			//����RTV(��ȾĿ����ͼ)��������(����ѵĺ���Ӧ�����Ϊ������߹̶���СԪ�صĹ̶���С�Դ��)
			D3D12_DESCRIPTOR_HEAP_DESC stRTVHeapDesc = {};
			stRTVHeapDesc.NumDescriptors = nFrameBackBufCount;
			stRTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			stRTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateDescriptorHeap(&stRTVHeapDesc, IID_PPV_ARGS(&pIRTVHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRTVHeap);
			//�õ�ÿ��������Ԫ�صĴ�С
			nRTVDescriptorSize = g_pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			nSRVDescriptorSize = g_pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			//����RTV��������
			D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle = pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
			for (UINT i = 0; i < nFrameBackBufCount; i++)
			{
				GRS_THROW_IF_FAILED(pISwapChain3->GetBuffer(i, IID_PPV_ARGS(&pIARenderTargets[i])));
				GRS_SET_D3D12_DEBUGNAME_INDEXED_COMPTR(pIARenderTargets, i);
				g_pID3D12Device4->CreateRenderTargetView(pIARenderTargets[i].Get(), nullptr, stRTVHandle);
				stRTVHandle.ptr += nRTVDescriptorSize;
			}
			// �ر�ALT+ENTER���л�ȫ���Ĺ��ܣ���Ϊ����û��ʵ��OnSize���������ȹر�
			GRS_THROW_IF_FAILED(pIDXGIFactory5->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
		}

		// ����Χ��������CPU��GPU�߳�֮���ͬ��
		{
			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pIFence)));
			n64FenceValue = 1;
			// ����һ��Eventͬ���������ڵȴ�Χ���¼�֪ͨ
			g_hEventFence = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (g_hEventFence == nullptr)
			{
				GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
			}
		}

		// ������������
		{
			D3D12_FEATURE_DATA_ROOT_SIGNATURE stFeatureData = {};
			// ����Ƿ�֧��V1.1�汾�ĸ�ǩ��
			stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
			if (FAILED(g_pID3D12Device4->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &stFeatureData, sizeof(stFeatureData))))
			{
				stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
			}

			D3D12_STATIC_SAMPLER_DESC stSamplerDesc[1] = {};
			stSamplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
			stSamplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			stSamplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			stSamplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			stSamplerDesc[0].MipLODBias = 0;
			stSamplerDesc[0].MaxAnisotropy = 0;
			stSamplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			stSamplerDesc[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
			stSamplerDesc[0].MinLOD = 0.0f;
			stSamplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX;
			stSamplerDesc[0].ShaderRegister = 0;
			stSamplerDesc[0].RegisterSpace = 0;
			stSamplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

			D3D12_VERSIONED_ROOT_SIGNATURE_DESC stRootSignatureDesc = {};

			if (D3D_ROOT_SIGNATURE_VERSION_1_1 == stFeatureData.HighestVersion)
			{
				// ��GPU��ִ��SetGraphicsRootDescriptorTable�����ǲ��޸������б��е�SRV��������ǿ���ʹ��Ĭ��Rang��Ϊ:
				// D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE
				D3D12_DESCRIPTOR_RANGE1 stDSPRanges1[2] = {};
				stDSPRanges1[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				stDSPRanges1[0].NumDescriptors = 1;
				stDSPRanges1[0].BaseShaderRegister = 0;
				stDSPRanges1[0].RegisterSpace = 0;
				stDSPRanges1[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
				stDSPRanges1[0].OffsetInDescriptorsFromTableStart = 0;

				stDSPRanges1[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
				stDSPRanges1[1].NumDescriptors = 1;
				stDSPRanges1[1].BaseShaderRegister = 0;
				stDSPRanges1[1].RegisterSpace = 0;
				stDSPRanges1[1].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
				stDSPRanges1[1].OffsetInDescriptorsFromTableStart = 0;

				D3D12_ROOT_PARAMETER1 stRootParameters1[2] = {};
				stRootParameters1[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				stRootParameters1[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
				stRootParameters1[0].DescriptorTable.NumDescriptorRanges = 1;
				stRootParameters1[0].DescriptorTable.pDescriptorRanges = &stDSPRanges1[0];

				stRootParameters1[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				stRootParameters1[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				stRootParameters1[1].DescriptorTable.NumDescriptorRanges = 1;
				stRootParameters1[1].DescriptorTable.pDescriptorRanges = &stDSPRanges1[1];

				stRootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
				stRootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
				stRootSignatureDesc.Desc_1_1.NumParameters = _countof(stRootParameters1);
				stRootSignatureDesc.Desc_1_1.pParameters = stRootParameters1;
				stRootSignatureDesc.Desc_1_1.NumStaticSamplers = _countof(stSamplerDesc);
				stRootSignatureDesc.Desc_1_1.pStaticSamplers = stSamplerDesc;
			}
			else
			{
				D3D12_DESCRIPTOR_RANGE stDSPRanges[2] = {};
				stDSPRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				stDSPRanges[0].NumDescriptors = 1;
				stDSPRanges[0].BaseShaderRegister = 0;
				stDSPRanges[0].RegisterSpace = 0;
				stDSPRanges[0].OffsetInDescriptorsFromTableStart = 0;

				stDSPRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
				stDSPRanges[1].NumDescriptors = 1;
				stDSPRanges[1].BaseShaderRegister = 0;
				stDSPRanges[1].RegisterSpace = 0;
				stDSPRanges[1].OffsetInDescriptorsFromTableStart = 0;

				D3D12_ROOT_PARAMETER stRootParameters[2] = {};
				stRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				stRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
				stRootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
				stRootParameters[0].DescriptorTable.pDescriptorRanges = &stDSPRanges[0];

				stRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				stRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				stRootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
				stRootParameters[1].DescriptorTable.pDescriptorRanges = &stDSPRanges[1];

				stRootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_0;
				stRootSignatureDesc.Desc_1_0.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
				stRootSignatureDesc.Desc_1_0.NumParameters = _countof(stRootParameters);
				stRootSignatureDesc.Desc_1_0.pParameters = stRootParameters;
				stRootSignatureDesc.Desc_1_0.NumStaticSamplers = _countof(stSamplerDesc);
				stRootSignatureDesc.Desc_1_0.pStaticSamplers = stSamplerDesc;
			}

			ComPtr<ID3DBlob> pISignatureBlob;
			ComPtr<ID3DBlob> pIErrorBlob;

			GRS_THROW_IF_FAILED(D3D12SerializeVersionedRootSignature(&stRootSignatureDesc
				, &pISignatureBlob
				, &pIErrorBlob));

			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateRootSignature(0
				, pISignatureBlob->GetBufferPointer()
				, pISignatureBlob->GetBufferSize()
				, IID_PPV_ARGS(&pIRootSignature)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRootSignature);

			//------------------------------------------------------------------------------------------------------------
			// ����������ߵĸ�ǩ��
			D3D12_VERSIONED_ROOT_SIGNATURE_DESC stRSComputerDesc = {};
			if (D3D_ROOT_SIGNATURE_VERSION_1_1 == stFeatureData.HighestVersion)
			{
				// ��GPU��ִ��SetGraphicsRootDescriptorTable�����ǲ��޸������б��е�SRV��������ǿ���ʹ��Ĭ��Rang��Ϊ:
				// D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE
				D3D12_DESCRIPTOR_RANGE1 stDSPRanges1[3] = {};
				stDSPRanges1[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
				stDSPRanges1[0].NumDescriptors = 1; // 1 Const Buffer View + 1 Texture View
				stDSPRanges1[0].BaseShaderRegister = 0;
				stDSPRanges1[0].RegisterSpace = 0;
				stDSPRanges1[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
				stDSPRanges1[0].OffsetInDescriptorsFromTableStart = 0;

				stDSPRanges1[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				stDSPRanges1[1].NumDescriptors = 1; // 1 Const Buffer View + 1 Texture View
				stDSPRanges1[1].BaseShaderRegister = 0;
				stDSPRanges1[1].RegisterSpace = 0;
				stDSPRanges1[1].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
				stDSPRanges1[1].OffsetInDescriptorsFromTableStart = 0;

				stDSPRanges1[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
				stDSPRanges1[2].NumDescriptors = 1;
				stDSPRanges1[2].BaseShaderRegister = 0;
				stDSPRanges1[2].RegisterSpace = 0;
				stDSPRanges1[2].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
				stDSPRanges1[2].OffsetInDescriptorsFromTableStart = 0;

				D3D12_ROOT_PARAMETER1 stRootParameters1[3] = {};
				stRootParameters1[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				stRootParameters1[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				stRootParameters1[0].DescriptorTable.NumDescriptorRanges = 1;
				stRootParameters1[0].DescriptorTable.pDescriptorRanges = &stDSPRanges1[0];

				stRootParameters1[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				stRootParameters1[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				stRootParameters1[1].DescriptorTable.NumDescriptorRanges = 1;
				stRootParameters1[1].DescriptorTable.pDescriptorRanges = &stDSPRanges1[1];

				stRootParameters1[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				stRootParameters1[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				stRootParameters1[2].DescriptorTable.NumDescriptorRanges = 1;
				stRootParameters1[2].DescriptorTable.pDescriptorRanges = &stDSPRanges1[2];

				stRSComputerDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
				stRSComputerDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
				stRSComputerDesc.Desc_1_1.NumParameters = _countof(stRootParameters1);
				stRSComputerDesc.Desc_1_1.pParameters = stRootParameters1;
				stRSComputerDesc.Desc_1_1.NumStaticSamplers = 0;
				stRSComputerDesc.Desc_1_1.pStaticSamplers = nullptr;
			}
			else
			{
				D3D12_DESCRIPTOR_RANGE stDSPRanges[3] = {};
				stDSPRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
				stDSPRanges[0].NumDescriptors = 1;		// 1 Const Buffer View + 1 Texture View
				stDSPRanges[0].BaseShaderRegister = 0;
				stDSPRanges[0].RegisterSpace = 0;
				stDSPRanges[0].OffsetInDescriptorsFromTableStart = 0;

				stDSPRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				stDSPRanges[1].NumDescriptors = 1;		// 1 Const Buffer View + 1 Texture View
				stDSPRanges[1].BaseShaderRegister = 0;
				stDSPRanges[1].RegisterSpace = 0;
				stDSPRanges[1].OffsetInDescriptorsFromTableStart = 0;

				stDSPRanges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
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
				stRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				stRootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
				stRootParameters[1].DescriptorTable.pDescriptorRanges = &stDSPRanges[1];

				stRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				stRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				stRootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
				stRootParameters[2].DescriptorTable.pDescriptorRanges = &stDSPRanges[2];

				stRSComputerDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_0;
				stRSComputerDesc.Desc_1_0.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
				stRSComputerDesc.Desc_1_0.NumParameters = _countof(stRootParameters);
				stRSComputerDesc.Desc_1_0.pParameters = stRootParameters;
				stRSComputerDesc.Desc_1_0.NumStaticSamplers = 0;
				stRSComputerDesc.Desc_1_0.pStaticSamplers = nullptr;
			}

			pISignatureBlob.Reset();
			pIErrorBlob.Reset();

			GRS_THROW_IF_FAILED(D3D12SerializeVersionedRootSignature(&stRSComputerDesc, &pISignatureBlob, &pIErrorBlob));

			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateRootSignature(0
				, pISignatureBlob->GetBufferPointer()
				, pISignatureBlob->GetBufferSize()
				, IID_PPV_ARGS(&pIRSComputer)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRSComputer);
		}

		// ����Shader������Ⱦ����״̬����
		{ {
				ComPtr<ID3DBlob> pIVSCode;
				ComPtr<ID3DBlob> pIPSCode;
#if defined(_DEBUG)
				// Enable better shader debugging with the graphics debugging tools.
				UINT nShaderCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
				UINT nShaderCompileFlags = 0;
#endif
				//����Ϊ�о�����ʽ	   
				nShaderCompileFlags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

				TCHAR pszShaderFileName[MAX_PATH] = {};
				StringCchPrintf(pszShaderFileName, MAX_PATH, _T("%s13-ShowGIFAndResourceStatus\\Shader\\13-ShowGIFAndResourceStatus.hlsl"), g_pszAppPath);

				GRS_THROW_IF_FAILED(D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
					, "VSMain", "vs_5_0", nShaderCompileFlags, 0, &pIVSCode, nullptr));
				GRS_THROW_IF_FAILED(D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
					, "PSMain", "ps_5_0", nShaderCompileFlags, 0, &pIPSCode, nullptr));

				// Define the vertex input layout.
				D3D12_INPUT_ELEMENT_DESC stInputElementDescs[] =
				{
					{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
					{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
				};

				// ���� graphics pipeline state object (PSO)����
				D3D12_GRAPHICS_PIPELINE_STATE_DESC stPSODesc = {};
				stPSODesc.InputLayout = { stInputElementDescs, _countof(stInputElementDescs) };
				stPSODesc.pRootSignature = pIRootSignature.Get();

				stPSODesc.VS.pShaderBytecode = pIVSCode->GetBufferPointer();
				stPSODesc.VS.BytecodeLength = pIVSCode->GetBufferSize();

				stPSODesc.PS.pShaderBytecode = pIPSCode->GetBufferPointer();
				stPSODesc.PS.BytecodeLength = pIPSCode->GetBufferSize();

				stPSODesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
				stPSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;

				stPSODesc.BlendState.AlphaToCoverageEnable = FALSE;
				stPSODesc.BlendState.IndependentBlendEnable = FALSE;
				stPSODesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

				stPSODesc.DepthStencilState.DepthEnable = FALSE;
				stPSODesc.DepthStencilState.StencilEnable = FALSE;

				stPSODesc.SampleMask = UINT_MAX;
				stPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				stPSODesc.NumRenderTargets = 1;
				stPSODesc.RTVFormats[0] = emRenderTarget;
				stPSODesc.SampleDesc.Count = 1;

				GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateGraphicsPipelineState(&stPSODesc, IID_PPV_ARGS(&pIPipelineState)));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(pIPipelineState);
			} }

		// �����������״̬����
		{

#if defined(_DEBUG)
			// Enable better shader debugging with the graphics debugging tools.
			UINT nShaderCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
			UINT nShaderCompileFlags = 0;
#endif
			//����Ϊ�о�����ʽ	   
			//nShaderCompileFlags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

			TCHAR pszShaderFileName[MAX_PATH] = {};
			StringCchPrintf(pszShaderFileName
				, MAX_PATH
				, _T("%s13-ShowGIFAndResourceStatus\\Shader\\13-ShowGIF_CS.hlsl")
				, g_pszAppPath);

			ComPtr<ID3DBlob> pICSCode;
			ComPtr<ID3DBlob> pIErrMsg;
			CHAR pszErrMsg[MAX_PATH] = {};
			HRESULT hr = S_OK;

			hr = D3DCompileFromFile(pszShaderFileName
				, nullptr
				, nullptr
				, "ShowGIFCS"
				, "cs_5_0"
				, nShaderCompileFlags
				, 0
				, &pICSCode
				, &pIErrMsg);
			if (FAILED(hr))
			{
				StringCchPrintfA(pszErrMsg, MAX_PATH, "\n%s\n", (CHAR*)pIErrMsg->GetBufferPointer());
				::OutputDebugStringA(pszErrMsg);
				throw CGRSCOMException(hr);
			}

			// ���� graphics pipeline state object (PSO)����
			D3D12_COMPUTE_PIPELINE_STATE_DESC stPSODesc = {};
			stPSODesc.pRootSignature = pIRSComputer.Get();
			stPSODesc.CS.pShaderBytecode = pICSCode->GetBufferPointer();
			stPSODesc.CS.BytecodeLength = pICSCode->GetBufferSize();

			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateComputePipelineState(&stPSODesc, IID_PPV_ARGS(&pIPSOComputer)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIPSOComputer);
		}

		// ������������
		{
			D3D12_HEAP_PROPERTIES stHeapProp = { D3D12_HEAP_TYPE_UPLOAD };

			D3D12_RESOURCE_DESC stResDesc = {};
			stResDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			stResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			stResDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			stResDesc.Format = DXGI_FORMAT_UNKNOWN;
			stResDesc.Width = szMVPBuffer;
			stResDesc.Height = 1;
			stResDesc.DepthOrArraySize = 1;
			stResDesc.MipLevels = 1;
			stResDesc.SampleDesc.Count = 1;
			stResDesc.SampleDesc.Quality = 0;

			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateCommittedResource(
				&stHeapProp,
				D3D12_HEAP_FLAG_NONE,
				&stResDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&pICBVRes)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICBVRes);

			// Map ֮��Ͳ���Unmap�� ֱ�Ӹ������ݽ�ȥ ����ÿ֡������map-copy-unmap�˷�ʱ����
			GRS_THROW_IF_FAILED(pICBVRes->Map(0, nullptr, reinterpret_cast<void**>(&pMVPBuffer)));

			//-----------------------------------------------------------------------------------------------------
			stResDesc.Width = szGIFFrameInfo;

			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateCommittedResource(
				&stHeapProp,
				D3D12_HEAP_FLAG_NONE,
				&stResDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&pICBGIFFrameInfo)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICBGIFFrameInfo);

			// Map ֮��Ͳ���Unmap�� ֱ�Ӹ������ݽ�ȥ ����ÿ֡������map-copy-unmap�˷�ʱ����
			GRS_THROW_IF_FAILED(pICBGIFFrameInfo->Map(0, nullptr, reinterpret_cast<void**>(&pstGIFFrameInfo)));
			//-----------------------------------------------------------------------------------------------------
		}

		// �������㻺��
		{
			ST_GRS_VERTEX* pstVertices = nullptr;
			UINT* pnIndices = nullptr;
			USES_CONVERSION;
			CHAR pszMeshFileName[MAX_PATH] = {};
			StringCchPrintfA(pszMeshFileName, MAX_PATH, "%sAssets\\Cube.txt", T2A(g_pszAppPath));
			//���뷽��
			LoadMeshVertex(pszMeshFileName, nVertexCnt, pstVertices, pnIndices);

			UINT nIndexCnt = nVertexCnt;

			UINT nVertexBufferSize = nVertexCnt * sizeof(ST_GRS_VERTEX);

			D3D12_HEAP_PROPERTIES stHeapProp = { D3D12_HEAP_TYPE_UPLOAD };

			D3D12_RESOURCE_DESC stResDesc = {};
			stResDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			stResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			stResDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			stResDesc.Format = DXGI_FORMAT_UNKNOWN;
			stResDesc.Width = nVertexBufferSize;
			stResDesc.Height = 1;
			stResDesc.DepthOrArraySize = 1;
			stResDesc.MipLevels = 1;
			stResDesc.SampleDesc.Count = 1;
			stResDesc.SampleDesc.Quality = 0;

			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateCommittedResource(
				&stHeapProp,
				D3D12_HEAP_FLAG_NONE,
				&stResDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&pIVBRes)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIVBRes);

			UINT8* pVertexDataBegin = nullptr;
			D3D12_RANGE stReadRange = { 0, 0 };
			GRS_THROW_IF_FAILED(pIVBRes->Map(0, &stReadRange, reinterpret_cast<void**>(&pVertexDataBegin)));
			memcpy(pVertexDataBegin, pstVertices, nVertexBufferSize);
			pIVBRes->Unmap(0, nullptr);

			stVertexBufferView.BufferLocation = pIVBRes->GetGPUVirtualAddress();
			stVertexBufferView.StrideInBytes = sizeof(ST_GRS_VERTEX);
			stVertexBufferView.SizeInBytes = nVertexBufferSize;

			stResDesc.Width = nIndexCnt * sizeof(UINT);
			//���� Index Buffer ��ʹ��Upload��ʽ��
			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateCommittedResource(
				&stHeapProp
				, D3D12_HEAP_FLAG_NONE
				, &stResDesc
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pIIBRes)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIIBRes);

			UINT8* pIndexDataBegin = nullptr;
			GRS_THROW_IF_FAILED(pIIBRes->Map(0, &stReadRange, reinterpret_cast<void**>(&pIndexDataBegin)));
			memcpy(pIndexDataBegin, pnIndices, nIndexCnt * sizeof(UINT));
			pIIBRes->Unmap(0, nullptr);

			//����Index Buffer View
			stIndexBufferView.BufferLocation = pIIBRes->GetGPUVirtualAddress();
			stIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
			stIndexBufferView.SizeInBytes = nIndexCnt * sizeof(UINT);

			::HeapFree(::GetProcessHeap(), 0, pstVertices);
			::HeapFree(::GetProcessHeap(), 0, pnIndices);
		}

		// ʹ��WIC����������һ��2D����
		{
			//ʹ�ô�COM��ʽ����WIC�೧����Ҳ�ǵ���WIC��һ��Ҫ��������
			GRS_THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&g_pIWICFactory)));
			//ʹ��WIC�೧����ӿڼ�������ͼƬ�����õ�һ��WIC����������ӿڣ�ͼƬ��Ϣ��������ӿڴ���Ķ�������
			StringCchPrintfW(g_pszTexcuteFileName, MAX_PATH, _T("%sAssets\\MM.gif"), g_pszAppPath);

			OPENFILENAME ofn;
			RtlZeroMemory(&ofn, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = nullptr;
			ofn.lpstrFilter = L"*Gif �ļ�\0*.gif\0";
			ofn.lpstrFile = g_pszTexcuteFileName;
			ofn.nMaxFile = MAX_PATH;
			ofn.lpstrTitle = L"��ѡ��һ��Ҫ��ʾ��GIF�ļ�...";
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

			if (!GetOpenFileName(&ofn))
			{
				StringCchPrintfW(g_pszTexcuteFileName, MAX_PATH, _T("%sAssets\\MM.gif"), g_pszAppPath);
			}

			//ֻ�����ļ�������������������Ⱦѭ��������������
			if (!LoadGIF(g_pszTexcuteFileName, g_pIWICFactory.Get(), g_stGIF))
			{
				throw CGRSCOMException(E_FAIL);
			}

			//----------------------------------------------------------------------------------------------------------
			// ����Computer Shader ��Ҫ�ı��� RWTexture2D ��Դ
			DXGI_FORMAT emFmtRWTexture = DXGI_FORMAT_R8G8B8A8_UNORM;
			D3D12_RESOURCE_DESC stRWTextureDesc = {};
			stRWTextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			stRWTextureDesc.MipLevels = 1;
			stRWTextureDesc.Format = emFmtRWTexture;
			stRWTextureDesc.Width = g_stGIF.m_nPixelWidth;
			stRWTextureDesc.Height = g_stGIF.m_nPixelHeight;
			stRWTextureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			stRWTextureDesc.DepthOrArraySize = 1;
			stRWTextureDesc.SampleDesc.Count = 1;
			stRWTextureDesc.SampleDesc.Quality = 0;

			D3D12_HEAP_PROPERTIES stHeapProp = { D3D12_HEAP_TYPE_DEFAULT };

			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateCommittedResource(
				&stHeapProp
				, D3D12_HEAP_FLAG_NONE
				, &stRWTextureDesc
				, D3D12_RESOURCE_STATE_COMMON
				, nullptr
				, IID_PPV_ARGS(&g_pIRWTexture)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(g_pIRWTexture);

		}

		// ����SRV�� (Shader Resource View Heap)
		{
			D3D12_DESCRIPTOR_HEAP_DESC stSRVHeapDesc = {};
			stSRVHeapDesc.NumDescriptors = 2;  //1 Texutre SRV + 1 CBV
			stSRVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			stSRVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateDescriptorHeap(&stSRVHeapDesc, IID_PPV_ARGS(&pISRVHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pISRVHeap);

			// ����SRV������
			D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
			stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			stSRVDesc.Format = stGIFFrame.m_enTextureFormat;
			stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			stSRVDesc.Texture2D.MipLevels = 1;
			// ע��ʹ��RWTexture��Ϊ��Ⱦ�õ����� Ҳ���Ǿ���Computer Shader Ԥ����һ����
			g_pID3D12Device4->CreateShaderResourceView(g_pIRWTexture.Get(), &stSRVDesc, pISRVHeap->GetCPUDescriptorHandleForHeapStart());

			// CBV
			D3D12_CONSTANT_BUFFER_VIEW_DESC stCBVDesc = {};
			stCBVDesc.BufferLocation = pICBVRes->GetGPUVirtualAddress();
			stCBVDesc.SizeInBytes = static_cast<UINT>(szMVPBuffer);

			D3D12_CPU_DESCRIPTOR_HANDLE stSRVHandle = pISRVHeap->GetCPUDescriptorHandleForHeapStart();
			stSRVHandle.ptr += nSRVDescriptorSize;
			g_pID3D12Device4->CreateConstantBufferView(&stCBVDesc, stSRVHandle);


			//-------------------------------------------------------------------------------------------------------------
			// ���� Computer Shader ��Ҫ�� Descriptor Heap
			stSRVHeapDesc.NumDescriptors = 3;  // 1 CBV + 1 Texutre SRV + 1 UAV

			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateDescriptorHeap(&stSRVHeapDesc, IID_PPV_ARGS(&g_pICSSRVHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(g_pICSSRVHeap);


			stCBVDesc.BufferLocation = pICBGIFFrameInfo->GetGPUVirtualAddress();
			stCBVDesc.SizeInBytes = static_cast<UINT>(szGIFFrameInfo);

			stSRVHandle = g_pICSSRVHeap->GetCPUDescriptorHandleForHeapStart();
			// Computer Shader CBV
			g_pID3D12Device4->CreateConstantBufferView(&stCBVDesc, stSRVHandle);

			stSRVHandle.ptr += nSRVDescriptorSize;
			// Computer Shader SRV
			//g_pID3D12Device4->CreateShaderResourceView(pITexture.Get(), &stSRVDesc, stSRVHandle);			

			stSRVHandle.ptr += nSRVDescriptorSize;

			// Computer Shader UAV
			D3D12_UNORDERED_ACCESS_VIEW_DESC stUAVDesc = {};
			stUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			stUAVDesc.Format = g_pIRWTexture->GetDesc().Format;
			stUAVDesc.Texture2D.MipSlice = 0;

			g_pID3D12Device4->CreateUnorderedAccessView(g_pIRWTexture.Get(), nullptr, &stUAVDesc, stSRVHandle);
		}

		// �����Դ���Ͻṹ
		D3D12_RESOURCE_BARRIER stBeginRenderRTResBarrier = {};
		stBeginRenderRTResBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		stBeginRenderRTResBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		stBeginRenderRTResBarrier.Transition.pResource = pIARenderTargets[nCurrentFrameIndex].Get();
		stBeginRenderRTResBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		stBeginRenderRTResBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		stBeginRenderRTResBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		D3D12_RESOURCE_BARRIER stEndRenderRTResBarrier = {};
		stEndRenderRTResBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		stEndRenderRTResBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		stEndRenderRTResBarrier.Transition.pResource = pIARenderTargets[nCurrentFrameIndex].Get();
		stEndRenderRTResBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		stEndRenderRTResBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		stEndRenderRTResBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle = pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
		DWORD dwRet = 0;
		BOOL bExit = FALSE;

		ULONGLONG n64tmFrameStart = ::GetTickCount64();
		ULONGLONG n64tmCurrent = n64tmFrameStart;
		ULONGLONG n64GIFPlayDelay = stGIFFrame.m_nFrameDelay;
		BOOL bReDrawFrame = FALSE;
		//������ת�Ƕ���Ҫ�ı���
		double dModelRotationYAngle = 0.0f;


		GRS_THROW_IF_FAILED(pICSList->Close());
		GRS_THROW_IF_FAILED(pICMDList->Close());
		SetEvent(g_hEventFence);

		//��ʼ���������Ժ�����ʾ����
		ShowWindow(hWnd, nCmdShow);
		UpdateWindow(hWnd);

		// ��ʼ��Ϣѭ�����������в�����Ⱦ
		while (!bExit)
		{
			dwRet = ::MsgWaitForMultipleObjects(1, &g_hEventFence, FALSE, INFINITE, QS_ALLINPUT);
			switch (dwRet - WAIT_OBJECT_0)
			{
			case 0:
			{
				n64tmCurrent = ::GetTickCount();
				//OnUpdate()
				{// OnUpdate()
					//������ת�ĽǶȣ���ת�Ƕ�(����) = ʱ��(��) * ���ٶ�(����/��)
					//�����������൱�ھ�����Ϸ��Ϣѭ���е�OnUpdate��������Ҫ��������
					dModelRotationYAngle += ((n64tmCurrent - n64tmFrameStart) / 1000.0f) * g_fPalstance;
					//��ת�Ƕ���2PI���ڵı�����ȥ����������ֻ�������0���ȿ�ʼ��С��2PI�Ļ��ȼ���
					if (dModelRotationYAngle > XM_2PI)
					{
						dModelRotationYAngle = fmod(dModelRotationYAngle, XM_2PI);
					}

					//ģ�;��� model
					XMMATRIX xmRot = XMMatrixRotationY(static_cast<float>(dModelRotationYAngle));

					//���� ģ�;��� model * �Ӿ��� view
					XMMATRIX xmMVP = XMMatrixMultiply(xmRot, XMMatrixLookAtLH(g_v4EyePos, g_v4LookAt, g_v4UpDir));

					//ͶӰ���� projection
					xmMVP = XMMatrixMultiply(xmMVP, (XMMatrixPerspectiveFovLH(XM_PIDIV4, (FLOAT)iWidth / (FLOAT)iHeight, 0.1f, 1000.0f)));

					XMStoreFloat4x4(&pMVPBuffer->m_MVP, xmMVP);
				}

				//OnComputerShader()
				{
					if (0 == g_stGIF.m_nCurrentFrame || (n64tmCurrent - n64tmFrameStart) >= n64GIFPlayDelay)
					{
						GRS_THROW_IF_FAILED(pICSAlloc->Reset());
						GRS_THROW_IF_FAILED(pICSList->Reset(pICSAlloc.Get(), pIPSOComputer.Get()));

						stGIFFrame.m_pIWICGIFFrameBMP.Reset();
						stGIFFrame.m_pIWICGIFFrame.Reset();
						stGIFFrame.m_nFrameDelay = 0;

						if (!LoadGIFFrame(g_pIWICFactory.Get(), g_stGIF, stGIFFrame))
						{
							throw CGRSCOMException(E_FAIL);
						}

						pITexture.Reset();
						pITextureUpload.Reset();

						if (!UploadGIFFrame(g_pID3D12Device4.Get()
							, pICSList.Get()
							, g_pIWICFactory.Get()
							, stGIFFrame
							, pITexture.GetAddressOf()
							, pITextureUpload.GetAddressOf()))
						{
							throw CGRSCOMException(E_FAIL);
						}

						// ����GIF�ı���ɫ��ע��Shader����ɫֵһ����RGBA��ʽ
						pstGIFFrameInfo->m_c4BkColor = XMFLOAT4(
							GRS_ARGB_R(g_stGIF.m_nBkColor)
							, GRS_ARGB_G(g_stGIF.m_nBkColor)
							, GRS_ARGB_B(g_stGIF.m_nBkColor)
							, GRS_ARGB_A(g_stGIF.m_nBkColor));
						pstGIFFrameInfo->m_nFrame = g_stGIF.m_nCurrentFrame;
						pstGIFFrameInfo->m_nDisposal = stGIFFrame.m_nFrameDisposal;

						pstGIFFrameInfo->m_nLeftTop[0] = stGIFFrame.m_nLeftTop[0];	//Frame Image Left
						pstGIFFrameInfo->m_nLeftTop[1] = stGIFFrame.m_nLeftTop[1];	//Frame Image Top
						pstGIFFrameInfo->m_nFrameWH[0] = stGIFFrame.m_nFrameWH[0];	//Frame Image Width
						pstGIFFrameInfo->m_nFrameWH[1] = stGIFFrame.m_nFrameWH[1];	//Frame Image Height

						D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
						stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
						stSRVDesc.Format = stGIFFrame.m_enTextureFormat;
						stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
						stSRVDesc.Texture2D.MipLevels = 1;

						D3D12_CPU_DESCRIPTOR_HANDLE stSRVHandle = g_pICSSRVHeap->GetCPUDescriptorHandleForHeapStart();
						stSRVHandle.ptr += nSRVDescriptorSize;
						g_pID3D12Device4->CreateShaderResourceView(pITexture.Get(), &stSRVDesc, stSRVHandle);

						n64GIFPlayDelay = stGIFFrame.m_nFrameDelay;

						//���µ���һ֡֡��
						g_stGIF.m_nCurrentFrame = (++g_stGIF.m_nCurrentFrame) % g_stGIF.m_nFrames;

						bReDrawFrame = TRUE;
					}
					else
					{
						if (!SUCCEEDED(ULongLongSub(n64GIFPlayDelay, (n64tmCurrent - n64tmFrameStart), &n64GIFPlayDelay)))
						{// �������ʧ��˵��һ�����Ѿ���ʱ����һ֡��ʱ���ˣ��ǾͿ�ʼ��һѭ��������һ֡
							n64GIFPlayDelay = 0;
						}
						bReDrawFrame = FALSE; //ֻ�����ӳ�ʱ�䣬������
					}

					if (bReDrawFrame)
					{
						// ��ʼ�����������
						D3D12_RESOURCE_BARRIER stRWResBeginBarrier = {};
						stRWResBeginBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
						stRWResBeginBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
						stRWResBeginBarrier.Transition.pResource = g_pIRWTexture.Get();
						stRWResBeginBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
						stRWResBeginBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
						stRWResBeginBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

						pICSList->ResourceBarrier(1, &stRWResBeginBarrier);

						pICSList->SetComputeRootSignature(pIRSComputer.Get());
						pICSList->SetPipelineState(pIPSOComputer.Get());

						ID3D12DescriptorHeap* ppHeaps[] = { g_pICSSRVHeap.Get() };
						pICSList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

						D3D12_GPU_DESCRIPTOR_HANDLE stSRVGPUHandle = g_pICSSRVHeap->GetGPUDescriptorHandleForHeapStart();

						pICSList->SetComputeRootDescriptorTable(0, stSRVGPUHandle);
						stSRVGPUHandle.ptr += nSRVDescriptorSize;
						pICSList->SetComputeRootDescriptorTable(1, stSRVGPUHandle);
						stSRVGPUHandle.ptr += nSRVDescriptorSize;
						pICSList->SetComputeRootDescriptorTable(2, stSRVGPUHandle);

						// Computer!
						// ע�⣺����֡��С����������߳�
						pICSList->Dispatch(stGIFFrame.m_nFrameWH[0], stGIFFrame.m_nFrameWH[1], 1);
						//pICSList->Dispatch(g_stGIF.m_nWidth, g_stGIF.m_nHeight, 1);

						// ��ʼ�����������
						D3D12_RESOURCE_BARRIER stRWResEndBarrier = {};
						stRWResEndBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
						stRWResEndBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
						stRWResEndBarrier.Transition.pResource = g_pIRWTexture.Get();
						stRWResEndBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
						stRWResEndBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
						stRWResEndBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

						pICSList->ResourceBarrier(1, &stRWResEndBarrier);

						GRS_THROW_IF_FAILED(pICSList->Close());
						//ִ�������б�
						ID3D12CommandList* ppComputeCommandLists[] = { pICSList.Get() };
						pICSQueue->ExecuteCommandLists(_countof(ppComputeCommandLists), ppComputeCommandLists);
						UINT64 n64CurrentFenceValue = n64FenceValue;
						GRS_THROW_IF_FAILED(pICSQueue->Signal(pIFence.Get(), n64CurrentFenceValue));
						//3D ��Ⱦ���� �ȴ� Computer ����ִ�н���
						GRS_THROW_IF_FAILED(pICMDQueue->Wait(pIFence.Get(), n64CurrentFenceValue));
						n64FenceValue++;
					}
				}

				//OnRender()
				{
					//��ȡ�µĺ󻺳���ţ���ΪPresent�������ʱ�󻺳����ž͸�����
					nCurrentFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();
					//�����������Resetһ��
					GRS_THROW_IF_FAILED(pICMDAlloc->Reset());
					//Reset�����б�������ָ�������������PSO����
					GRS_THROW_IF_FAILED(pICMDList->Reset(pICMDAlloc.Get(), pIPipelineState.Get()));
					//��ʼ��¼����
					pICMDList->SetGraphicsRootSignature(pIRootSignature.Get());
					ID3D12DescriptorHeap* ppHeaps[] = { pISRVHeap.Get() };
					pICMDList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
					D3D12_GPU_DESCRIPTOR_HANDLE stSRVHandle = pISRVHeap->GetGPUDescriptorHandleForHeapStart();
					pICMDList->SetGraphicsRootDescriptorTable(0, stSRVHandle);
					stSRVHandle.ptr += nSRVDescriptorSize;
					pICMDList->SetGraphicsRootDescriptorTable(1, stSRVHandle);
					pICMDList->RSSetViewports(1, &stViewPort);
					pICMDList->RSSetScissorRects(1, &stScissorRect);

					// ͨ����Դ�����ж��󻺳��Ѿ��л���Ͽ��Կ�ʼ��Ⱦ��
					stBeginRenderRTResBarrier.Transition.pResource = pIARenderTargets[nCurrentFrameIndex].Get();
					pICMDList->ResourceBarrier(1, &stBeginRenderRTResBarrier);

					stRTVHandle = pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
					stRTVHandle.ptr += nCurrentFrameIndex * nRTVDescriptorSize;
					//������ȾĿ��
					pICMDList->OMSetRenderTargets(1, &stRTVHandle, FALSE, nullptr);
					pICMDList->ClearRenderTargetView(stRTVHandle, c4ClearColor, 0, nullptr);

					// ������¼�����������ʼ��һ֡����Ⱦ
					pICMDList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

					pICMDList->IASetVertexBuffers(0, 1, &stVertexBufferView);
					pICMDList->IASetIndexBuffer(&stIndexBufferView);
					//Draw Call������
					pICMDList->DrawInstanced(nVertexCnt, 1, 0, 0);

					//��һ����Դ���ϣ�����ȷ����Ⱦ�Ѿ����������ύ����ȥ��ʾ��
					stEndRenderRTResBarrier.Transition.pResource = pIARenderTargets[nCurrentFrameIndex].Get();
					pICMDList->ResourceBarrier(1, &stEndRenderRTResBarrier);
					//�ر������б�����ȥִ����
					GRS_THROW_IF_FAILED(pICMDList->Close());

					//ִ�������б�
					ID3D12CommandList* ppCommandLists[] = { pICMDList.Get() };
					pICMDQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

					//�ύ����
					GRS_THROW_IF_FAILED(pISwapChain3->Present(1, 0));

					//��ʼͬ��GPU��CPU��ִ�У��ȼ�¼Χ�����ֵ
					const UINT64 n64CurrentFenceValue = n64FenceValue;
					GRS_THROW_IF_FAILED(pICMDQueue->Signal(pIFence.Get(), n64CurrentFenceValue));
					GRS_THROW_IF_FAILED(pICSQueue->Wait(pIFence.Get(), n64CurrentFenceValue))
						GRS_THROW_IF_FAILED(pIFence->SetEventOnCompletion(n64CurrentFenceValue, g_hEventFence));
					n64FenceValue++;
				}

				n64tmFrameStart = n64tmCurrent;
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
	//::CoUninitialize();
	return 0;
}

BOOL LoadGIF(LPCWSTR pszGIFFileName, IWICImagingFactory* pIWICFactory, ST_GRS_GIF& g_stGIF, const WICColor& nDefaultBkColor)
{
	BOOL bRet = TRUE;
	try
	{
		PROPVARIANT					stCOMPropValue = {};
		DWORD						dwBGColor = 0;
		BYTE						bBkColorIndex = 0;
		WICColor					pPaletteColors[256] = {};
		UINT						nColorsCopied = 0;
		IWICPalette* pIWICPalette = NULL;
		IWICMetadataQueryReader* pIGIFMetadate = NULL;

		PropVariantInit(&stCOMPropValue);

		GRS_THROW_IF_FAILED(pIWICFactory->CreateDecoderFromFilename(
			pszGIFFileName,						// �ļ���
			NULL,								// ��ָ����������ʹ��Ĭ��
			GENERIC_READ,						// ����Ȩ��
			WICDecodeMetadataCacheOnDemand,		// ����Ҫ�ͻ������� 
			&g_stGIF.m_pIWICDecoder				// ����������
		));

		GRS_THROW_IF_FAILED(g_stGIF.m_pIWICDecoder->GetFrameCount(&g_stGIF.m_nFrames));
		GRS_THROW_IF_FAILED(g_stGIF.m_pIWICDecoder->GetMetadataQueryReader(&pIGIFMetadate));
		if (
			SUCCEEDED(pIGIFMetadate->GetMetadataByName(L"/logscrdesc/GlobalColorTableFlag", &stCOMPropValue))
			&& VT_BOOL == stCOMPropValue.vt
			&& stCOMPropValue.boolVal
			)
		{// ����б���ɫ�����ȡ����ɫ
			PropVariantClear(&stCOMPropValue);
			if (SUCCEEDED(pIGIFMetadate->GetMetadataByName(L"/logscrdesc/BackgroundColorIndex", &stCOMPropValue)))
			{
				if (VT_UI1 == stCOMPropValue.vt)
				{
					bBkColorIndex = stCOMPropValue.bVal;
				}
				GRS_THROW_IF_FAILED(pIWICFactory->CreatePalette(&pIWICPalette));
				GRS_THROW_IF_FAILED(g_stGIF.m_pIWICDecoder->CopyPalette(pIWICPalette));
				GRS_THROW_IF_FAILED(pIWICPalette->GetColors(ARRAYSIZE(pPaletteColors), pPaletteColors, &nColorsCopied));

				// Check whether background color is outside range 
				if (bBkColorIndex <= nColorsCopied)
				{
					g_stGIF.m_nBkColor = pPaletteColors[bBkColorIndex];
				}
			}
		}
		else
		{
			g_stGIF.m_nBkColor = nDefaultBkColor;
		}

		PropVariantClear(&stCOMPropValue);

		if (
			SUCCEEDED(pIGIFMetadate->GetMetadataByName(L"/logscrdesc/Width", &stCOMPropValue))
			&& VT_UI2 == stCOMPropValue.vt
			)
		{
			g_stGIF.m_nWidth = stCOMPropValue.uiVal;
		}
		PropVariantClear(&stCOMPropValue);

		if (
			SUCCEEDED(pIGIFMetadate->GetMetadataByName(L"/logscrdesc/Height", &stCOMPropValue))
			&& VT_UI2 == stCOMPropValue.vt
			)
		{
			g_stGIF.m_nHeight = stCOMPropValue.uiVal;
		}
		PropVariantClear(&stCOMPropValue);

		if (
			SUCCEEDED(pIGIFMetadate->GetMetadataByName(L"/logscrdesc/PixelAspectRatio", &stCOMPropValue))
			&& VT_UI1 == stCOMPropValue.vt
			)
		{
			UINT uPixelAspRatio = stCOMPropValue.bVal;
			if (uPixelAspRatio != 0)
			{
				FLOAT pixelAspRatio = (uPixelAspRatio + 15.f) / 64.f;
				if (pixelAspRatio > 1.f)
				{
					g_stGIF.m_nPixelWidth = g_stGIF.m_nWidth;
					g_stGIF.m_nPixelHeight = static_cast<UINT>(g_stGIF.m_nHeight / pixelAspRatio);
				}
				else
				{
					g_stGIF.m_nPixelWidth = static_cast<UINT>(g_stGIF.m_nWidth * pixelAspRatio);
					g_stGIF.m_nPixelHeight = g_stGIF.m_nHeight;
				}
			}
			else
			{
				g_stGIF.m_nPixelWidth = g_stGIF.m_nWidth;
				g_stGIF.m_nPixelHeight = g_stGIF.m_nHeight;
			}
		}
		PropVariantClear(&stCOMPropValue);

		if (SUCCEEDED(pIGIFMetadate->GetMetadataByName(L"/appext/application", &stCOMPropValue))
			&& stCOMPropValue.vt == (VT_UI1 | VT_VECTOR)
			&& stCOMPropValue.caub.cElems == 11
			&& (!memcmp(stCOMPropValue.caub.pElems, "NETSCAPE2.0", stCOMPropValue.caub.cElems)
				||
				!memcmp(stCOMPropValue.caub.pElems, "ANIMEXTS1.0", stCOMPropValue.caub.cElems)))
		{
			PropVariantClear(&stCOMPropValue);

			if (
				SUCCEEDED(pIGIFMetadate->GetMetadataByName(L"/appext/data", &stCOMPropValue))
				&& stCOMPropValue.vt == (VT_UI1 | VT_VECTOR)
				&& stCOMPropValue.caub.cElems >= 4
				&& stCOMPropValue.caub.pElems[0] > 0
				&& stCOMPropValue.caub.pElems[1] == 1
				)
			{
				g_stGIF.m_nTotalLoopCount = MAKEWORD(stCOMPropValue.caub.pElems[2], stCOMPropValue.caub.pElems[3]);

				if (g_stGIF.m_nTotalLoopCount != 0)
				{
					g_stGIF.m_bHasLoop = TRUE;
				}
				else
				{
					g_stGIF.m_bHasLoop = FALSE;
				}
			}
		}
	}
	catch (CGRSCOMException& e)
	{
		e;
		bRet = FALSE;
	}
	return bRet;
}

BOOL LoadGIFFrame(IWICImagingFactory* pIWICFactory, ST_GRS_GIF& g_stGIF, ST_GRS_GIF_FRAME& stGIFFrame)
{
	BOOL bRet = TRUE;
	try
	{
		WICPixelFormatGUID  stGuidRawFormat = {};
		WICPixelFormatGUID	stGuidTargetFormat = {};
		ComPtr<IWICFormatConverter>			pIWICConverter;
		ComPtr<IWICMetadataQueryReader>		pIWICMetadata;

		stGIFFrame.m_enTextureFormat = DXGI_FORMAT_UNKNOWN;
		//����ָ����֡
		GRS_THROW_IF_FAILED(g_stGIF.m_pIWICDecoder->GetFrame(g_stGIF.m_nCurrentFrame, &stGIFFrame.m_pIWICGIFFrame));
		//��ȡWICͼƬ��ʽ
		GRS_THROW_IF_FAILED(stGIFFrame.m_pIWICGIFFrame->GetPixelFormat(&stGuidRawFormat));
		//ͨ����һ��ת��֮���ȡDXGI�ĵȼ۸�ʽ
		if (GetTargetPixelFormat(&stGuidRawFormat, &stGuidTargetFormat))
		{
			stGIFFrame.m_enTextureFormat = GetDXGIFormatFromPixelFormat(&stGuidTargetFormat);
		}

		if (DXGI_FORMAT_UNKNOWN == stGIFFrame.m_enTextureFormat)
		{// ��֧�ֵ�ͼƬ��ʽ Ŀǰ�˳����� 
		 // һ�� ��ʵ�ʵ����浱�ж����ṩ�����ʽת�����ߣ�
		 // ͼƬ����Ҫ��ǰת���ã����Բ�����ֲ�֧�ֵ�����
			throw CGRSCOMException(S_FALSE);
		}

		// ��ʽת��
		if (!InlineIsEqualGUID(stGuidRawFormat, stGuidTargetFormat))
		{// ����жϺ���Ҫ�����ԭWIC��ʽ����ֱ����ת��ΪDXGI��ʽ��ͼƬʱ
		 // ������Ҫ���ľ���ת��ͼƬ��ʽΪ�ܹ�ֱ�Ӷ�ӦDXGI��ʽ����ʽ
			//����ͼƬ��ʽת����

			GRS_THROW_IF_FAILED(pIWICFactory->CreateFormatConverter(&pIWICConverter));

			//��ʼ��һ��ͼƬת������ʵ��Ҳ���ǽ�ͼƬ���ݽ����˸�ʽת��
			GRS_THROW_IF_FAILED(pIWICConverter->Initialize(
				stGIFFrame.m_pIWICGIFFrame.Get(),                // ����ԭͼƬ����
				stGuidTargetFormat,						 // ָ����ת����Ŀ���ʽ
				WICBitmapDitherTypeNone,         // ָ��λͼ�Ƿ��е�ɫ�壬�ִ��������λͼ�����õ�ɫ�壬����ΪNone
				NULL,                            // ָ����ɫ��ָ��
				0.f,                             // ָ��Alpha��ֵ
				WICBitmapPaletteTypeCustom       // ��ɫ�����ͣ�ʵ��û��ʹ�ã�����ָ��ΪCustom
			));
			// ����QueryInterface������ö����λͼ����Դ�ӿ�
			GRS_THROW_IF_FAILED(pIWICConverter.As(&stGIFFrame.m_pIWICGIFFrameBMP));
		}
		else
		{
			//ͼƬ���ݸ�ʽ����Ҫת����ֱ�ӻ�ȡ��λͼ����Դ�ӿ�
			GRS_THROW_IF_FAILED(stGIFFrame.m_pIWICGIFFrame.As(&stGIFFrame.m_pIWICGIFFrameBMP));
		}
		// ��ȡGIF����ϸ��Ϣ
		GRS_THROW_IF_FAILED(stGIFFrame.m_pIWICGIFFrame->GetMetadataQueryReader(&pIWICMetadata));

		PROPVARIANT stCOMPropValue = {};
		PropVariantInit(&stCOMPropValue);

		// ��ȡ֡�ӳ�ʱ��
		if (SUCCEEDED(pIWICMetadata->GetMetadataByName(L"/grctlext/Delay", &stCOMPropValue)))
		{
			if (VT_UI2 == stCOMPropValue.vt)
			{
				GRS_THROW_IF_FAILED(UIntMult(stCOMPropValue.uiVal, 10, &stGIFFrame.m_nFrameDelay));
				if (0 == stGIFFrame.m_nFrameDelay)
				{
					stGIFFrame.m_nFrameDelay = 100;
				}
			}
		}
		PropVariantClear(&stCOMPropValue);

		if (SUCCEEDED(pIWICMetadata->GetMetadataByName(L"/imgdesc/Left", &stCOMPropValue)))
		{
			if (VT_UI2 == stCOMPropValue.vt)
			{
				stGIFFrame.m_nLeftTop[0] = static_cast<UINT>(stCOMPropValue.uiVal);
			}
		}
		PropVariantClear(&stCOMPropValue);

		if (SUCCEEDED(pIWICMetadata->GetMetadataByName(L"/imgdesc/Top", &stCOMPropValue)))
		{
			if (VT_UI2 == stCOMPropValue.vt)
			{
				stGIFFrame.m_nLeftTop[1] = static_cast<UINT>(stCOMPropValue.uiVal);
			}
		}
		PropVariantClear(&stCOMPropValue);

		if (SUCCEEDED(pIWICMetadata->GetMetadataByName(L"/imgdesc/Width", &stCOMPropValue)))
		{
			if (VT_UI2 == stCOMPropValue.vt)
			{
				stGIFFrame.m_nFrameWH[0] = static_cast<UINT>(stCOMPropValue.uiVal);
			}
		}
		PropVariantClear(&stCOMPropValue);

		if (SUCCEEDED(pIWICMetadata->GetMetadataByName(L"/imgdesc/Height", &stCOMPropValue)))
		{
			if (VT_UI2 == stCOMPropValue.vt)
			{
				stGIFFrame.m_nFrameWH[1] = static_cast<UINT>(stCOMPropValue.uiVal);
			}
		}
		PropVariantClear(&stCOMPropValue);

		if (SUCCEEDED(pIWICMetadata->GetMetadataByName(L"/grctlext/Disposal", &stCOMPropValue)))
		{
			if (VT_UI1 == stCOMPropValue.vt)
			{
				stGIFFrame.m_nFrameDisposal = stCOMPropValue.bVal;
			}
		}
		else
		{
			stGIFFrame.m_nFrameDisposal = DM_UNDEFINED;
		}

		PropVariantClear(&stCOMPropValue);
	}
	catch (CGRSCOMException& e)
	{//������COM�쳣
		e;
		bRet = FALSE;
	}
	return bRet;

}

BOOL UploadGIFFrame(ID3D12Device4* pID3D12Device4, ID3D12GraphicsCommandList* pICMDList, IWICImagingFactory* pIWICFactory, ST_GRS_GIF_FRAME& stGIFFrame, ID3D12Resource** pIGIFFrame, ID3D12Resource** pIGIFFrameUpload)
{
	BOOL bRet = TRUE;
	try
	{
		UINT				nTextureW = 0;
		UINT				nTextureH = 0;
		UINT				nBPP = 0; // Bit Per Pixel
		UINT				nTextureRowPitch = 0;

		UINT64				n64TextureRowSizes = 0u;
		UINT64				n64UploadBufferSize = 0;
		UINT				nTextureRowNum = 0;
		D3D12_RESOURCE_DESC	stTextureDesc = {};
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT	stTxtLayouts = {};
		VOID* pGIFFrameData = nullptr;
		VOID* pUploadBufData = nullptr;

		//���ͼƬ��С����λ�����أ�
		GRS_THROW_IF_FAILED(stGIFFrame.m_pIWICGIFFrameBMP->GetSize(&nTextureW, &nTextureH));

		if (nTextureW != stGIFFrame.m_nFrameWH[0] || nTextureH != stGIFFrame.m_nFrameWH[1])
		{//�����Խ�������֡�����С��ת����Ļ����С��һ��
			throw CGRSCOMException(S_FALSE);
		}

		//��ȡͼƬ���ص�λ��С��BPP��Bits Per Pixel����Ϣ�����Լ���ͼƬ�����ݵ���ʵ��С����λ���ֽڣ�
		ComPtr<IWICComponentInfo> pIWICmntinfo;
		WICPixelFormatGUID stGuidTargetFormat = {};
		stGIFFrame.m_pIWICGIFFrameBMP->GetPixelFormat(&stGuidTargetFormat);
		GRS_THROW_IF_FAILED(pIWICFactory->CreateComponentInfo(stGuidTargetFormat, pIWICmntinfo.GetAddressOf()));

		WICComponentType type;
		GRS_THROW_IF_FAILED(pIWICmntinfo->GetComponentType(&type));

		if (type != WICPixelFormat)
		{
			throw CGRSCOMException(S_FALSE);
		}

		ComPtr<IWICPixelFormatInfo> pIWICPixelinfo;
		GRS_THROW_IF_FAILED(pIWICmntinfo.As(&pIWICPixelinfo));

		// ���������ڿ��Եõ�BPP�ˣ���Ҳ���ҿ��ıȽ���Ѫ�ĵط���Ϊ��BPP��Ȼ������ô�໷��
		GRS_THROW_IF_FAILED(pIWICPixelinfo->GetBitsPerPixel(&nBPP));

		// ����ͼƬʵ�ʵ��д�С����λ���ֽڣ�������ʹ����һ����ȡ����������A+B-1��/B ��
		// ����������˵��΢���������,ϣ�����Ѿ���������ָ��
		nTextureRowPitch = GRS_UPPER_DIV(nTextureW * nBPP, 8u);

		// ����ͼƬ��Ϣ�����2D������Դ����Ϣ�ṹ��
		stTextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		stTextureDesc.MipLevels = 1;
		stTextureDesc.Format = stGIFFrame.m_enTextureFormat;
		stTextureDesc.Width = nTextureW;
		stTextureDesc.Height = nTextureH;
		stTextureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		stTextureDesc.DepthOrArraySize = 1;
		stTextureDesc.SampleDesc.Count = 1;
		stTextureDesc.SampleDesc.Quality = 0;

		D3D12_HEAP_PROPERTIES stHeapProp = { D3D12_HEAP_TYPE_DEFAULT };

		//����Ĭ�϶��ϵ���Դ��������Texture2D��GPU��Ĭ�϶���Դ�ķ����ٶ�������
		//��Ϊ������Դһ���ǲ��ױ����Դ����������ͨ��ʹ���ϴ��Ѹ��Ƶ�Ĭ�϶���
		//�ڴ�ͳ��D3D11����ǰ��D3D�ӿ��У���Щ���̶�����װ�ˣ�����ֻ��ָ������ʱ������ΪĬ�϶� 
		GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
			&stHeapProp
			, D3D12_HEAP_FLAG_NONE
			, &stTextureDesc				//����ʹ��CD3DX12_RESOURCE_DESC::Tex2D���򻯽ṹ��ĳ�ʼ��
			, D3D12_RESOURCE_STATE_COPY_DEST
			, nullptr
			, IID_PPV_ARGS(pIGIFFrame)));
		GRS_SET_D3D12_DEBUGNAME(*pIGIFFrame);

		//��ȡ�ϴ�����Դ����Ĵ�С������ߴ�ͨ������ʵ��ͼƬ�ĳߴ�
		stTextureDesc = (*pIGIFFrame)->GetDesc();

		// ��������Ǻ����õ�һ����������Buffer�������ݵ��������ߴ����������ݵ�Buffer�������������������Ǻܶ���صĸ�������Ϣ
		// ��ʵ��Ҫ����Ϣ���Ƕ���������˵�еĴ�С�����뷽ʽ����Ϣ
		pID3D12Device4->GetCopyableFootprints(&stTextureDesc
			, 0
			, 1
			, 0
			, &stTxtLayouts
			, &nTextureRowNum
			, &n64TextureRowSizes
			, &n64UploadBufferSize);

		// ���������ϴ��������Դ,ע����������Buffer
		// �ϴ��Ѷ���GPU������˵�����Ǻܲ�ģ�
		// ���Զ��ڼ������������������������
		// ͨ�������ϴ���GPU���ʸ���Ч��Ĭ�϶���
		stHeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC stUploadBufDesc = {};
		stUploadBufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		stUploadBufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		stUploadBufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		stUploadBufDesc.Format = DXGI_FORMAT_UNKNOWN;
		stUploadBufDesc.Width = n64UploadBufferSize;
		stUploadBufDesc.Height = 1;
		stUploadBufDesc.DepthOrArraySize = 1;
		stUploadBufDesc.MipLevels = 1;
		stUploadBufDesc.SampleDesc.Count = 1;
		stUploadBufDesc.SampleDesc.Quality = 0;

		GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
			&stHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&stUploadBufDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(pIGIFFrameUpload)));
		GRS_SET_D3D12_DEBUGNAME(*pIGIFFrameUpload);

		//������Դ�����С������ʵ��ͼƬ���ݴ洢���ڴ��С
		pGIFFrameData = ::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, n64UploadBufferSize);
		if (nullptr == pGIFFrameData)
		{
			throw CGRSCOMException(HRESULT_FROM_WIN32(GetLastError()));
		}

		//��ͼƬ�ж�ȡ������
		GRS_THROW_IF_FAILED(stGIFFrame.m_pIWICGIFFrameBMP->CopyPixels(nullptr
			, nTextureRowPitch
			, static_cast<UINT>(nTextureRowPitch * nTextureH)   //ע���������ͼƬ������ʵ�Ĵ�С�����ֵͨ��С�ڻ���Ĵ�С
			, reinterpret_cast<BYTE*>(pGIFFrameData)));

		//--------------------------------------------------------------------------------------------------------------
		// ��һ��Copy������ڴ渴�Ƶ��ϴ��ѣ������ڴ棩��
		//��Ϊ�ϴ���ʵ�ʾ���CPU�������ݵ�GPU���н�
		//�������ǿ���ʹ����Ϥ��Map����������ӳ�䵽CPU�ڴ��ַ��
		//Ȼ�����ǰ��н����ݸ��Ƶ��ϴ�����
		//��Ҫע�����֮���԰��п�������ΪGPU��Դ���д�С
		//��ʵ��ͼƬ���д�С���в����,���ߵ��ڴ�߽����Ҫ���ǲ�һ����
		GRS_THROW_IF_FAILED((*pIGIFFrameUpload)->Map(0, NULL, reinterpret_cast<void**>(&pUploadBufData)));

		BYTE* pDestSlice = reinterpret_cast<BYTE*>(pUploadBufData) + stTxtLayouts.Offset;
		const BYTE* pSrcSlice = reinterpret_cast<const BYTE*>(pGIFFrameData);
		for (UINT y = 0; y < nTextureRowNum; ++y)
		{
			// ��һ��Copy�����CPU��ɣ������ݴ��ڴ渴�Ƶ��ϴ��ѣ������ڴ棩��
			memcpy(pDestSlice + static_cast<SIZE_T>(stTxtLayouts.Footprint.RowPitch) * y
				, pSrcSlice + static_cast<SIZE_T>(nTextureRowPitch) * y
				, nTextureRowPitch);
		}
		//ȡ��ӳ�� �����ױ��������ÿ֡�ı任��������ݣ�������������Unmap�ˣ�
		//������פ�ڴ�,������������ܣ���Ϊÿ��Map��Unmap�Ǻܺ�ʱ�Ĳ���
		//��Ϊ�������붼��64λϵͳ��Ӧ���ˣ���ַ�ռ����㹻�ģ�������ռ�ò���Ӱ��ʲô
		(*pIGIFFrameUpload)->Unmap(0, NULL);

		//�ͷ�ͼƬ���ݣ���һ���ɾ��ĳ���Ա
		::HeapFree(::GetProcessHeap(), 0, pGIFFrameData);
		//--------------------------------------------------------------------------------------------------------------

		//--------------------------------------------------------------------------------------------------------------
		// �ڶ���Copy�����GPU��Copy Engine��ɣ����ϴ��ѣ������ڴ棩�����������ݵ�Ĭ�϶ѣ��Դ棩��
		D3D12_TEXTURE_COPY_LOCATION stDst = {};
		stDst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		stDst.pResource = *pIGIFFrame;
		stDst.SubresourceIndex = 0;

		D3D12_TEXTURE_COPY_LOCATION stSrc = {};
		stSrc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		stSrc.pResource = *pIGIFFrameUpload;
		stSrc.PlacedFootprint = stTxtLayouts;

		//�ڶ���Copy������GPU����ɣ���Texture���ݴ��ϴ��ѣ������ڴ棩���Ƶ�Ĭ�϶ѣ��Դ棩��
		pICMDList->CopyTextureRegion(&stDst, 0, 0, 0, &stSrc, nullptr);
		//--------------------------------------------------------------------------------------------------------------

		// ����һ����Դ���ϣ�Ĭ�϶��ϵ��������ݸ�����ɺ󣬽������״̬�Ӹ���Ŀ��ת��ΪShader��Դ
		// ������Ҫע���������������У�״̬ת���������ֻ����Pixel Shader���ʣ�������Shaderû������
		D3D12_RESOURCE_BARRIER stResBar = {};
		stResBar.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		stResBar.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		stResBar.Transition.pResource = *pIGIFFrame;
		stResBar.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		stResBar.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
		stResBar.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		pICMDList->ResourceBarrier(1, &stResBar);
	}
	catch (CGRSCOMException& e)
	{
		e;
		bRet = FALSE;
	}
	return bRet;
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

		ppVertex = (ST_GRS_VERTEX*)HeapAlloc(::GetProcessHeap()
			, HEAP_ZERO_MEMORY
			, nVertexCnt * sizeof(ST_GRS_VERTEX));
		ppIndices = (UINT*)HeapAlloc(::GetProcessHeap()
			, HEAP_ZERO_MEMORY
			, nVertexCnt * sizeof(UINT));

		for (UINT i = 0; i < nVertexCnt; i++)
		{
			fin >> ppVertex[i].m_v4Position.x >> ppVertex[i].m_v4Position.y >> ppVertex[i].m_v4Position.z;
			ppVertex[i].m_v4Position.w = 1.0f;
			fin >> ppVertex[i].m_vTxc.x >> ppVertex[i].m_vTxc.y;
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

		if (VK_TAB == n16KeyCode)
		{// ��Tab���л�ѡ�������GIF����ʾ
			// 1����Ϣѭ����·
			if ( WAIT_OBJECT_0 != ::WaitForSingleObject(g_hEventFence, INFINITE) )
			{
				GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
			}

			OPENFILENAME ofn = {};
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = hWnd;
			ofn.lpstrFilter = L"*Gif �ļ�\0*.gif\0";
			ofn.lpstrFile = g_pszTexcuteFileName;
			ofn.nMaxFile = MAX_PATH;
			ofn.lpstrTitle = L"��ѡ��һ��Ҫ��ʾ��GIF�ļ�...";
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

			if (!GetOpenFileName(&ofn))
			{
				StringCchPrintfW(g_pszTexcuteFileName, MAX_PATH, _T("%sAssets\\MM.gif"), g_pszAppPath);
			}
			else
			{
				// ע������������ȫ�ֱ���g_stGIF�����ⲻ���������
				// ԭ����ʹ����MsgWaitForMultipleObjects����֮����Ϣ�������Ⱦʵ���ϱ���֧(��·)��
				// ��������Ϣʱ����Ⱦ�����ò��������������CPU��Ⱦ����ʱ���϶����ᴦ�������Ϣ
				g_stGIF.m_pIWICDecoder.Reset();
				g_stGIF.m_nTotalLoopCount = 0;
				g_stGIF.m_nLoopNumber = 0;
				g_stGIF.m_bHasLoop = 0;
				g_stGIF.m_nFrames = 0;
				g_stGIF.m_nCurrentFrame = 0;
				g_stGIF.m_nBkColor = 0;
				g_stGIF.m_nWidth = 0;
				g_stGIF.m_nHeight = 0;
				g_stGIF.m_nPixelWidth = 0;
				g_stGIF.m_nPixelHeight = 0;
				//ֻ�����ļ�������������������Ⱦѭ��������������
				if (!LoadGIF(g_pszTexcuteFileName, g_pIWICFactory.Get(), g_stGIF))
				{
					throw CGRSCOMException(E_FAIL);
				}

				//���´���һ��RWTexture��Ҳ�������ǵġ����塱 
				D3D12_RESOURCE_DESC stRWTextureDesc = {};
				stRWTextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
				stRWTextureDesc.MipLevels = 1;
				stRWTextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				stRWTextureDesc.Width = g_stGIF.m_nPixelWidth;
				stRWTextureDesc.Height = g_stGIF.m_nPixelHeight;
				stRWTextureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
				stRWTextureDesc.DepthOrArraySize = 1;
				stRWTextureDesc.SampleDesc.Count = 1;
				stRWTextureDesc.SampleDesc.Quality = 0;

				D3D12_HEAP_PROPERTIES stHeapProp = { D3D12_HEAP_TYPE_DEFAULT };

				// ������ܻ��������⣬�������ͷ���RW������Դ��������GPU����ʹ����
				// ��Ȼ��Ϣ�������Ⱦ���̱���·�ˣ�������Ϣ������CPU�����飬����Ⱦ��GPU������
				// ���������ڿ��ǲ��й���Ŷ������᲻����������,
				// ��Ȼ�����İ취���ǵ���WaitForSingleObject�����ȴ�һ��GPU���������ټ���
				// �ұȽ������Ȳ����ˣ����п��ٸĿ�����Ȼ���ܿ���������˵ʲô����ô���Ǳ�ڵ�Bug���Լ����ָ��˰�
				// ---------------------------------------------------------------------------------------------
				// ������֮ǰ��ע�ͣ�����͵������û�뵽�Ҿ������������ˣ���Ȼ����ÿ�ζ��ᷢ������
				// ����ķ�����ʱWaitһ��Fence Event��������Ȼʹ�������ķ���������������ȸ�20ms����
				// ����ȴ�������INFINITE�����û����ȾҲû����������¼������Fence�Ļ�������Զ�������źţ������ȴ�����������һ��

				//DWORD dwRet = WaitForSingleObject(g_hEventFence, 40);

				g_pIRWTexture.Reset();

				GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateCommittedResource(
					&stHeapProp
					, D3D12_HEAP_FLAG_NONE
					, &stRWTextureDesc
					, D3D12_RESOURCE_STATE_COMMON
					, nullptr
					, IID_PPV_ARGS(&g_pIRWTexture)));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(g_pIRWTexture);

				D3D12_CPU_DESCRIPTOR_HANDLE stSRVHandle = g_pICSSRVHeap->GetCPUDescriptorHandleForHeapStart();
				stSRVHandle.ptr += 2 * g_pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				// Computer Shader UAV
				D3D12_UNORDERED_ACCESS_VIEW_DESC stUAVDesc = {};
				stUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
				stUAVDesc.Format = g_pIRWTexture->GetDesc().Format;
				stUAVDesc.Texture2D.MipSlice = 0;

				g_pID3D12Device4->CreateUnorderedAccessView(g_pIRWTexture.Get(), nullptr, &stUAVDesc, stSRVHandle);

				//����Setһ�£�ʵ���Ͼ��������˸ո��Ǹ�Wait�ĸ����ã���Ϊ��������Ϣѭ����Ҳ��Wait���Event Handle������ѭ��״̬��
				//if (WAIT_OBJECT_0 == dwRet)
				//{
					SetEvent(g_hEventFence);
				//}
			}
		}

		if (VK_ADD == n16KeyCode || VK_OEM_PLUS == n16KeyCode)
		{
			//double g_fPalstance = 10.0f * XM_PI / 180.0f;	//������ת�Ľ��ٶȣ���λ������/��
			g_fPalstance += 10 * XM_PI / 180.0f;
			if (g_fPalstance > XM_PI)
			{
				g_fPalstance = XM_PI;
			}
		}

		if (VK_SUBTRACT == n16KeyCode || VK_OEM_MINUS == n16KeyCode)
		{
			g_fPalstance -= 10 * XM_PI / 180.0f;
			if (g_fPalstance < 0.0f)
			{
				g_fPalstance = XM_PI / 180.0f;
			}
		}
	}
	break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

