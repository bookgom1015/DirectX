#ifndef __RTXCOMMON_HLSL__
#define __RTXCOMMON_HLSL__

//
// Global root signature
//
RWTexture2D<float4> gRTOutput				: register(u0);
RaytracingAccelerationStructure gSceneBVH	: register(t0);
ByteAddressBuffer gVertices					: register(t1);
ByteAddressBuffer gIndices					: register(t2);

cbuffer cbPass : register(b0) {
	float2	gResolution;
	float	gPassConstantPad0;
	float	gPassConstantPad1;
}

//
// Local root signature
//
cbuffer cbMaterial : register(b1) {
	float4		lDiffuseAlbedo;
	float3		lFresnelR0;
	float		lRoughness;
	float4x4	lMatTransform;
};

//
// Shader attributes
//
struct HitInfo {
	float4 ShadedColorAndHitT;
};

struct Attributes {
	float2 TexC;
};

struct VertexAttributes {
	float3 PosL;
	float2 TexC;
};

uint3 GetIndices(uint triangleIndex) {
	uint baseIndex = (triangleIndex * 3);
	int address = (baseIndex * 4);
	return gIndices.Load3(address);
}

VertexAttributes GetVertexAttributes(uint triangleIndex, float3 barycentrics) {
	uint3 indices = GetIndices(triangleIndex);
	VertexAttributes v;
	v.PosL = float3(0, 0, 0);
	v.TexC = float2(0, 0);

	for (uint i = 0; i < 3; ++i) {
		int address = (indices[i] * 5) * 4;
		v.PosL += asfloat(gVertices.Load3(address)) * barycentrics[i];
		address += (3 * 4);
		v.TexC += asfloat(gVertices.Load2(address)) * barycentrics[i];
	}

	return v;
}

#endif // __RTXCOMMON_HLSL__