#pragma pack_matrix(row_major)

cbuffer ObjectConstants : register(b0)
{
    float4x4 World;
};

cbuffer ViewConstants : register(b1)
{
    float4x4 ViewProjection;
};

cbuffer DecalConstants : register(b2)
{
    float4x4 DecalViewProjection;
    float4 DecalColor;
    float4 DecalForward;
};

Texture2D DecalTexture : register(t0);
SamplerState ClampSampler : register(s0);

struct VSInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float4 Tangent : TANGENT;
    float2 UV : TEXCOORD;
    float4 Color : COLOR;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float4 DecalClipPosition : TEXCOORD0;
    float3 WorldNormal : TEXCOORD4;
    float3 WorldPosition : TEXCOORD1;
};

PSInput mainVS(VSInput input)
{
    PSInput Output;
    
    float4 WorldPosition = mul(float4(input.Position, 1.0f), World);
    
    Output.Position = mul(WorldPosition, ViewProjection);
    Output.Position.z -= 0.00001f * Output.Position.w;
    Output.DecalClipPosition = mul(WorldPosition, DecalViewProjection);
    Output.WorldNormal = mul(float4(input.Normal, 0.0f), World).xyz;
    Output.WorldPosition = WorldPosition.xyz;
    
    return Output;
}

float4 mainPS(PSInput input) : SV_TARGET
{
    float4 Result;
    
    float3 DecalNDC = input.DecalClipPosition.xyz / input.DecalClipPosition.w;
    
    if (DecalNDC.x < -1.0f || DecalNDC.x > 1.0f || DecalNDC.y < -1.0f || DecalNDC.y > 1.0f || DecalNDC.z < 0.0f || DecalNDC.z > 1.0f)
    {
        discard;
    }
    
    float angleCos = dot(normalize(input.WorldNormal), -DecalForward.xyz);
    if (angleCos <= 0.0f)
    {
        discard;
    }
    
    float2 DecalUV;
    DecalUV.x = DecalNDC.x * 0.5f + 0.5f;
    DecalUV.y = -DecalNDC.y * 0.5f + 0.5f;
    
    float4 DecalColorSample = DecalTexture.Sample(ClampSampler, DecalUV);
    DecalColorSample.rgb *= DecalColor.rgb;
    
    // TODO : decrease alpha based on distance from decal center
    
    Result = float4(DecalColorSample.xyz, 1.0f);
    
    return Result;
}