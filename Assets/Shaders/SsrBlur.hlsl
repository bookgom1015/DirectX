#ifndef _SSRBLUR_HLSL_
#define _SSRBLUR_HLSL_

#include "SsrCommon.hlsl"

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

float4 PS(VertexOut pin) : SV_Target{
	// unpack into float array.
	float blurWeights[20] = {
		gBlurWeights[0].x, gBlurWeights[0].y, gBlurWeights[0].z, gBlurWeights[0].w,
		gBlurWeights[1].x, gBlurWeights[1].y, gBlurWeights[1].z, gBlurWeights[1].w,
		gBlurWeights[2].x, gBlurWeights[2].y, gBlurWeights[2].z, gBlurWeights[2].w,
		gBlurWeights[3].x, gBlurWeights[3].y, gBlurWeights[3].z, gBlurWeights[3].w,
		gBlurWeights[4].x, gBlurWeights[4].y, gBlurWeights[4].z, gBlurWeights[4].w,
	};

	float2 texOffset;
	if (gHorizontalBlur)
		texOffset = float2(gInvRenderTargetSize.x, 0.0f);
	else
		texOffset = float2(0.0f, gInvRenderTargetSize.y);

	// The center value always contributes to the sum.
	float4 color = blurWeights[gBlurRadius] * gInputMap.SampleLevel(gsamPointClamp, pin.TexC, 0.0f);
	float totalWeight = blurWeights[gBlurRadius];

	for (int idx = -gBlurRadius; idx <= gBlurRadius; ++idx) {
		// We already added in the center weight.
		if (idx == 0)
			continue;

		float2 tex = pin.TexC + idx * texOffset;

		float weight = blurWeights[idx + gBlurRadius];
				
		// Add neighbor pixel to blur.
		color += weight * gInputMap.SampleLevel(gsamPointClamp, tex, 0.0f);

		totalWeight += weight;
	}

	// Compensate for discarded samples by making total weights sum to 1.
	return color / totalWeight;
}

#endif // _SSRBLUR_HLSL_