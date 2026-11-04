#pragma pack_matrix(row_major)

#include "LightData.hlsli"  
#include "LightEvaluation.hlsli"  
#include "LightGridCommon.hlsli"

struct FAmbientLightInfo
{
    float4 ColorIntensity;
};

cbuffer ObjectConstants : register(b0)
{
    float4x4 World;
};

cbuffer Lighting : register(b1)
{
    FAmbientLightInfo Ambient;
    float4 CameraPosition;
    uint4 LightCounts;  // x: Point, y: Spot, z: Directional, w: Rect
    uint4 LightOffsets; // SceneLights 시작 위치
    float4 SkyColorIntensity;
};

cbuffer DecalConstants : register(b2)
{
    float4x4 DecalViewProjection;
    float4 DecalColor;
    float4 DecalForward;
};

cbuffer ViewConstants : register(b4)
{
    float4x4 ViewProjection;
};

Texture2D DecalTexture : register(t0);
Texture2D DecalNormalTexture : register(t1);
StructuredBuffer<FLightRenderParameters> SceneLights : register(t8);
StructuredBuffer<uint4> TileLightRanges : register(t9);
StructuredBuffer<uint> TilePointIndices : register(t10);
TextureCube<float4> SkyIrradiance : register(t11);
StructuredBuffer<uint> TileSpotIndices : register(t12);
Texture2D<float4> ScreenShadowMask : register(t13);

SamplerState ClampSampler : register(s0);
SamplerState SkySampler : register(s1);

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
    float4 WorldTangent : TEXCOORD5;
    float3 WorldPosition : TEXCOORD1;
    float ViewDepth : TEXCOORD2;
};

// 광원의 그림자 (1 = 빛 받음). 채널이 없으면 1.
float GetLightShadow(FLightRenderParameters Light, float3 PixelPosition)
{
    if (Light.ShadowMapChannel == INVALID_SHADOW_CHANNEL)
    {
        return 1.0f;
    }
    return ScreenShadowMask.Load(int3(int2(PixelPosition.xy), 0))[Light.ShadowMapChannel];
}

float3 CalculateAmbientLight(FAmbientLightInfo Info)
{
    return Info.ColorIntensity.rgb * max(Info.ColorIntensity.a, 0.0f);
}

float3 CalculateSkyLight(float3 N)
{
    if (SkyColorIntensity.a <= 0.0f)
    {
        return 0.0f;
    }
    return SkyIrradiance.SampleLevel(SkySampler, N, 0).rgb * SkyColorIntensity.rgb * SkyColorIntensity.a;
}

void AddLight(inout FLightResult Result, float3 N, float3 V, float3 L, float3 Radiance, bool bIncludeSpecular)
{
    float3 SpecularColor = float3(0.04f, 0.04f, 0.04f);
    float Shininess = 32.0f;
    AccumulateLight(Result, N, V, L, Radiance, SpecularColor, Shininess, bIncludeSpecular);
}

void CalculateDirectionalLight(inout FLightResult Result, FLightRenderParameters Info, float3 N, float3 V, bool bIncludeSpecular)
{
    if (Info.Intensity <= 0.0f)
    {
        return;
    }
    
    float3 L = normalize(-Info.Direction);
    float3 Radiance = Info.Color * Info.Intensity;
    
    AddLight(Result, N, V, L, Radiance, bIncludeSpecular);
}

void CalculatePointLight(inout FLightResult Result, FLightRenderParameters Info, float3 P, float3 N, float3 V, bool bIncludeSpecular)
{
    float3 ToLight = Info.Position - P;
    float DistanceSquared = dot(ToLight, ToLight);
    float RadiusSquared = Info.Radius * Info.Radius;
    if (RadiusSquared <= 0.0f || DistanceSquared >= RadiusSquared || DistanceSquared < 1.0e-8f)
    {
        return;
    }
    float Distance = sqrt(max(DistanceSquared, 1.0e-6f));
    float Fade = pow(saturate(1.0f - Distance / Info.Radius), max(Info.FalloffExponent, 0.0f));
    if (Fade <= 0.0f)
    {
        return;
    }
    float3 L = ToLight / Distance;
    float3 Radiance = Info.Color * max(Info.Intensity, 0.0f) * Fade;
    AddLight(Result, N, V, L, Radiance, bIncludeSpecular);
}

void CalculateSpotLight(inout FLightResult Result, FLightRenderParameters Info, float3 P, float3 N, float3 V, bool bIncludeSpecular)
{
    float3 ToLight = Info.Position - P;
    float DistanceSquared = dot(ToLight, ToLight);
    float RadiusFade = CalculateDistanceAttenuation(DistanceSquared, Info.Radius * Info.Radius);
    if (RadiusFade <= 0.0f)
    {
        return;
    }
    float3 L = ToLight / sqrt(max(DistanceSquared, 1.0e-6f));
    float ConeCos = dot(-L, normalize(Info.Direction));
    float ConeFade = CalculateConeAttenuation(ConeCos, Info.InnerConeCos, Info.OuterConeCos);
    if (ConeFade <= 0.0f)
    {
        return;
    }
    float3 Radiance = Info.Color * max(Info.Intensity, 0.0f) * (RadiusFade * ConeFade);
    AddLight(Result, N, V, L, Radiance, bIncludeSpecular);
}

void CalculateRectLight(inout FLightResult Result, FLightRenderParameters Info, float3 P, float3 N, float3 V, bool bIncludeSpecular)
{
    float3 Position = Info.Position;
    float Radius = Info.Radius;
    float3 Direction = Info.Direction;
    float BarnDoorLength = Info.BarnDoorLength;
    float3 Color = Info.Color;
    float Intensity = Info.Intensity;
    float BarnDoorCos = Info.BarnDoorCos;
    float BarnDoorSin = Info.BarnDoorSin;
    float FalloffExponent = Info.FalloffExponent;
    float3 Right = Info.Right;
    float HalfWidth = Info.HalfWidth;
    float HalfHeight = Info.HalfHeight;

    float3 Up = cross(Direction, Right);
    float3 CenterToSurface = P - Position;

    float RectX = clamp(dot(CenterToSurface, Right), -HalfWidth, HalfWidth);
    float RectY = clamp(dot(CenterToSurface, Up), -HalfHeight, HalfHeight);
    float3 ClosestPoint = Position + Right * RectX + Up * RectY;

    float3 ToLight = ClosestPoint - P;
    float DistanceSquared = dot(ToLight, ToLight);
    float RadiusSquared = Radius * Radius;
    if (RadiusSquared <= 0.0f || DistanceSquared >= RadiusSquared || DistanceSquared < 1.0e-8f)
    {
        return;
    }

    float Distance = sqrt(max(DistanceSquared, 1.0e-6f));
    float3 L = ToLight / Distance;

    float Attenuation = pow(saturate(1.0f - Distance / max(Radius, 0.001f)), max(FalloffExponent, 0.001f));
    float FrontFace = saturate(dot(-L, Direction));
    if (FrontFace <= 0.0f || Attenuation <= 0.0f)
    {
        return;
    }

    float BarnDoorMask = 1.0f;
    if (BarnDoorLength > 0.0f)
    {
        float3 EmitterToSurface = P - ClosestPoint;
        float EmitterDist = max(length(EmitterToSurface), 0.0001f);
        float ForwardDist = dot(EmitterToSurface, Direction);
        float LateralX = abs(dot(EmitterToSurface, Right));
        float LateralY = abs(dot(EmitterToSurface, Up));
        float SlackX = (ForwardDist * BarnDoorSin - LateralX * BarnDoorCos) / EmitterDist;
        float SlackY = (ForwardDist * BarnDoorSin - LateralY * BarnDoorCos) / EmitterDist;
        BarnDoorMask = smoothstep(-0.05f, 0.0f, SlackX) * smoothstep(-0.05f, 0.0f, SlackY);
    }

    float3 Radiance = Color * max(Intensity, 0.0f) * Attenuation * FrontFace * BarnDoorMask;
    AddLight(Result, N, V, L, Radiance, bIncludeSpecular);
}

FLightResult CalculateBaseLighting(float3 PixelPosition, float3 P, float ViewDepth, float3 N, float3 V, bool bIncludeSpecular)
{
    FLightResult Result;
    Result.Diffuse = CalculateAmbientLight(Ambient) + CalculateSkyLight(N);
    Result.Specular = 0.0f;

    [loop]
    for (uint DirectionalIndex = 0; DirectionalIndex < LightCounts.z; ++DirectionalIndex)
    {
        FLightRenderParameters Light = SceneLights[LightOffsets.z + DirectionalIndex];
        float Shadow = GetLightShadow(Light, PixelPosition);
            
        FLightResult DirectionalResult = (FLightResult)0;
        CalculateDirectionalLight(DirectionalResult, Light, N, V, bIncludeSpecular);
        DirectionalResult.Diffuse *= Shadow;
        DirectionalResult.Specular *= Shadow;
         
        Result.Diffuse += DirectionalResult.Diffuse;
        Result.Specular += DirectionalResult.Specular;
    }
    
    return Result;
}

void AddAllLocalLights(inout FLightResult Result, float3 PixelPosition, float3 P, float3 N, float3 V, bool bIncludeSpecular)
{
    [loop]
    for (uint PointIndex = 0; PointIndex < LightCounts.x; ++PointIndex)
    {
        FLightRenderParameters Light = SceneLights[LightOffsets.x + PointIndex];
        float Shadow = GetLightShadow(Light, PixelPosition);

        FLightResult PointResult = (FLightResult)0;
        CalculatePointLight(PointResult, Light, P, N, V, bIncludeSpecular);
         
        Result.Diffuse += PointResult.Diffuse * Shadow;
        Result.Specular += PointResult.Specular * Shadow;
    }
    [loop]
    for (uint SpotIndex = 0; SpotIndex < LightCounts.y; ++SpotIndex)
    {
        FLightRenderParameters Light = SceneLights[LightOffsets.y + SpotIndex];
        float Shadow = GetLightShadow(Light, PixelPosition);
            
        FLightResult SpotResult = (FLightResult)0;
        CalculateSpotLight(SpotResult, Light, P, N, V, bIncludeSpecular);

        Result.Diffuse += SpotResult.Diffuse * Shadow;
        Result.Specular += SpotResult.Specular * Shadow;
    }
    [loop]
    for (uint RectIndex = 0; RectIndex < LightCounts.w; ++RectIndex)
    {
        CalculateRectLight(Result, SceneLights[LightOffsets.w + RectIndex], P, N, V, bIncludeSpecular);
    }
}

FLightResult CalculateTiledLighting(float3 PixelPosition, float3 P, float ViewDepth, float3 Normal, bool bIncludeSpecular)
{
    float3 N = normalize(Normal);
    float3 ToCamera = CameraPosition.xyz - P;
    float3 V = ToCamera * rsqrt(max(dot(ToCamera, ToCamera), 1.0e-8f));
    FLightResult Result = CalculateBaseLighting(PixelPosition, P, ViewDepth, N, V, bIncludeSpecular);

    if (GridMode == LIGHT_GRID_MODE_OFF)
    {
        AddAllLocalLights(Result, PixelPosition, P, N, V, bIncludeSpecular);
    }
    else
    {
        uint CellIndex = GetLightGridCellIndex(uint2(PixelPosition.xy), PixelPosition.z);
        uint4 Range = TileLightRanges[CellIndex];
        bool UsePointList = Range.x != 0xffffffffu;
        bool UseSpotList = Range.z != 0xffffffffu;
        uint PointCount = UsePointList ? Range.y : LightCounts.x;
        uint SpotCount = UseSpotList ? Range.w : LightCounts.y;

        [loop]
        for (uint PointListIndex = 0; PointListIndex < PointCount; ++PointListIndex)
        {
            uint LightIndex = UsePointList ? TilePointIndices[Range.x + PointListIndex] : PointListIndex;
            FLightRenderParameters Light = SceneLights[LightOffsets.x + LightIndex];
            float Shadow = GetLightShadow(Light, PixelPosition);
            
            FLightResult PointResult = (FLightResult)0;
            CalculatePointLight(PointResult, Light, P, N, V, bIncludeSpecular);
         
            Result.Diffuse += PointResult.Diffuse * Shadow;
            Result.Specular += PointResult.Specular * Shadow;
        }

        [loop]
        for (uint SpotListIndex = 0; SpotListIndex < SpotCount; ++SpotListIndex)
        {
            uint LightIndex = UseSpotList ? TileSpotIndices[Range.z + SpotListIndex] : SpotListIndex;
            FLightRenderParameters Light = SceneLights[LightOffsets.y + LightIndex];
            float Shadow = GetLightShadow(Light, PixelPosition);

            FLightResult SpotResult = (FLightResult)0;
            CalculateSpotLight(SpotResult, Light, P, N, V, bIncludeSpecular);

            Result.Diffuse += SpotResult.Diffuse * Shadow;
            Result.Specular += SpotResult.Specular * Shadow;
        }
    }

    return Result;
}

PSInput mainVS(VSInput input)
{
    PSInput Output;
    
    float4 WorldPosition = mul(float4(input.Position, 1.0f), World);
    
    Output.Position = mul(WorldPosition, ViewProjection);
    Output.Position.z -= 0.00001f * Output.Position.w; // Avoid z-fighting
    Output.DecalClipPosition = mul(WorldPosition, DecalViewProjection);
    Output.WorldNormal = mul(float4(input.Normal, 0.0f), World).xyz;
    Output.WorldPosition = WorldPosition.xyz;
    Output.WorldTangent.xyz = mul(float4(input.Tangent.xyz, 0.0f), World).xyz;
    Output.WorldTangent.w = input.Tangent.w;
    Output.ViewDepth = Output.Position.z / Output.Position.w;
    
    return Output;
}

float3 GetDecalShadingNormal(PSInput input, float2 DecalUV, float DecalAlpha)
{     
    float3 N = normalize(input.WorldNormal);
    float4 NormalSample = DecalNormalTexture.Sample(ClampSampler, DecalUV);
    
    // 노멀맵이 바인딩되어 있고 유효한 샘플인 경우 (0이 아닌 경우) TBN 변환
    if (dot(NormalSample.xyz, NormalSample.xyz) > 0.001f)
    {
        float3 MapNormal = NormalSample.xyz * 2.0f - 1.0f;
        float3 WorldTangent = input.WorldTangent.xyz - N * dot(N, input.WorldTangent.xyz);
        float TangentLenSq = dot(WorldTangent, WorldTangent);
        
        if (TangentLenSq > 1.0e-8f)
        {
            float3 T = WorldTangent * rsqrt(TangentLenSq);
            float3 B = cross(N, T) * input.WorldTangent.w;
            float3 MappedNormal = T * MapNormal.x + B * MapNormal.y + N * MapNormal.z;            
            float3 FinalShadingNormal = normalize(lerp(N, MappedNormal, DecalAlpha));
            float MappedLenSq = dot(FinalShadingNormal, FinalShadingNormal);
            if (MappedLenSq > 1.0e-8f)
            {
                N = FinalShadingNormal * rsqrt(MappedLenSq);
            }
        }
    }
    
    return N;
}

float4 mainPS(PSInput input) : SV_TARGET
{
    float3 DecalNDC = input.DecalClipPosition.xyz / input.DecalClipPosition.w;
    
    if (DecalNDC.x < -1.0f || DecalNDC.x > 1.0f || 
        DecalNDC.y < -1.0f || DecalNDC.y > 1.0f || 
        DecalNDC.z < 0.0f  || DecalNDC.z > 1.0f)
    {
        discard;
    }
    
    // Edge Z fading
    float Margin = 0.2f;    
    float FadeZ = smoothstep(1.0f, 1.0f - Margin, abs(DecalNDC.z * 2.0f - 1.0f));    
    
    float2 DecalUV;
    DecalUV.x = DecalNDC.x * 0.5f + 0.5f;
    DecalUV.y = -DecalNDC.y * 0.5f + 0.5f;
    
    // Albedo sample & sRGB to linear conversion
    float4 DecalColorSample = DecalTexture.Sample(ClampSampler, DecalUV);
    float3 DecalAlbedo = pow(max(DecalColorSample.rgb, 0.0f), 2.2f) * DecalColor.rgb;
    
    float3 FinalColor;
    float FinalAlpha = DecalColorSample.a * DecalColor.a * FadeZ;
    if (DecalForward.w > 0.5f)
    {
        // Emissive / Unlit decal (e.g., Fake Spot Light): shines even without scene lights
        FinalColor = DecalAlbedo;
    }
    else
    {
        // Perturbed surface normal from decal normal map
        float3 ShadingNormal = GetDecalShadingNormal(input, DecalUV, FinalAlpha);
        
        // Calculate full forward tiled lighting
        FLightResult LitResult = CalculateTiledLighting(input.Position.xyz, input.WorldPosition, input.ViewDepth, ShadingNormal, true);
        
        // Combine diffuse and specular
        FinalColor = (DecalAlbedo * LitResult.Diffuse) + LitResult.Specular;
    }       
    
    return float4(FinalColor, FinalAlpha);
}
