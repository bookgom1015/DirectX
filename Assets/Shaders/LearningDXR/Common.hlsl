#ifndef __COMMON_HLSL__
#define __COMMON_HLSL__

#include "LightingUtil.hlsl"

RWTexture2D<float4> gUAV : register(u0);

cbuffer cbPerObject : register(b0) {
	float4x4 gWorld;
	float4x4 gTexTransform;
};

cbuffer cbPass : register(b1) {
	float4x4	gView;
	float4x4	gInvView;
	float4x4	gProj;
	float4x4	gInvProj;
	float4x4	gViewProj;
	float4x4	gInvViewProj;
	float3		gEyePosW;
	float		cbPerObjectPad1;
	float4		gAmbientLight;
	Light gLights[MaxLights];
};

cbuffer cbMaterial : register(b2) {
	float4		gDiffuseAlbedo;
	float3		gFresnelR0;
	float		gRoughness;
	float4x4	gMatTransform;
};

SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

#endif // __COMMON_HLSL__