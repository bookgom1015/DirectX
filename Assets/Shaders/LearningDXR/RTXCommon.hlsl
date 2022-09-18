#ifndef __RTXCOMMON_HLSL__
#define __RTXCOMMON_HLSL__

typedef BuiltInTriangleIntersectionAttributes Attributes;

struct RayPayload {
	float4 Color;
};

struct Vertex {
	float3 Pos;
	float3 Normal;
	float2 TexC;
	float3 Tangent;
};

//
// Global root signatures
//
RWTexture2D<float4>				gOutput		: register(u0);
RaytracingAccelerationStructure	gBVH		: register(t0);
StructuredBuffer<Vertex>		gVertices	: register(t1);
ByteAddressBuffer				gIndices	: register(t2);

cbuffer PassCB	: register(b0) {
	float4x4 gProjToWorld;
	float4 gEyePosW;
	float4 gLightPosW;
	float4 gLightAmbientColor;
	float4 gLightDiffuseColor;
};

//
// Local root signatures
//
cbuffer lObjectCB : register(b1) {
	float4 lAlbedo;
};

//
// Helper functions
//
// Load three 32 bit indices from a byte addressed buffer.
uint3 Load3x32BitIndices(uint offsetBytes) {
	return gIndices.Load3(offsetBytes);
}

// Retrieve hit world position.
float3 HitWorldPosition() {
	return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}

// Retrieve attribute at a hit position interpolated from vertex attributes using the hit's barycentrics.
float3 HitAttribute(float3 vertexAttribute[3], Attributes attr) {
	return normalize(vertexAttribute[0] + attr.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) + attr.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]));
}

// Generate a ray in world space for a camera pixel corresponding to an index from the dispatched 2D grid.
inline void GenerateCameraRay(uint2 index, out float3 origin, out float3 direction) {
	float2 xy = index + 0.5f; // center in the middle of the pixel.
	float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0 - 1.0;

	// Invert Y for DirectX-style coordinates.
	screenPos.y = -screenPos.y;

	// Unproject the pixel coordinate into a ray.
	float4 world = mul(float4(screenPos, 0, 1), gProjToWorld);

	world.xyz /= world.w;
	origin = gEyePosW.xyz;
	direction = normalize(world.xyz - origin);
}

// Diffuse lighting calculation.
float4 CalculateDiffuseLighting(float3 hitPosition, float3 normal) {
	float3 pixelToLight = normalize(gLightPosW.xyz - hitPosition);

	// Diffuse contribution.
	float fNDotL = max(0.0, dot(pixelToLight, normal));

	return lAlbedo * gLightDiffuseColor * fNDotL;
}

#endif // __RTXCOMMON_HLSL__