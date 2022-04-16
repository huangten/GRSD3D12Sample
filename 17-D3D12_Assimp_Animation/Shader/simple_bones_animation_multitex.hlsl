// 02-3D_Animation_With_Assimp_D3D12 Vertex Shader
cbuffer cbMVP : register(b0)
{
    float4x4 mxWorld;                   //�������������ʵ��Model->World��ת������
    float4x4 mxView;                    //�Ӿ���
    float4x4 mxProjection;              //ͶӰ����
    float4x4 mxViewProj;                //�Ӿ���*ͶӰ
    float4x4 mxMVP;                     //����*�Ӿ���*ͶӰ
};

cbuffer cbBones : register(b1)
{
    float4x4 mxBones[256];              //������������ɫ�塱 ���256��"���ͷ"���ڴ���Դ�࣬���ԣ�
};

struct VSInput
{
    float4 position : POSITION0;        //����λ��
    float4 normal   : NORMAL0;          //����
    float2 texuv    : TEXCOORD0;        //��������
    uint4  bonesID  : BLENDINDICES0;    //��������
    float4 fWeights : BLENDWEIGHT0;     //����Ȩ��
};

struct PSInput
{
    float4 position : SV_POSITION0;        //λ��
    float4 normal   : NORMAL0;          //����
    float2 texuv    : TEXCOORD0;        //��������
};


PSInput VSMain(VSInput vin)
{
    PSInput vout;
    // ���ݹ�ͷ�����Լ�Ȩ�ؼ����ͷ�任�ĸ��Ͼ���
    float4x4 mxBonesTrans 
        = vin.fWeights[0] * mxBones[vin.bonesID[0]]
        + vin.fWeights[1] * mxBones[vin.bonesID[1]]
        + vin.fWeights[2] * mxBones[vin.bonesID[2]]
        + vin.fWeights[3] * mxBones[vin.bonesID[3]];

    // �任��ͷ�Զ����Ӱ��
    vout.position = mul(vin.position, mxBonesTrans);
    
    //���ձ任���ӿռ�
    vout.position = mul(vout.position, mxMVP);   

    //vout.position = mul(vin.position, mxMVP);

    // ���������������任
    vout.normal = mul(vin.normal, mxWorld);

    // ��������ԭ�����
    vout.texuv = vin.texuv;
    

    return vout;
}

Texture2D		g_txDiffuse : register(t0);
SamplerState    g_sampler   : register(s0);

float4 PSMain(PSInput input) : SV_TARGET
{
    //return float4( 0.9f, 0.9f, 0.9f, 1.0f );
    return float4(g_txDiffuse.Sample(g_sampler, input.texuv).rgb,1.0f);
}