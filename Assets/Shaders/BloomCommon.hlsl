#ifndef __BLOOMCOMMON_HLSL__
#define __BLOOMCOMMON_HLSL__

static const float2 gTexCoords[6] = {
	float2(0.0f, 1.0f),
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f),
	float2(0.0f, 1.0f),
	float2(1.0f, 0.0f),
	float2(1.0f, 1.0f)
};

cbuffer cbSsr : register(b0) {
	float4		gBlurWeights[9];
	float2		gInvRenderTargetSize;
	int			gBlurRadius;
	int			gConstantPad0;
};

cbuffer cbRootConstants : register(b1) {
	bool	gHorizontalBlur;
};

Texture2D		gMainPassMap1		: register(t0);
Texture2D		gMainPassMap2		: register(t1);
Texture2D		gBlurSourceMap		: register(t2);

SamplerState	gsamPointClamp		: register(s0);
SamplerState	gsamLinearClamp		: register(s1);
SamplerState	gsamDepthMap		: register(s2);
SamplerState	gsamLinearWrap		: register(s3);

#endif // __BLOOMCOMMON_HLSL__