// Simplified PBR HLSL 



static const float PI = 3.14159265359;

cbuffer MVPBuffer : register( b0 )
{
    float4x4 mxWorld;                   //�������������ʵ��Model->World��ת������
    float4x4 mxView;                    //�Ӿ���
    float4x4 mxProjection;				//ͶӰ����
    float4x4 mxViewProj;                //�Ӿ���*ͶӰ
    float4x4 mxMVP;                     //����*�Ӿ���*ͶӰ
};

// Camera
cbuffer ST_CB_CAMERA: register( b1 )
{
    float4 g_v4CameraPos;
};

#define GRS_LIGHT_COUNT 8
// lights
cbuffer ST_CB_LIGHTS : register( b2 )
{
    float4 g_v4LightPos[GRS_LIGHT_COUNT];
    float4 g_v4LightClr[GRS_LIGHT_COUNT];
};

// PBR material parameters
cbuffer ST_CB_PBR_MATERIAL : register( b3 )
{
    float3   g_v3Albedo;      // ������
    float    g_fMetallic;     // ������
    float    g_fRoughness;    // �ֲڶ�
    float    g_fAO;           // �������ڱ�
};

struct PSInput
{
    float4 g_v4PosWVP   : SV_POSITION;
    float4 g_v4PosWorld : POSITION;
    float4 g_v4Normal   : NORMAL;
    float2 g_v2UV       : TEXCOORD;
};

float3 Fresnel_Schlick( float cosTheta, float3 F0 )
{
	return F0 + ( 1.0 - F0 ) * pow( 1.0 - cosTheta, 5.0 );
}

float Distribution_GGX( float3 N, float3 H, float fRoughness )
{
    float a = fRoughness * fRoughness;
    float a2 = a * a;
    float NdotH = max( dot( N, H ), 0.0 );
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = ( NdotH2 * ( a2 - 1.0 ) + 1.0 );
    denom = PI * denom * denom;

    return num / denom;
}

float Geometry_Schlick_GGX( float NdotV, float fRoughness )
{
    // ֱ�ӹ���ʱ k = (fRoughness + 1)^2 / 8;
    // IBL ʱ k = ��fRoughness^2)/2;
    float r = ( fRoughness + 1.0 );
    float k = ( r * r ) / 8.0;

    float num = NdotV;
    float denom = NdotV * ( 1.0 - k ) + k;

    return num / denom;
}

float Geometry_Smith( float3 N, float3 V, float3 L, float fRoughness )
{
    float NdotV = max( dot( N, V ), 0.0 );
    float NdotL = max( dot( N, L ), 0.0 );
    float ggx2 = Geometry_Schlick_GGX( NdotV, fRoughness );
    float ggx1 = Geometry_Schlick_GGX( NdotL, fRoughness );

    return ggx1 * ggx2;
}

float4 PSMain( PSInput stPSInput ) : SV_TARGET
{
    // ʹ�ò�ֵ���ɵĹ⻬����
    float3 N = normalize( stPSInput.g_v4Normal.xyz );
    // ������
    float3 V = normalize( g_v4CameraPos.xyz - stPSInput.g_v4PosWorld.xyz );

    float3 F0 = float3( 0.04f, 0.04f, 0.04f );

    // Gamma������ɫ
    float3 v3Albedo = pow( g_v3Albedo, 2.2f );
    
    F0 = lerp( F0, v3Albedo, g_fMetallic );

    // ��������
    float3 Lo = float3( 0.0f, 0.0f, 0.0f );

    // �ֲڶ�
    float fRoughness = g_fRoughness;

    for ( int i = 0; i < GRS_LIGHT_COUNT; ++i )
    {// ��ÿһ����Դ�����ջ��ַ���
        // ���Դ�����
        // ���������
        float3 L = normalize( g_v4LightPos[i].xyz - stPSInput.g_v4PosWorld.xyz );
        // �м�������������뷨�ߵĽ�ƽ���ߣ�
        float3 H = normalize( V + L );
        float distance = length( g_v4LightPos[i].xyz - stPSInput.g_v4PosWorld.xyz );

        //float attenuation = 1.0 / ( distance * distance );
        // �Ѿ�Gamma�����ˣ��Ͳ�Ҫ���η���˥���ˣ�����˥���Ϳ�����
        float attenuation = 1.0 / distance ;
        
        float3 radiance = g_v4LightClr[i].xyz * attenuation;

        // Cook-Torrance����ģ�� BRDF
        float NDF = Distribution_GGX( N, H, fRoughness );
        float G = Geometry_Smith( N, V, L, fRoughness );
        float3 F = Fresnel_Schlick( max( dot( H, V ), 0.0 ), F0 );

        float3 kS = F;
        float3 kD = float3( 1.0f,1.0f,1.0f ) - kS;
        kD *= 1.0 - g_fMetallic;

        float3 numerator = NDF * G * F;
        float denominator = 4.0 * max( dot( N, V ), 0.0 ) * max( dot( N, L ), 0.0 );
        float3 specular = numerator / max( denominator, 0.001 );

        // add to outgoing radiance Lo
        float NdotL = max( dot( N, L ), 0.0 );
        Lo += ( kD * v3Albedo / PI + specular ) * radiance * NdotL;
    }

    float fAO = g_fAO;
    // ��������
    float3 ambient = float3( 0.03f, 0.03f, 0.03f ) * v3Albedo * fAO;
    float3 color = ambient + Lo;

    color = color / ( color + float3( 1.0f,1.0f,1.0f ) );
    // Gamma ����
    color = pow( color, 1.0f / 2.2f );
    return float4( color, 1.0 );
}

