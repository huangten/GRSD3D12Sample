#pragma once

#include <strsafe.h>
#include <atlbase.h>
#include <atlcoll.h>
#include <atlchecked.h>
#include <atlstr.h>
#include <atlconv.h>

#include <DirectXMath.h>
using namespace DirectX;

#include "assimp/Importer.hpp"      // �������ڸ�ͷ�ļ��ж���
#include "assimp/scene.h"           // ��ȡ����ģ�����ݶ�����scene��
#include "assimp/postprocess.h"

// �����ļ�ʱԤ����ı�־
// aiProcess_LimitBoneWeights
// aiProcess_OptimizeMeshes
// aiProcess_MakeLeftHanded
// aiProcess_ConvertToLeftHanded
// aiProcess_MakeLeftHanded
// aiProcess_ConvertToLeftHanded
//| aiProcess_MakeLeftHanded\

#define ASSIMP_LOAD_FLAGS (aiProcess_Triangulate\
 | aiProcess_GenSmoothNormals\
 | aiProcess_GenBoundingBoxes\
 | aiProcess_JoinIdenticalVertices\
 | aiProcess_FlipUVs\
 | aiProcess_ConvertToLeftHanded\
 | aiProcess_LimitBoneWeights)			

// ÿ�������������������������ĵ����־�������ģ��Ӧ�ö����ϸ������������
#define GRS_INDICES_PER_FACE 3

// ����ÿ����������������ĸ�������������ĵ����־�������ϸ��޶�ÿ�����㱻���4����ͷӰ�죩
#define GRS_BONE_DATACNT 4

// ��������֧�ֵĹ�ͷ����
#define GRS_MAX_BONES 256


struct ST_GRS_VERTEX_BONE
{
public:
	UINT32  m_nBonesIDs[GRS_BONE_DATACNT];
	FLOAT	m_fWeights[GRS_BONE_DATACNT];
public:
	VOID AddBoneData(UINT nBoneID, FLOAT fWeight)
	{
		for (UINT32 i = 0; i < GRS_BONE_DATACNT; i++)
		{
			if ( m_fWeights[i] == 0.0 )
			{
				m_nBonesIDs[i] = nBoneID;
				m_fWeights[i] = fWeight;
				break;
			}
		}
	}
};

// ��������������ÿ����ͷ�����ģ�Ϳռ��λ�ƾ���
// ����BoneOffset��λ��
// FinalTransformation���ڽ������ն���ʱ������ʵ��ÿ����ͷ�ı任
struct ST_GRS_BONE_DATA
{
	XMMATRIX m_mxBoneOffset;
	XMMATRIX m_mxFinalTransformation;
};

typedef CAtlArray<XMFLOAT4>						CGRSARPositions;
typedef CAtlArray<XMFLOAT4>						CGRSARNormals;
typedef CAtlArray<XMFLOAT2>						CGRSARTexCoords;
typedef CAtlArray<UINT>								CGRSARIndices;
typedef CAtlArray<ST_GRS_VERTEX_BONE>	CGRSARVertexBones;
typedef CAtlArray<ST_GRS_BONE_DATA>		CGRSARBoneDatas;
typedef CAtlArray<CStringA>						CGRSARTTextureName;
typedef CAtlArray<XMMATRIX>					CGRSARMatrix;
typedef CAtlMap<UINT, CStringA>				CGRSMapUINT2String;
typedef CAtlMap<CStringA, UINT>				CGRSMapString2UINT;
typedef CAtlMap<UINT, UINT>						CGRSMapUINT2UINT;
// ģ����������Ķ���ƫ�Ƶ���Ϣ
struct ST_GRS_SUBMESH_DATA
{
	UINT m_nNumIndices;
	UINT m_nBaseVertex;
	UINT m_nBaseIndex;
	UINT m_nMaterialIndex;
};

typedef CAtlArray<ST_GRS_SUBMESH_DATA> CGRSSubMesh;

const UINT			g_ncSlotCnt = 4;		// ��4������ϴ���������
struct ST_GRS_MESH_DATA
{
	const aiScene*		m_paiModel;
	CStringA			m_strFileName;
	XMMATRIX			m_mxModel;

	CGRSSubMesh			m_arSubMeshInfo;

	CGRSARPositions		m_arPositions;
	CGRSARNormals		m_arNormals;
	CGRSARTexCoords		m_arTexCoords;
	CGRSARVertexBones	m_arBoneIndices;
	CGRSARIndices		m_arIndices;

	CGRSMapString2UINT	m_mapTextrueName2Index;
	CGRSMapUINT2UINT	m_mapTextureIndex2HeapIndex;

	CGRSARBoneDatas		m_arBoneDatas;
	CGRSMapString2UINT	m_mapName2Bone;			//����->����������
	CGRSMapString2UINT	m_mapAnimName2Index;	//����->����������

	UINT				m_nCurrentAnimIndex;	// ��ǰ���ŵĶ���������������ǰ������
};

__inline const XMMATRIX& MXEqual(XMMATRIX& mxDX, const aiMatrix4x4& mxAI)
{
	mxDX = XMMatrixSet(mxAI.a1, mxAI.a2, mxAI.a3, mxAI.a4,
		mxAI.b1, mxAI.b2, mxAI.b3, mxAI.b4,
		mxAI.c1, mxAI.c2, mxAI.c3, mxAI.c4,
		mxAI.d1, mxAI.d2, mxAI.d3, mxAI.d4);
	return mxDX;
}

__inline const XMVECTOR& VectorEqual(XMVECTOR& vDX, const aiVector3D& vAI)
{
	vDX = XMVectorSet(vAI.x, vAI.y, vAI.z, 0);
	return vDX;
}

__inline const XMVECTOR& QuaternionEqual(XMVECTOR& qDX, const aiQuaternion& qAI)
{
	qDX = XMVectorSet(qAI.x, qAI.y, qAI.z, qAI.w);
	return qDX;
}

__inline VOID VectorLerp(XMVECTOR& vOut, aiVector3D& aivStart, aiVector3D& aivEnd, FLOAT t)
{
	XMVECTOR vStart = XMVectorSet(aivStart.x, aivStart.y, aivStart.z, 0);
	XMVECTOR vEnd = XMVectorSet(aivEnd.x, aivEnd.y, aivEnd.z, 0);
	vOut = XMVectorLerp(vStart, vEnd, t);
}

__inline VOID QuaternionSlerp(XMVECTOR& vOut, aiQuaternion& qStart, aiQuaternion& qEnd, FLOAT t)
{
	//DirectXMath��Ԫ������ʹ��XMVECTOR 4 - vector����ʾ��Ԫ��������X��Y��Z������ʸ�����֣�W�����Ǳ������֡�

	XMVECTOR qdxStart;
	XMVECTOR qdxEnd;
	QuaternionEqual(qdxStart, qStart);
	QuaternionEqual(qdxEnd, qEnd);

	vOut = XMQuaternionSlerp(qdxStart, qdxEnd, t);
}

BOOL LoadMesh(LPCSTR pszFileName, ST_GRS_MESH_DATA& stMeshData);
VOID CalcAnimation(ST_GRS_MESH_DATA& stMeshData, FLOAT fTimeInSeconds, CGRSARMatrix& arTransforms);
