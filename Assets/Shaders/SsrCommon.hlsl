#ifndef __SSRCOMMON_HLSL__
#define __SSRCOMMON_HLSL__

cbuffer cbSsr : register(b0) {
	float4x4	gInvView;
	float4x4	gProj;
	float4x4	gInvProj;
	float4x4	gViewProj;
	float3		gEyePosW;
	// For SsrBlur.hlsl
	float4		gBlurWeights[3];
	float2		gInvRenderTargetSize;
};

cbuffer cbRootConstants : register(b1) {
	bool gHorizontalBlur;
};

// Nonnumeric values cannot be added to a cbuffer.
Texture2D		gDiffuseMap			: register(t0);
Texture2D		gNormalMap			: register(t1);
Texture2D		gDepthMap			: register(t2);
Texture2D		gInputMap			: register(t3);

SamplerState	gsamPointClamp		: register(s0);
SamplerState	gsamLinearClamp		: register(s1);
SamplerState	gsamDepthMap		: register(s2);
SamplerState	gsamLinearWrap		: register(s3);

static const float2 gTexCoords[6] = {
	float2(0.0f, 1.0f),
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f),
	float2(0.0f, 1.0f),
	float2(1.0f, 0.0f),
	float2(1.0f, 1.0f)
};

#endif // __SSRCOMMON_HLSL__