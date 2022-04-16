// Simplified PBR HLSL Vertex Shader

cbuffer MVPBuffer : register( b0 )
{
	float4x4 mxWorld;                   //�������������ʵ��Model->World��ת������
	float4x4 mxView;                    //�Ӿ���
	float4x4 mxProjection;				//ͶӰ����
	float4x4 mxViewProj;                //�Ӿ���*ͶӰ
	float4x4 mxMVP;                     //����*�Ӿ���*ͶӰ
};

struct VSInput
{
	float4		mv4LocalPos		: POSITION;
	float4		mv4LocalNormal	: NORMAL;
	float2		mv2UV			: TEXCOORD;
	float4x4	mxModel2World	: WORLD;
	float4		mv4Albedo		: COLOR0;    // ������
	float		mfMetallic		: COLOR1;    // ������
	float		mfRoughness		: COLOR2;    // �ֲڶ�
	float		mfAO			: COLOR3;
	uint		mnInstanceId	: SV_InstanceID;
};

struct PSInput
{
	float4		m_v4PosWVP		: SV_POSITION;
	float4		m_v4PosWorld	: POSITION;
	float4		m_v4Normal		: NORMAL;
	float2		m_v2UV			: TEXCOORD;
	float4x4	mxModel2World	: WORLD;
	float3		mv3Albedo		: COLOR0;    // ������
	float		mfMetallic		: COLOR1;    // ������
	float		mfRoughness		: COLOR2;    // �ֲڶ�
	float		mfAO			: COLOR3;
};

PSInput VSMain( VSInput vsInput )
{
	PSInput stOutput;
	// �Ȱ�ÿ��ʵ��������任����������ϵ��
	stOutput.m_v4PosWVP = mul( vsInput.mv4LocalPos, vsInput.mxModel2World );
	// �ٽ�����������ϵ����任,�õ�������������ϵ�е�����
	stOutput.m_v4PosWorld = mul( stOutput.m_v4PosWVP, mxWorld );
	// �任����-ͶӰ�ռ���
	stOutput.m_v4PosWVP = mul( stOutput.m_v4PosWVP, mxMVP );

	// �����ȱ任����������ϵ�У��ٽ�����������任
	vsInput.mv4LocalNormal.w = 0.0f;
	stOutput.m_v4Normal = mul( vsInput.mv4LocalNormal, vsInput.mxModel2World );
	vsInput.mv4LocalNormal.w = 0.0f;
	stOutput.m_v4Normal = mul( vsInput.mv4LocalNormal, mxWorld);
	vsInput.mv4LocalNormal.w = 0.0f;
	stOutput.m_v4Normal = normalize( stOutput.m_v4Normal );
	
	stOutput.mxModel2World = vsInput.mxModel2World;
	stOutput.m_v2UV = vsInput.mv2UV;
	stOutput.mv3Albedo = vsInput.mv4Albedo.xyz;
	stOutput.mfMetallic = vsInput.mfMetallic;
	stOutput.mfRoughness = vsInput.mfRoughness;
	stOutput.mfAO = vsInput.mfAO;
	return stOutput;
}