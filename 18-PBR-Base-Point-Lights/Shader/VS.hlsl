// Simplified PBR HLSL Vertex Shader

cbuffer MVPBuffer : register( b0 )
{
	float4x4 mxWorld;                   //�������������ʵ��Model->World��ת������
	float4x4 mxView;                    //�Ӿ���
	float4x4 mxProjection;				//ͶӰ����
	float4x4 mxViewProj;                //�Ӿ���*ͶӰ
	float4x4 mxMVP;                     //����*�Ӿ���*ͶӰ
};

struct PSInput
{
	float4 m_v4PosWVP : SV_POSITION;
	float4 m_v4PosWorld : POSITION;
	float4 m_v4Normal: NORMAL;
	float2 m_v2UV: TEXCOORD;
};

PSInput VSMain( float4 v4LocalPos : POSITION, float4 v4LocalNormal:NORMAL, float2 v2UV : TEXCOORD )
{
	PSInput stOutput;

	stOutput.m_v4PosWVP = mul( v4LocalPos, mxMVP );
	stOutput.m_v4PosWorld = mul( v4LocalPos, mxWorld);
	v4LocalNormal.w = 0.0f;
	stOutput.m_v4Normal = mul( v4LocalNormal, mxWorld);
	stOutput.m_v2UV = v2UV;
	return stOutput;
}