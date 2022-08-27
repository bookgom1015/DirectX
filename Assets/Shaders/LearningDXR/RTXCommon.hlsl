#ifndef __RTXCOMMON_HLSL__
#define __RTXCOMMON_HLSL__

RWTexture2D<float4> RTOutput				: register(u0);
RaytracingAccelerationStructure SceneBVH	: register(t0);
//ByteAddressBuffer Vertices				: register(t1);
//ByteAddressBuffer Indices					: register(t2);

struct HitInfo {
	float4 ShadedColorAndHitT;
};

struct Attributes {
	float2 TexC;
};

cbuffer cbObject : register(b0) {
	float4x4	gWorld;
};

struct VertexAttributes {
	float3 PosL;
	float2 TexC;
};

uint3 GetIndices(uint triangleIndex) {
	uint baseIndex = (triangleIndex * 3);
	int address = (baseIndex * 4);
	return Indices.Load3(address);
}

VertexAttributes GetVertexAttributes(uint triangleIndex, float3 barycentrics) {
	uint3 indices = GetIndices(triangleIndex);
	VertexAttributes v;
	v.PosL = float3(0, 0, 0);
	v.TexC = float2(0, 0);

	for (uint i = 0; i < 3; ++i) {
		int address = (indices[i] * 5) * 4;
		v.PosL += asfloat(Vertices.Load3(address)) * barycentrics[i];
		address += (3 * 4);
		v.TexC += asfloat(Vertices.Load2(address)) * barycentrics[i];
	}

	return v;
}

#endif // __RTXCOMMON_HLSL__