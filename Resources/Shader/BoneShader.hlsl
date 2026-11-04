#pragma pack_matrix(row_major)

cbuffer Constants : register(b0)
{
    matrix World;
    matrix ViewProjection;
    float4 Color;
    float3 CameraLocation;
    float Padding;
}

struct VertexInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float4 Tangent : TANGENT;
    float2 UV : TEXCOORD;
    float4 Color : COLOR;
};

struct PixelInput
{
    float4 Position : SV_POSITION;
    float3 WorldPosition : TEXCOORD0;
    float3 Normal : NORMAL;
};

PixelInput mainVS(VertexInput Input)
{
    PixelInput Output;
    
    Output.Position = mul(float4(Input.Position, 1.0f), World);
    Output.WorldPosition = Output.Position.xyz;
    Output.Position = mul(Output.Position, ViewProjection);
    Output.Normal = mul(Input.Normal, (float3x3)World); // 균일 스케일만 쓰기 때문에 월드 행렬 그대로 사용
    
    return Output;
}

float4 mainPS(PixelInput Input) : SV_TARGET
{
    float3 N = normalize(Input.Normal);
    float3 V = normalize(CameraLocation - Input.WorldPosition);
    return float4(Color.rgb * lerp(0.35f, 1.0f, saturate(dot(N, V))), Color.a);
}
