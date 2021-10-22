#ifndef _SSRBLUR_HLSL_
#define _SSRBLUR_HLSL_

#ifndef __SSR_HLSL__
#define __SSR_HLSL__

#include "SsrCommon.hlsl"

static const int gBlurRadius = 5;

struct VertexOut {
	float4 PosH  : SV_POSITION;
	float2 TexC  : TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout;

	vout.TexC = gTexCoords[vid];

	// Quad covering screen in NDC space.
	vout.PosH = float4(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y, 0.0f, 1.0f);

	return vout;
}

float NdcDepthToViewDepth(float z_ndc) {
	// z_ndc = A + B/viewZ, where gProj[2,2]=A and gProj[3,2]=B.
	float viewZ = gProj[3][2] / (z_ndc - gProj[2][2]);
	return viewZ;
}

float4 PS(VertexOut pin) : SV_Target{
	// unpack into float array.
	float blurWeights[12] = {
		gBlurWeights[0].x, gBlurWeights[0].y, gBlurWeights[0].z, gBlurWeights[0].w,
		gBlurWeights[1].x, gBlurWeights[1].y, gBlurWeights[1].z, gBlurWeights[1].w,
		gBlurWeights[2].x, gBlurWeights[2].y, gBlurWeights[2].z, gBlurWeights[2].w,
	};

	float2 texOffset;
	if (gHorizontalBlur)
		texOffset = float2(gInvRenderTargetSize.x, 0.0f);
	else
		texOffset = float2(0.0f, gInvRenderTargetSize.y);

	// The center value always contributes to the sum.
	float4 color = blurWeights[gBlurRadius] * gInputMap.SampleLevel(gsamPointClamp, pin.TexC, 0.0);
	float totalWeight = blurWeights[gBlurRadius];

	for (float i = -gBlurRadius; i <= gBlurRadius; ++i) {
		// We already added in the center weight.
		if (i == 0)
			continue;

		float2 tex = pin.TexC + i * texOffset;

		float weight = blurWeights[i + gBlurRadius];
				
		// Add neighbor pixel to blur.
		color += weight * gInputMap.SampleLevel(gsamPointClamp, tex, 0.0);

		totalWeight += weight;
	}

	// Compensate for discarded samples by making total weights sum to 1.
	return color / totalWeight;
}

#endif // __SSR_HLSL__

#endif // _SSRBLUR_HLSL_