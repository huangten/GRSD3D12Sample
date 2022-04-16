#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // �� Windows ͷ���ų�����ʹ�õ�����
#include <windows.h>
#include <tchar.h>
#include <commdlg.h>
#include <strsafe.h>
#include <atlbase.h>
#include <atlcoll.h>
#include <atlchecked.h>
#include <atlstr.h>
#include <atlconv.h>
#include <atlexcept.h>

#include <DirectXMath.h>
using namespace DirectX;

#include "../Commons/GRS_Console_Utility.h"

#include <assimp/Importer.hpp>      // �������ڸ�ͷ�ļ��ж���
#include <assimp/scene.h>           // ��ȡ����ģ�����ݶ�����scene��
#include <assimp/postprocess.h>

#pragma comment(lib, "assimp-vc142-mtd.lib")

#define GRS_WND_TITLE L"GRS DirectX12 Assimp Import Data Display Sample"

// �����ļ�ʱԤ����ı�־ define in "postprocess.h"
// ע�������Post Porcess����˼�������Assimp��˵�������ļ��������Ժ�ĺ�������Ⱦ�ĺ���û�а�ëǮ��ϵ
// aiProcess_LimitBoneWeights
// aiProcess_OptimizeMeshes
// aiProcess_MakeLeftHanded
// aiProcess_ConvertToLeftHanded
// aiProcess_MakeLeftHanded
//#define ASSIMP_LOAD_FLAGS aiProcess_Triangulate\
// | aiProcess_GenSmoothNormals\
// | aiProcess_FlipUVs\
// | aiProcess_JoinIdenticalVertices\
// | aiProcess_ConvertToLeftHanded\
// | aiProcess_GenBoundingBoxes\
// | aiProcess_LimitBoneWeights

#define ASSIMP_LOAD_FLAGS 0

VOID GRSPrintfMX( const aiMatrix4x4& aiMatrix )
{
    GRSPrintfA(
        "\n%11.6f, %11.6f, %11.6f, %11.6f;\n\
%11.6f, %11.6f, %11.6f, %11.6f;\n\
%11.6f, %11.6f, %11.6f, %11.6f;\n\
%11.6f, %11.6f, %11.6f, %11.6f;\n"
        , aiMatrix.a1, aiMatrix.a2, aiMatrix.a3, aiMatrix.a4
        , aiMatrix.b1, aiMatrix.b2, aiMatrix.b3, aiMatrix.b4
        , aiMatrix.c1, aiMatrix.c2, aiMatrix.c3, aiMatrix.c4
        , aiMatrix.d1, aiMatrix.d2, aiMatrix.d3, aiMatrix.d4
    );
}

VOID GRSPrintfMX( const aiMatrix4x4& aiMatrix, int iBlank )
{
    GRSPrintBlank( iBlank );
    GRSPrintfA(
        "%11.6f, %11.6f, %11.6f, %11.6f;\n"
        , aiMatrix.a1, aiMatrix.a2, aiMatrix.a3, aiMatrix.a4
    );
    GRSPrintBlank( iBlank );
    GRSPrintfA(
        "%11.6f, %11.6f, %11.6f, %11.6f;\n"
        , aiMatrix.b1, aiMatrix.b2, aiMatrix.b3, aiMatrix.b4
    );
    GRSPrintBlank( iBlank );
    GRSPrintfA(
        "%11.6f, %11.6f, %11.6f, %11.6f;\n"
        , aiMatrix.c1, aiMatrix.c2, aiMatrix.c3, aiMatrix.c4
    );
    GRSPrintBlank( iBlank );
    GRSPrintfA(
        "%11.6f, %11.6f, %11.6f, %11.6f;\n"
        , aiMatrix.d1, aiMatrix.d2, aiMatrix.d3, aiMatrix.d4
    );
}

void ShowSceneFlags( const unsigned int& nFlags );
void ShowMesh( const UINT& nIndex, const aiMesh*& paiMesh );
void ShowMaterial( const UINT& nIndex, const aiMaterial*& paiMaterial );
void ShowAnimations( const aiScene* pModel );
void ShowChannleInfo( const aiNodeAnim* pChannle );
void TraverseNodeTree( const aiNode* pNode, int iBlank );
void ShowTextures( const UINT& nIndex, const aiTexture* paiTexture );
void ShowLights( const UINT& nIndex, const aiLight* paiLight );
void ShowCamera( const UINT& nIndex, const aiCamera* paiCamera );
void ShowMetadata( const aiMetadata* paiMetaData );

int APIENTRY _tWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR    lpCmdLine, int nCmdShow )
{
    try
    {
        GRSInitConsole( GRS_WND_TITLE );
        USES_CONVERSION;
        //���������д��ڴ�С �����ʾ
        GRSSetConsoleMax();

        WCHAR pszAppPath[MAX_PATH] = {};
        // �õ���ǰ�Ĺ���Ŀ¼����������ʹ�����·�������ʸ�����Դ�ļ�
        {
            if ( 0 == ::GetModuleFileNameW( nullptr, pszAppPath, MAX_PATH ) )
            {
                GRSPrintfW( L"��ȡ��ǰ·��(GetModuleFileNameA)����ʧ��(%U)!\n", GetLastError() );
                AtlThrowLastWin32();
            }

            WCHAR* lastSlash = wcsrchr( pszAppPath, L'\\' );
            if ( lastSlash )
            {//ɾ��Exe�ļ���
                *( lastSlash ) = L'\0';
            }

            lastSlash = wcsrchr( pszAppPath, L'\\' );
            if ( lastSlash )
            {//ɾ��x64·��
                *( lastSlash ) = L'\0';
            }

            lastSlash = wcsrchr( pszAppPath, L'\\' );
            if ( lastSlash )
            {//ɾ��Debug �� Release·��
                *( lastSlash + 1 ) = L'\0';
            }
        }

        CHAR pszAppPathA[MAX_PATH] = {};
        StringCchCopyA( pszAppPathA, MAX_PATH, T2A( pszAppPath ) );
        GRSPrintfA( "��ǰ�������л���·��Ϊ��\"%s\"\n", pszAppPathA );

        CHAR pXFile[MAX_PATH] = {};
        StringCchPrintfA( pXFile, MAX_PATH, "%Assets\\The3DModel\\hero.x", pszAppPathA );

        OPENFILENAMEA ofn;
        RtlZeroMemory( &ofn, sizeof( ofn ) );
        ofn.lStructSize = sizeof( ofn );
        ofn.hwndOwner = ::GetConsoleWindow();
        ofn.lpstrFilter = "x �ļ�\0*.x;*.X\0OBJ�ļ�\0*.obj\0FBX�ļ�\0*.fbx\0�����ļ�\0*.*\0";
        ofn.lpstrFile = pXFile;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = "��ѡ��һ��x�ļ�...";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

        if ( !GetOpenFileNameA( &ofn ) )
        {
            StringCchPrintfA( pXFile, MAX_PATH, "%Assets\\The3DModel\\hero.x", pszAppPathA );
        }

        Assimp::Importer aiImporter;
        // ��ȡx�ļ�
        const aiScene* pModel = aiImporter.ReadFile( pXFile, ASSIMP_LOAD_FLAGS );
        if ( nullptr == pModel )
        {
            GRSPrintfA( "�޷������ļ�(%s)��%s (%d)\n", pXFile, aiImporter.GetErrorString(), ::GetLastError() );
            AtlThrowLastWin32();
        }

        GRSPrintfA( "\n��ʼ����ģ���ļ�:\"%s\"", pXFile );
        GRSPrintfW( L"\nAssimp File Flags:" );
        ShowSceneFlags( pModel->mFlags );

        if ( pModel->mMetaData )
        {
            GRSPrintfW( L"\nMetaData��\n" );
            GRSConsolePause();
            GRSPrintfW( L"\n" );
            ShowMetadata( pModel->mMetaData );
        }
        else
        {
            GRSPrintfW( L"��ģ����û��MetaData��Ϣ��\n" );
        }

        if ( pModel->HasMeshes() )
        {
            GRSPrintfW( L"\n����\nMesh Count[%u]:\n", pModel->mNumMeshes );
            GRSConsolePause();
            // ����
            const aiMesh* paiSubMesh = nullptr;
            for ( UINT i = 0; i < pModel->mNumMeshes; i++ )
            {
                paiSubMesh = pModel->mMeshes[i];
                ShowMesh( i + 1, paiSubMesh );
                GRSConsolePause();
            }
        }
        else
        {
            GRSPrintfW( L"��ģ����û��������Ϣ��\n" );
        }

        if ( pModel->HasMaterials() )
        {
            GRSPrintfW( L"\n���ʣ�\n" );
            GRSConsolePause();
            GRSPrintfW( L"\n" );

            // ����
            for ( UINT i = 0; i < pModel->mNumMaterials; i++ )
            {
                const aiMaterial* paiMaterial = pModel->mMaterials[i];
                ShowMaterial( i, paiMaterial );
            }
        }
        else
        {
            GRSPrintfW( L"��ģ����û�в�����Ϣ��\n" );
        }

        if ( nullptr != pModel->mRootNode )
        {
            GRSPrintfW( L"\n������\n" );
            GRSConsolePause();
            GRSPrintfW( L"\n" );
            // ����
            TraverseNodeTree( pModel->mRootNode, 0 );
        }
        else
        {
            GRSPrintfW( L"��ģ����û�й�����Ϣ��\n" );
        }

        if ( pModel->HasAnimations() )
        {
            GRSPrintfW( L"\n�ؼ�֡������\n" );
            GRSConsolePause();
            GRSPrintfW( L"\n" );
            // ��������
            ShowAnimations( pModel );
        }
        else
        {
            GRSPrintfW( L"��ģ����û�йؼ�֡������Ϣ��\n" );
        }

        if ( pModel->HasTextures() )
        {
            GRSPrintfW( L"\n��Ƕ����\n" );
            GRSConsolePause();
            GRSPrintfW( L"\n" );
            // ��Ƕ����
            aiTexture* paiTexture = nullptr;
            for ( UINT i = 0; i < pModel->mNumTextures; i++ )
            {
                paiTexture = pModel->mTextures[i];
                ShowTextures( i, paiTexture );
            }
        }
        else
        {
            GRSPrintfW( L"��ģ����û����Ƕ������Ϣ��\n" );
        }

        // ��Դ
        if ( pModel->HasLights() )
        {
            GRSPrintfW( L"\n��Դ��\n" );
            GRSConsolePause();
            GRSPrintfW( L"\n" );
            for ( UINT i = 0; i < pModel->mNumLights; i++ )
            {
                aiLight* paiLight = pModel->mLights[i];
                ShowLights( i, paiLight );
            }
        }
        else
        {
            GRSPrintfW( L"��ģ����û�й�Դ��Ϣ��\n" );
        }


        if ( pModel->HasCameras() )
        {
            GRSPrintfW( L"\n�����\n" );
            GRSConsolePause();
            GRSPrintfW( L"\n" );
            // ���
            for ( UINT i = 0; i < pModel->mNumCameras; i++ )
            {
                aiCamera* parCamera = pModel->mCameras[i];
                ShowCamera( i, parCamera );
            }
        }
        else
        {
            GRSPrintfW( L"��ģ����û���������Ϣ��\n" );
        }
        GRSPrintfW( L"������ʾ��ϣ�\n\n" );
    }
    catch ( CAtlException& e )
    {//������COM�쳣
        e;
    }
    catch ( ... )
    {

    }
    GRSUninitConsole();
    return 0;
}

#define GRS_NAME_ANONYMITY "����"
#define GRS_SHOW_VECTOR_1D_FORMAT "(%11.6f)"
#define GRS_SHOW_VECTOR_2D_FORMAT "(%11.6f,%11.6f)"
#define GRS_SHOW_VECTOR_3D_FORMAT "(%11.6f,%11.6f,%11.6f)"
#define GRS_SHOW_VECTOR_4D_FORMAT "(%11.6f,%11.6f,%11.6f,%11.6f)"
#define GRS_SHOW_VECTOR_COLS 4

void ShowSceneFlags( const unsigned int& nFlags )
{
    if ( nFlags & AI_SCENE_FLAGS_INCOMPLETE )
    {
        GRSPrintfW( L"AI_SCENE_FLAGS_INCOMPLETE " );
    }

    if ( nFlags & AI_SCENE_FLAGS_VALIDATED )
    {
        GRSPrintfW( L"AI_SCENE_FLAGS_VALIDATED " );
    }

    if ( nFlags & AI_SCENE_FLAGS_VALIDATION_WARNING )
    {
        GRSPrintfW( L"AI_SCENE_FLAGS_VALIDATION_WARNING " );
    }

    if ( nFlags & AI_SCENE_FLAGS_NON_VERBOSE_FORMAT )
    {
        GRSPrintfW( L"AI_SCENE_FLAGS_NON_VERBOSE_FORMAT " );
    }

    if ( nFlags & AI_SCENE_FLAGS_TERRAIN )
    {
        GRSPrintfW( L"AI_SCENE_FLAGS_TERRAIN " );
    }

    if ( nFlags & AI_SCENE_FLAGS_ALLOW_SHARED )
    {
        GRSPrintfW( L"AI_SCENE_FLAGS_ALLOW_SHARED " );
    }
}

void ShowMeshPrimitive( const unsigned int& emPT )
{
    if ( emPT & aiPrimitiveType_POINT )
    {
        GRSPrintfA( "aiPrimitiveType_POINT " );
    }
    if ( emPT & aiPrimitiveType_LINE )
    {
        GRSPrintfA( "aiPrimitiveType_LINE " );
    }
    if ( emPT & aiPrimitiveType_TRIANGLE )
    {
        GRSPrintfA( "aiPrimitiveType_TRIANGLE " );
    }
    if ( emPT & aiPrimitiveType_TRIANGLE )
    {
        GRSPrintfA( "aiPrimitiveType_POLYGON " );
    }
}

void ShowMesh( const UINT& nIndex, const aiMesh*& paiMesh )
{
    GRSPrintfA( "\nMesh[%u]:\"%s\", Primitive: <"
        , nIndex
        , paiMesh->mName.length > 0 ? paiMesh->mName.C_Str() : GRS_NAME_ANONYMITY
    );

    ShowMeshPrimitive( paiMesh->mPrimitiveTypes );
    GRSPrintfW( L">, ���� Material Index:[%u]:\n", paiMesh->mMaterialIndex );

    GRSPrintfW( L"AABB:Min" GRS_SHOW_VECTOR_3D_FORMAT L",Max" GRS_SHOW_VECTOR_3D_FORMAT L"\n\n"
        , paiMesh->mAABB.mMin.x
        , paiMesh->mAABB.mMin.y
        , paiMesh->mAABB.mMin.z
        , paiMesh->mAABB.mMax.x
        , paiMesh->mAABB.mMax.y
        , paiMesh->mAABB.mMax.z
    );

    if ( paiMesh->mNumVertices > 0 )
    {
        GRSPrintfW( L"Vertex Count[%u]:\n", paiMesh->mNumVertices );
    }

    const UINT nCol = GRS_SHOW_VECTOR_COLS;
    if ( paiMesh->HasPositions() )
    {
        GRSPrintfW( L"Positions[%u]:\n", paiMesh->mNumVertices );
        for ( UINT i = 0; i < paiMesh->mNumVertices; i++ )
        {
            GRSPrintfW( L"[%4u]" GRS_SHOW_VECTOR_3D_FORMAT L", "
                , i
                , paiMesh->mVertices[i].x
                , paiMesh->mVertices[i].y
                , paiMesh->mVertices[i].z
            );

            if ( ( ( i + 1 ) % nCol ) == 0 )
            {
                GRSPrintfW( L"\n" );
            }
        }
        GRSPrintfW( L"\n" );
    }

    if ( paiMesh->HasNormals() )
    {
        GRSPrintfW( L"\nNormals[%u]:\n", paiMesh->mNumVertices );
        for ( UINT i = 0; i < paiMesh->mNumVertices; i++ )
        {
            GRSPrintfW( L"[%4u]" GRS_SHOW_VECTOR_3D_FORMAT L", "
                , i
                , paiMesh->mNormals[i].x
                , paiMesh->mNormals[i].y
                , paiMesh->mNormals[i].z );

            if ( ( ( i + 1 ) % nCol ) == 0 )
            {
                GRSPrintfW( L"\n" );
            }
        }
        GRSPrintfW( L"\n" );
    }

    if ( paiMesh->HasTangentsAndBitangents() )
    {
        GRSPrintfW( L"\nTangents[%u]:\n", paiMesh->mNumVertices );

        for ( UINT i = 0; i < paiMesh->mNumVertices; i++ )
        {
            GRSPrintfW( L"[%4u]" GRS_SHOW_VECTOR_3D_FORMAT L", "
                , i
                , paiMesh->mTangents[i].x
                , paiMesh->mTangents[i].y
                , paiMesh->mTangents[i].z );

            if ( ( ( i + 1 ) % nCol ) == 0 )
            {
                GRSPrintfA( "\n" );
            }
        }

        GRSPrintfW( L"\n" );

        GRSPrintfW( L"\nBitangents[%u]:\n", paiMesh->mNumVertices );

        for ( UINT i = 0; i < paiMesh->mNumVertices; i++ )
        {
            GRSPrintfW( L"[%4u]" GRS_SHOW_VECTOR_3D_FORMAT L", "
                , i
                , paiMesh->mBitangents[i].x
                , paiMesh->mBitangents[i].y
                , paiMesh->mBitangents[i].z );

            if ( ( ( i + 1 ) % nCol ) == 0 )
            {
                GRSPrintfW( L"\n" );
            }
        }
        GRSPrintfW( L"\n" );
    }

    UINT nColorChannels = 0;
    for ( UINT nChannel = 0; nChannel < AI_MAX_NUMBER_OF_COLOR_SETS; nChannel++ )
    {
        if ( !paiMesh->HasVertexColors( nChannel ) )
        {
            continue;
        }

        ++nColorChannels;

        GRSPrintfW( L"\nColors Channel[%u]|Colors Count[%u]:):\n", nChannel, paiMesh->mNumVertices );

        for ( UINT i = 0; i < paiMesh->mNumVertices; i++ )
        {
            GRSPrintfW( L"[%4u]" GRS_SHOW_VECTOR_4D_FORMAT L", "
                , i
                , paiMesh->mColors[nChannel][i].r
                , paiMesh->mColors[nChannel][i].g
                , paiMesh->mColors[nChannel][i].b
                , paiMesh->mColors[nChannel][i].a );

            if ( ( ( i + 1 ) % nCol ) == 0 )
            {
                GRSPrintfW( L"\n" );
            }
        }

        GRSPrintfW( L"\n" );
    }

    nColorChannels = 0;
    for ( UINT nChannel = 0; nChannel < AI_MAX_NUMBER_OF_COLOR_SETS; nChannel++ )
    {
        if ( !paiMesh->HasTextureCoords( nChannel ) )
        {
            continue;
        }

        ++nColorChannels;

        GRSPrintfW( L"\nTexture Coords Channel:[%u]|Coords Count[%u] UV Components[%u]:\n"
            , nChannel
            , paiMesh->mNumVertices
            , paiMesh->mNumUVComponents[nChannel] );

        if ( 2 == paiMesh->mNumUVComponents[nChannel] )
        {// (u,v)
            for ( UINT i = 0; i < paiMesh->mNumVertices; i++ )
            {
                GRSPrintfW( L"[%4u]" GRS_SHOW_VECTOR_2D_FORMAT L", "
                    , i
                    , paiMesh->mTextureCoords[nChannel][i].x
                    , paiMesh->mTextureCoords[nChannel][i].y );

                if ( ( ( i + 1 ) % nCol ) == 0 )
                {
                    GRSPrintfW( L"\n" );
                }
            }
        }
        else
        {// (u,v,w)
            for ( UINT i = 0; i < paiMesh->mNumVertices; i++ )
            {
                GRSPrintfW( L"[%4u]" GRS_SHOW_VECTOR_3D_FORMAT L", "
                    , i
                    , paiMesh->mTextureCoords[nChannel][i].x
                    , paiMesh->mTextureCoords[nChannel][i].y
                    , paiMesh->mTextureCoords[nChannel][i].z );

                if ( ( ( i + 1 ) % nCol ) == 0 )
                {
                    GRSPrintfW( L"\n" );
                }
            }
        }

        GRSPrintfW( L"\n" );
    }

    if ( paiMesh->HasFaces() )
    {
        GRSPrintfW( L"\nFaces Count[%u]:\n", paiMesh->mNumFaces );
        for ( UINT i = 0; i < paiMesh->mNumFaces; i++ )
        {
            GRSPrintfW( L"Face[%4u]:{<%u>("
                , i
                , paiMesh->mFaces[i].mNumIndices );

            if ( paiMesh->mFaces[i].mNumIndices > 0 )
            {
                GRSPrintfW( L"%4u", paiMesh->mFaces[i].mIndices[0] );
            }

            for ( UINT j = 1; j < paiMesh->mFaces[i].mNumIndices; j++ )
            {
                GRSPrintfW( L",%4u", paiMesh->mFaces[i].mIndices[j] );
            }

            GRSPrintfW( L")}, " );

            if ( ( ( i + 1 ) % nCol ) == 0 )
            {
                GRSPrintfW( L"\n" );
            }
        }

        GRSPrintfW( L"\n" );
    }

    if ( paiMesh->HasBones() )
    {
        UINT nCol =  GRS_SHOW_VECTOR_COLS + 4;

        GRSPrintfW( L"\nBones[%u]:\n", paiMesh->mNumBones );
        for ( UINT i = 0; i < paiMesh->mNumBones; i++ )
        {
            aiBone* paiBone = paiMesh->mBones[i];
            GRSPrintfA( "\nBone[%u]:\"%s\" - ���λ�˾���(Inverse bind pose matrix):\n"
                , i
                , paiBone->mName.C_Str()
            );

            GRSPrintfMX( paiBone->mOffsetMatrix );

            GRSPrintfW( L"\nWeights[%u]:\n", paiBone->mNumWeights );

            for ( UINT j = 0; j < paiBone->mNumWeights; j++ )
            {
                GRSPrintfW( L"[%4u](%4u,%11.6f), "
                    , j
                    , paiBone->mWeights[j].mVertexId
                    , paiBone->mWeights[j].mWeight );

                if ( ( ( j + 1 ) % nCol ) == 0 )
                {
                    GRSPrintfW( L"\n" );
                }
            }
            GRSPrintfW( L"\n\n" );
        }
        GRSPrintfW( L"\n" );
    }

    //if ( paiMesh->mNumAnimMeshes > 0 )
    //{// ���񶯻�����������Ͳ�ϸ���ˣ������Ͼ��ǽ������������滻
    //	paiMesh->mMethod
    //}
}

void ShowTextureType( const aiTextureType& emTextureType )
{
    switch ( emTextureType )
    {
    case aiTextureType_NONE:
    {
        GRSPrintfW( L"aiTextureType_NONE (Not Texture)" );
    }
    break;
    case aiTextureType_DIFFUSE:
    {
        GRSPrintfW( L"aiTextureType_DIFFUSE" );
    }
    break;
    case aiTextureType_SPECULAR:
    {
        GRSPrintfW( L"aiTextureType_SPECULAR" );
    }
    break;
    case aiTextureType_AMBIENT:
    {
        GRSPrintfW( L"aiTextureType_AMBIENT" );
    }
    break;
    case aiTextureType_EMISSIVE:
    {
        GRSPrintfW( L"aiTextureType_EMISSIVE" );
    }
    break;
    case aiTextureType_HEIGHT:
    {
        GRSPrintfW( L"aiTextureType_HEIGHT" );
    }
    break;
    case aiTextureType_NORMALS:
    {
        GRSPrintfW( L"aiTextureType_NORMALS" );
    }
    break;
    case aiTextureType_SHININESS:
    {
        GRSPrintfW( L"aiTextureType_SHININESS" );
    }
    break;
    case aiTextureType_OPACITY:
    {
        GRSPrintfW( L"aiTextureType_OPACITY" );
    }
    break;
    case aiTextureType_DISPLACEMENT:
    {
        GRSPrintfW( L"aiTextureType_DISPLACEMENT" );
    }
    break;
    case aiTextureType_LIGHTMAP:
    {
        GRSPrintfW( L"aiTextureType_LIGHTMAP" );
    }
    break;
    case aiTextureType_REFLECTION:
    {
        GRSPrintfW( L"aiTextureType_REFLECTION" );
    }
    break;

    case aiTextureType_BASE_COLOR:
    {
        GRSPrintfW( L"aiTextureType_BASE_COLOR (PBR Materials)" );
    }
    break;
    case aiTextureType_NORMAL_CAMERA:
    {
        GRSPrintfW( L"aiTextureType_NORMAL_CAMERA (PBR Materials)" );
    }
    break;
    case aiTextureType_EMISSION_COLOR:
    {
        GRSPrintfW( L"aiTextureType_EMISSION_COLOR (PBR Materials)" );
    }
    break;
    case aiTextureType_METALNESS:
    {
        GRSPrintfW( L"aiTextureType_METALNESS (PBR Materials)" );
    }
    break;
    case aiTextureType_DIFFUSE_ROUGHNESS:
    {
        GRSPrintfW( L"aiTextureType_DIFFUSE_ROUGHNESS (PBR Materials)" );
    }
    break;
    case aiTextureType_AMBIENT_OCCLUSION:
    {
        GRSPrintfW( L"aiTextureType_AMBIENT_OCCLUSION (PBR Materials)" );
    }
    break;
    case aiTextureType_UNKNOWN:
    {
        GRSPrintfW( L"aiTextureType_UNKNOWN (PBR Materials)" );
    }
    break;
    default:
        break;
    }
}

void ShowMaterial( const UINT& nIndex, const aiMaterial*& paiMaterial )
{
    const aiString strMaterialName = ( (aiMaterial*) paiMaterial )->GetName(); // ע����һ�е�ǿ������ת����^~^

    GRSPrintfA( "\nMaterial[%u]:\"%s\", Property[%u]��\n"
        , nIndex
        , strMaterialName.length > 0 ? strMaterialName.C_Str() : GRS_NAME_ANONYMITY
        , paiMaterial->mNumProperties
    );

    if ( paiMaterial->mNumProperties > 0 )
    {
        aiMaterialProperty* paiMaterialProperty = nullptr;
        for ( UINT i = 0; i < paiMaterial->mNumProperties; i++ )
        {
            paiMaterialProperty = paiMaterial->mProperties[i];
            GRSPrintfA( "Material Property[%4u]: <Key=\"%16s\">: "
                , i
                , paiMaterialProperty->mKey.C_Str()
            );
            aiTextureType emType = (aiTextureType) paiMaterialProperty->mSemantic;
            ShowTextureType( emType );

            if ( aiTextureType_NONE == paiMaterialProperty->mSemantic )
            {// ������������
                USES_CONVERSION;
                GRSPrintfW( L" Property Type: " );
                switch ( paiMaterialProperty->mType )
                {
                case aiPTI_Float:
                {
                    GRSPrintfW( L"aiPTI_Float: " GRS_SHOW_VECTOR_1D_FORMAT, *(float*) paiMaterialProperty->mData );
                }
                break;
                case aiPTI_Double:
                {
                    GRSPrintfW( L"aiPTI_Double: " GRS_SHOW_VECTOR_1D_FORMAT, *(float*) paiMaterialProperty->mData );
                }
                break;
                case aiPTI_String:
                {
                    GRSPrintfW( L"aiPTI_String[%u]: \"%s\""
                        , paiMaterialProperty->mDataLength
                        , A2W( paiMaterialProperty->mData + sizeof( int ) )
                    );
                }
                break;
                case aiPTI_Integer:
                {
                    GRSPrintfW( L"aiPTI_Integer: %d", *(int*) paiMaterialProperty->mData );
                }
                break;
                case aiPTI_Buffer:
                {
                    GRSPrintfW( L"aiPTI_Buffer[%u]:[0x016X]"
                        , paiMaterialProperty->mDataLength
                        , paiMaterialProperty->mData );
                }
                break;
                default:
                    break;
                }

            }
            else
            {// ����������
                aiString aistrPath;
                aiTextureMapping	mapping = aiTextureMapping_UV;
                unsigned int		uvindex = 0;
                ai_real				blend = 0.0f;
                aiTextureOp			op = aiTextureOp_Multiply;
                aiTextureMapMode	mapmode = aiTextureMapMode_Wrap;

                if ( AI_SUCCESS == paiMaterial->GetTexture( emType
                    , paiMaterialProperty->mIndex
                    , &aistrPath
                    , &mapping
                    , &uvindex
                    , &blend
                    , &op
                    , &mapmode
                ) )
                {
                    GRSPrintfA( ": \"%s\", Mapping mode:", aistrPath.C_Str() );

                    switch ( mapping )
                    {
                    case aiTextureMapping_UV:
                    {
                        GRSPrintfW( L"aiTextureMapping_UV" );
                    }
                    break;
                    case aiTextureMapping_SPHERE:
                    {
                        GRSPrintfW( L"aiTextureMapping_SPHERE" );
                    }
                    break;
                    case aiTextureMapping_CYLINDER:
                    {
                        GRSPrintfW( L"aiTextureMapping_CYLINDER" );
                    }
                    break;
                    case aiTextureMapping_BOX:
                    {
                        GRSPrintfW( L"aiTextureMapping_BOX" );
                    }
                    break;
                    case aiTextureMapping_PLANE:
                    {
                        GRSPrintfW( L"aiTextureMapping_PLANE" );
                    }
                    break;
                    case aiTextureMapping_OTHER:
                    {
                        GRSPrintfW( L"aiTextureMapping_OTHER" );
                    }
                    break;

                    default:
                        break;
                    }


                    GRSPrintfW( L", UVIndex[%u], Blend" GRS_SHOW_VECTOR_1D_FORMAT, uvindex, blend );

                    GRSPrintfW( L", Alpha OP: " );

                    switch ( op )
                    {
                    case aiTextureOp_Multiply:
                    {
                        GRSPrintfW( L"aiTextureOp_Multiply (T = T1 * T2)" );
                    }
                    break;
                    case aiTextureOp_Add:
                    {
                        GRSPrintfW( L"aiTextureOp_Add (T = T1 + T2)" );
                    }
                    break;
                    case aiTextureOp_Subtract:
                    {
                        GRSPrintfW( L"aiTextureOp_Subtract (T = T1 - T2)" );
                    }
                    break;
                    case aiTextureOp_Divide:
                    {
                        GRSPrintfW( L"aiTextureOp_Divide (T = T1 / T2)" );
                    }
                    break;
                    case aiTextureOp_SmoothAdd:
                    {
                        GRSPrintfW( L"aiTextureOp_SmoothAdd ( T = (T1 + T2) - (T1 * T2) )" );
                    }
                    break;
                    case aiTextureOp_SignedAdd:
                    {
                        GRSPrintfW( L"aiTextureOp_SignedAdd ( T = T1 + (T2-0.5) )" );
                    }
                    break;
                    default:
                        break;
                    }

                    GRSPrintfW( L", Outside Process: " );
                    switch ( mapmode )
                    {
                    case aiTextureMapMode_Wrap:
                    {
                        GRSPrintfW( L"aiTextureMapMode_Wrap" );
                    }
                    break;
                    case aiTextureMapMode_Clamp:
                    {
                        GRSPrintfW( L"aiTextureMapMode_Clamp" );
                    }
                    break;
                    case aiTextureMapMode_Decal:
                    {
                        GRSPrintfW( L"aiTextureMapMode_Decal" );
                    }
                    break;
                    case aiTextureMapMode_Mirror:
                    {
                        GRSPrintfW( L"aiTextureMapMode_Mirror" );
                    }
                    break;
                    default:
                        ATLASSERT( FALSE );
                        break;
                    }
                }

            }

            GRSPrintfW( L";\n" );
        }
    }

}

void ShowAnimationsBehaviour( const aiAnimBehaviour& emBehaviour )
{// ��ʾ����֡���ֵ����Ϊ
    switch ( emBehaviour )
    {
    case aiAnimBehaviour_DEFAULT:
    {/** ȡĬ�Ͻڵ�ת����ֵ */
        GRSPrintfA( "aiAnimBehaviour_DEFAULT" );
    }
    break;
    case aiAnimBehaviour_CONSTANT:
    {/** ʹ����ӽ��ļ�ֵ����ʹ�ò�ֵ */
        GRSPrintfA( "aiAnimBehaviour_CONSTANT" );
    }
    break;
    case aiAnimBehaviour_LINEAR:
    {/** �������������ֵ����������Ϊ��ǰʱ��ֵ��*/
        GRSPrintfA( "aiAnimBehaviour_LINEAR" );
    }
    break;
    case aiAnimBehaviour_REPEAT:
    {/** �����ظ��������������n��m����ǰʱ��Ϊt����ʹ��ֵΪ(t-n) % (|m-n|)��*/
        GRSPrintfA( "aiAnimBehaviour_REPEAT" );
    }
    break;
    default:
    {
        GRSPrintfA( "0x%02X", emBehaviour );
    }
    break;
    };
}

void ShowAnimations( const aiScene* pModel )
{
    GRSPrintfA( "Anim Count[%4u]:\n", pModel->mNumAnimations );

    for ( size_t i = 0; i < pModel->mNumAnimations; i++ )
    {
        GRSPrintfA( "Anim[%u]��\"%s\"\tDuration Time: " GRS_SHOW_VECTOR_1D_FORMAT " ticks, Channels[%u]\n"
            , (UINT) i
            , pModel->mAnimations[i]->mName.C_Str()
            , pModel->mAnimations[i]->mDuration
            , pModel->mAnimations[i]->mNumChannels );
        GRSConsolePause();
        //GRSPrintfA("\n");
        for ( UINT j = 0; j < pModel->mAnimations[i]->mNumChannels; j++ )
        {
            aiNodeAnim* pChannels = pModel->mAnimations[i]->mChannels[j];

            GRSPrintfA( "Channel[%u]:\"%s\" Scaling Keys[%u], Rotation Keys[%u], Position Keys[%u], "
                , j
                , pChannels->mNodeName.C_Str()
                , pChannels->mNumScalingKeys
                , pChannels->mNumRotationKeys
                , pChannels->mNumPositionKeys
            );
            GRSPrintfA( "Pre State: " );
            ShowAnimationsBehaviour( pChannels->mPreState );
            GRSPrintfA( " Post State: " );
            ShowAnimationsBehaviour( pChannels->mPreState );
            GRSPrintfA( "\n" );

            ShowChannleInfo( pChannels );
        }
    }
}

void ShowChannleInfo( const aiNodeAnim* pChannle )
{
    UINT i = 0;
    UINT nCol = 3;

    GRSPrintfW( L"Scaling Keys[%u]:\n", pChannle->mNumScalingKeys );
    for ( i = 0; i < pChannle->mNumScalingKeys; i++ )
    {
        GRSPrintfW( L"[%4u]:Time=" GRS_SHOW_VECTOR_1D_FORMAT " S" GRS_SHOW_VECTOR_3D_FORMAT ", "
            , i
            , pChannle->mScalingKeys[i].mTime
            , pChannle->mScalingKeys[i].mValue.x
            , pChannle->mScalingKeys[i].mValue.y
            , pChannle->mScalingKeys[i].mValue.z );

        if ( ( ( i + 1 ) % nCol ) == 0 )
        {
            GRSPrintfW( L"\n" );
        }
    }

    GRSPrintfW( L"\nRotation Keys[%u]:\n", pChannle->mNumRotationKeys );

    for ( i = 0; i < pChannle->mNumRotationKeys; i++ )
    {// ��ת��������Ԫ�����ĸ�������������
        GRSPrintfW( L"[%4u]:Time=" GRS_SHOW_VECTOR_1D_FORMAT " Q" GRS_SHOW_VECTOR_4D_FORMAT ", "
            , i
            , pChannle->mRotationKeys[i].mTime
            , pChannle->mRotationKeys[i].mValue.x
            , pChannle->mRotationKeys[i].mValue.y
            , pChannle->mRotationKeys[i].mValue.z
            , pChannle->mRotationKeys[i].mValue.w );
        if ( ( ( i + 1 ) % nCol ) == 0 )
        {
            GRSPrintfW( L"\n" );
        }
    }

    GRSPrintfW( L"\nPosition Keys[%u]:\n", pChannle->mNumPositionKeys );

    for ( i = 0; i < pChannle->mNumPositionKeys; i++ )
    {
        GRSPrintfA( "[%4u]:Time=" GRS_SHOW_VECTOR_1D_FORMAT " T" GRS_SHOW_VECTOR_3D_FORMAT ", "
            , i
            , pChannle->mPositionKeys[i].mTime
            , pChannle->mPositionKeys[i].mValue.x
            , pChannle->mPositionKeys[i].mValue.y
            , pChannle->mPositionKeys[i].mValue.z );
        if ( ( ( i + 1 ) % nCol ) == 0 )
        {
            GRSPrintfA( "\n" );
        }
    }

    GRSPrintfA( "\n" );
}

void TraverseNodeTree( const aiNode* pNode, int iBlank )
{
    GRSPrintBlank( iBlank );

    int iY = GRSGetConsoleCurrentY();

    //��䲻��������
    if ( pNode->mParent )
    {//�и��ڵ�
        aiNode** pBrother = pNode->mParent->mChildren;//�ֵܽڵ��б�
        UINT nPos = 0;
        for ( nPos = 0; nPos < pNode->mParent->mNumChildren; nPos++ )
        {//���ҽڵ����ֵ��б��λ�ã���ǰ�ڵ�϶��ڸ��ڵ���ֵ��б���
            if ( pNode == pNode->mParent->mChildren[nPos] )
            {
                break;
            }
        }

        int iRow = 0;
        SHORT sLine = 0;

        if ( 0 != nPos )
        {//���ǵ�һ���ֵ�
            UINT nPrev = --nPos;
            if ( GRSFindConsoleLine( (void*) pNode->mParent->mChildren[nPrev], sLine ) )
            {
                iRow = iY - sLine - 1;
            }
        }
        else
        {// ��һ���ֵ� �Ҹ�����
            if ( GRSFindConsoleLine( (void*) pNode->mParent, sLine ) )
            {
                iRow = iY - sLine - 1;
            }
        }

        GRSPushConsoleCursor();//������λ��
        while ( iRow-- )
        {//��ͣ���ƶ���꣬���|
            GRSMoveConsoleCursor( 0, -1 );//����
            GRSPrintfA( "|" );//��ӡ|
            GRSMoveConsoleCursor( -1, 0 );//���ƻظ����λ��
        }
        GRSPopConsoleCursor();//�ָ����λ��
    }


    GRSSaveConsoleLine( (void*) pNode, iY );

    GRSPrintfA( "+�D�DName:\"%s\", Childs[%u]"
        , ( pNode->mName.length > 0 ) ? pNode->mName.C_Str() : GRS_NAME_ANONYMITY
        , pNode->mNumChildren
        , pNode->mNumMeshes );

    if ( pNode->mNumMeshes > 0 )
    {
        GRSPrintfW( L", Affect Meshs Count[%u]:{"
            , pNode->mNumMeshes );

        for ( UINT i = 0; i < pNode->mNumMeshes; i++ )
        {
            GRSPrintfW( L"Mesh[%u] ", pNode->mMeshes[i] );
        }

        GRSPrintfW( L"}" );
    }

    GRSPrintfW( L", Node Transformation:\n\n" );
    GRSPrintfMX( pNode->mTransformation, iBlank + 3 );
    GRSPrintfA( "\n" );

    for ( UINT i = 0; i < pNode->mNumChildren; ++i )
    {
        TraverseNodeTree( pNode->mChildren[i], iBlank + 1 );//�ۼ���������������
    }
}

void ShowTextures( const UINT& nIndex, const aiTexture* paiTexture )
{
    GRSPrintfA( "Texture[%u]: \"%s\", Format Hint:\"%s\", Width=%u, Height=%u\n"
        , nIndex
        , paiTexture->mFilename.length > 0 ? paiTexture->mFilename.C_Str() : GRS_NAME_ANONYMITY
        , paiTexture->achFormatHint
        , paiTexture->mWidth
        , paiTexture->mHeight
    );
}

void ShowLights( const UINT& nIndex, const aiLight* paiLight )
{
    GRSPrintfA( "\nLight[%u]: \"%s\" "
        , nIndex
        , paiLight->mName.length > 0 ? paiLight->mName.C_Str() : GRS_NAME_ANONYMITY
        , paiLight->mType
    );
    switch ( paiLight->mType )
    {
    case aiLightSource_DIRECTIONAL:
    {   //! A directional light source has a well-defined direction
        //! but is infinitely far away. That's quite a good
        //! approximation for sun light.
        GRSPrintfW( L"aiLightSource_DIRECTIONAL" );
    }
    break;
    case aiLightSource_POINT:
    {
        //! A point light source has a well-defined position
        //! in space but no direction - it emits light in all
        //! directions. A normal bulb is a point light.
        GRSPrintfW( L"aiLightSource_POINT" );
    }
    break;
    case aiLightSource_SPOT:
    {
        //! A spot light source emits light in a specific
        //! angle. It has a position and a direction it is pointing to.
        //! A good example for a spot light is a light spot in
        //! sport arenas.
        GRSPrintfW( L"aiLightSource_SPOT" );
    }
    break;
    case aiLightSource_AMBIENT:
    {
        //! The generic light level of the world, including the bounces
        //! of all other light sources.
        //! Typically, there's at most one ambient light in a scene.
        //! This light type doesn't have a valid position, direction, or
        //! other properties, just a color.
        GRSPrintfW( L"aiLightSource_AMBIENT" );
    }
    break;
    case aiLightSource_AREA:
    {
        //! An area light is a rectangle with predefined size that uniformly
        //! emits light from one of its sides. The position is center of the
        //! rectangle and direction is its normal vector.
        GRSPrintfW( L"aiLightSource_AREA" );
    }
    break;
    default:
        break;
    }

    GRSPrintfW( L" Position:" GRS_SHOW_VECTOR_3D_FORMAT " Direction:" GRS_SHOW_VECTOR_3D_FORMAT " Up:" GRS_SHOW_VECTOR_3D_FORMAT
        , paiLight->mPosition.x
        , paiLight->mPosition.y
        , paiLight->mPosition.z
        , paiLight->mDirection.x
        , paiLight->mDirection.y
        , paiLight->mDirection.z
        , paiLight->mUp.x
        , paiLight->mUp.y
        , paiLight->mUp.z
    );

    GRSPrintfW( L"\n\tLight Attenuation Property: Constant" GRS_SHOW_VECTOR_1D_FORMAT\
        " Linear" GRS_SHOW_VECTOR_1D_FORMAT " Quadratic" GRS_SHOW_VECTOR_1D_FORMAT
        , paiLight->mAttenuationConstant
        , paiLight->mAttenuationLinear
        , paiLight->mAttenuationQuadratic
    );

    GRSPrintfW( L"\n\tLight Color: Diffuse" GRS_SHOW_VECTOR_3D_FORMAT " Specular" GRS_SHOW_VECTOR_3D_FORMAT " Ambient" GRS_SHOW_VECTOR_3D_FORMAT
        , paiLight->mColorDiffuse.r
        , paiLight->mColorDiffuse.g
        , paiLight->mColorDiffuse.b
        , paiLight->mColorSpecular.r
        , paiLight->mColorSpecular.g
        , paiLight->mColorSpecular.b
        , paiLight->mColorAmbient.r
        , paiLight->mColorAmbient.g
        , paiLight->mColorAmbient.b
    );

    GRSPrintfW( L"\n\tSize Light Property : Inner Cone" GRS_SHOW_VECTOR_1D_FORMAT " Outer Cone" GRS_SHOW_VECTOR_1D_FORMAT
        , paiLight->mAngleInnerCone
        , paiLight->mAngleOuterCone
    );

    GRSPrintfW( L"\n\tSpot Light Property : Size" GRS_SHOW_VECTOR_2D_FORMAT "\n"
        , paiLight->mSize.x
        , paiLight->mSize.y
    );


}

void ShowCamera( const UINT& nIndex, const aiCamera* paiCamera )
{
    GRSPrintfA( "\nCamera[%u]: \"%s\", "
        , nIndex
        , paiCamera->mName.length > 0 ? paiCamera->mName.C_Str() : GRS_NAME_ANONYMITY
    );

    GRSPrintfW( L"Position:" GRS_SHOW_VECTOR_3D_FORMAT ", Up:" GRS_SHOW_VECTOR_3D_FORMAT ", LookAt:" GRS_SHOW_VECTOR_3D_FORMAT
        , paiCamera->mPosition.x
        , paiCamera->mPosition.y
        , paiCamera->mPosition.z
        , paiCamera->mUp.x
        , paiCamera->mUp.y
        , paiCamera->mUp.z
        , paiCamera->mLookAt.x
        , paiCamera->mLookAt.y
        , paiCamera->mLookAt.z
    );

    GRSPrintfW( L", FOV:" GRS_SHOW_VECTOR_1D_FORMAT ", Near:" GRS_SHOW_VECTOR_1D_FORMAT ", Far:" GRS_SHOW_VECTOR_1D_FORMAT ", Aspect:" GRS_SHOW_VECTOR_1D_FORMAT "\n"
        , paiCamera->mHorizontalFOV
        , paiCamera->mClipPlaneNear
        , paiCamera->mClipPlaneFar
        , paiCamera->mAspect
    );
}

void ShowMetadata( const aiMetadata* paiMetadata )
{
    for ( UINT i = 0; i < paiMetadata->mNumProperties; i++ )
    {
        GRSPrintfA( "MetaData[%u] - Key:\"%s\",Value="
            , i
            , paiMetadata->mKeys[i].C_Str()
        );

        if ( nullptr != paiMetadata->mValues[i].mData )
        {
            switch ( paiMetadata->mValues[i].mType )
            {
            case AI_BOOL:
            {
                GRSPrintfA( "(BOOL)%s"
                    , ( *(BOOL*) paiMetadata->mValues[i].mData ) == FALSE ? "FALSE" : "TRUE" );
            }
            break;
            case AI_INT32:
            {
                GRSPrintfA( "(INT32)%d"
                    , ( *(INT32*) paiMetadata->mValues[i].mData ) );

            }
            break;
            case AI_UINT64:
            {
                GRSPrintfA( "(UINT64)%u"
                    , ( *(UINT64*) paiMetadata->mValues[i].mData ) );

            }
            break;
            case AI_FLOAT:
            {
                GRSPrintfA( "(FLOAT)%11.6f"
                    , ( *(FLOAT*) paiMetadata->mValues[i].mData ) );

            }
            break;
            case AI_DOUBLE:
            {
                GRSPrintfA( "(DOUBLE)%11.6f"
                    , ( *(DOUBLE*) paiMetadata->mValues[i].mData ) );

            }
            break;
            case AI_AISTRING:
            {
                GRSPrintfA( "(String)\"%s\""
                    , ( (CHAR*) paiMetadata->mValues[i].mData ) );

            }
            break;
            case AI_AIVECTOR3D:
            {
                aiVector3D* pV3Tmp = (aiVector3D*) paiMetadata->mValues[i].mData;
                GRSPrintfA( "(Vector3D) " GRS_SHOW_VECTOR_3D_FORMAT
                    , pV3Tmp->x
                    , pV3Tmp->y
                    , pV3Tmp->z
                );
            }
            break;

            default:
            {
                GRSPrintfW( L"nullptr;" );
            }
            break;
            }
        }
        else
        {
            GRSPrintfW( L"nullptr;" );
        }

        GRSPrintfW( L"\n" );
    }
}