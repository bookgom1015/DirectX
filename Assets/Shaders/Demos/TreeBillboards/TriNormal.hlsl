//***************************************************************************************
// Default.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//
// Default shader, currently supports lighting.
//***************************************************************************************

// Defaults for number of lights.
#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

// Include structures and functions for lighting.
#include "LightingUtil.hlsl"

Texture2D    gDiffuseMap : register(t0);


SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

// Constant data that varies per frame.
cbuffer cbPerObject : register(b0) {
    float4x4 gWorld;
	float4x4 gTexTransform;
};

// Constant data that varies per material.
cbuffer cbPass : register(b1) {
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;

	float4 gFogColor;
	float gFogStart;
	float gFogRange;
	float2 cbPerObjectPad2;

    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light gLights[MaxLights];
};

cbuffer cbMaterial : register(b2) {
	float4   gDiffuseAlbedo;
    float3   gFresnelR0;
    float    gRoughness;
	float4x4 gMatTransform;
};

struct VertexIn {
	float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
};

struct VertexOut {
	float4 PosH    : POSITION_H;
    float3 PosW    : POSITION_W;
    float3 NormalW : NORMAL;
	float2 TexC    : TEXCOORD;
};

struct GeoOut {
	float4 PosH    : SV_POSITION;
	float3 PosW    : POSITION;
	float3 NormalW : NORMAL;
	float2 TexC    : TEXCOORD;
};

VertexOut VS(VertexIn vin) {
	VertexOut vout = (VertexOut)0.0f;
	
    vout.PosW = vin.PosL;
	vout.NormalW = vin.NormalL;
	vout.TexC = vin.TexC;

    return vout;
}

[maxvertexcount(5)]
void GS(triangle VertexOut gin[3],
	inout LineStream<GeoOut> lineStream) {
	GeoOut gout[5];

	for (int i = 0; i < 3; i++) {
		float4 posW = mul(float4(gin[i].PosW, 1.0f), gWorld);
		gout[i].PosW = mul(posW, gWorld).xyz;
		gout[i].PosH = mul(posW, gViewProj);
		gout[i].NormalW = mul(gin[i].NormalW, (float3x3)gWorld);
		float4 texC = mul(float4(gin[i].TexC, 0.0f, 1.0f), gTexTransform);
		gout[i].TexC = mul(texC, gMatTransform).xy;

		lineStream.Append(gout[i]);
	}

	float3 center = 0.5f * (gin[0].PosW + gin[1].PosW);
	center = 0.5f * (center + gin[2].PosW);

	float3 triNormal = 0.5f * (gin[0].NormalW + gin[1].NormalW);
	triNormal = 0.5f * (triNormal + gin[2].NormalW);

	lineStream.RestartStrip();

	float4 posW = mul(float4(center, 1.0f), gWorld);
	gout[3].PosW = posW.xyz;
	gout[3].PosH = mul(posW, gViewProj);
	gout[3].NormalW = mul(triNormal, (float3x3)gWorld);
	gout[3].TexC = float2(0.0f, 0.0f);

	lineStream.Append(gout[3]);

	posW = mul(float4(center + triNormal * 0.5f, 1.0f), gWorld);
	gout[4].PosW = posW.xyz;
	gout[4].PosH = mul(posW, gViewProj);
	gout[4].NormalW = mul(triNormal, (float3x3)gWorld);
	gout[4].TexC = float2(0.0f, 0.0f);

	lineStream.Append(gout[4]);
}

float4 PS(GeoOut pin) : SV_Target {
    float4 diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC) * gDiffuseAlbedo;
	
#ifdef ALPHA_TEST
	// Discard pixel if texture alpha < 0.1.  We do this test as soon 
	// as possible in the shader so that we can potentially exit the
	// shader early, thereby skipping the rest of the shader code.
	clip(diffuseAlbedo.a - 0.1f);
#endif

    // Interpolating normal can unnormalize it, so renormalize it.
    pin.NormalW = normalize(pin.NormalW);

    // Vector from point being lit to eye. 
	float3 toEyeW = gEyePosW - pin.PosW;
	float distToEye = length(toEyeW);
	toEyeW /= distToEye; // normalize

    // Light terms.
    float4 ambient = gAmbientLight*diffuseAlbedo;

    const float shininess = 1.0f - gRoughness;
    Material mat = { diffuseAlbedo, gFresnelR0, shininess };
    float3 shadowFactor = 1.0f;
    float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
        pin.NormalW, toEyeW, shadowFactor);

    float4 litColor = ambient + directLight;

#ifdef FOG
	float fogAmount = saturate((distToEye - gFogStart) / gFogRange);
	litColor = lerp(litColor, gFogColor, fogAmount);
#endif

    // Common convention to take alpha from diffuse albedo.
    litColor.a = diffuseAlbedo.a;

    //return litColor;
	return float4(1.0f, 1.0f, 1.0f, 1.0f);
}


