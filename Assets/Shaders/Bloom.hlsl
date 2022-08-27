#ifndef __BLOOM_HLSL__
#define __BLOOM_HLSL__

#include "BloomCommon.hlsl"

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout;

	vout.TexC = gTexCoords[vid];

	// Quad covering screen in NDC space.
	vout.PosH = float4(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y, 0.0f, 1.0f);

	return vout;
}

float4 PS(VertexOut pin) : SV_Target{
	float4 color = gMainPassMap1.Sample(gsamPointClamp, pin.TexC) + float4(gMainPassMap2.Sample(gsamPointClamp, pin.TexC).rgb, 0.0f);

	float brightness = dot(color.rgb, float3(0.2126f, 0.7152f, 0.0722f));
	if (brightness > 0.99f) return color;
	else return (float4)0.0f;
}

#endif // __BLOOM_HLSL__