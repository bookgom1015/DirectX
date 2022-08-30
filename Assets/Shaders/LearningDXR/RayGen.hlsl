#ifndef __RAYGEN_HLSL__
#define __RAYGEN_HLSL__

#include "RTXCommon.hlsl"

[shader("raygeneration")]
void RayGen() {
	uint2 launchIndex = DispatchRaysIndex().xy;
	uint2 launchDimensions = DispatchRaysDimensions().xy;

	float2 ndc = float2(((launchIndex.xy + 0.5f) / gResolution.xy) * 2.0f - 1.0f);
	float aspectRatio = (gResolution.x / gResolution.y);
	
	//float2 vPos = float2(ndc.x * aspectRatio, ndc.y);	
	//float2 vPosPrime = float2(vPos.x / gProj[0][0], vPos.y / gProj[1][1]);	
	//float2 wPos = mul(float4(vPosPrime.x, vPosPrime.y, 1.0f, 1.0f), gInvView);
	//
	//float4 aaa = mul(float4(ndc, 1.0f, 1.0f), gInvViewProj);
	//float3 wPos = aaa.xyz / aaa.w;
	//
	//float2 vPos = (ndc.x * aspectRatio, ndc.y);
	//float2 vPosPrime = (vPos.x * gProj[0][0], vPos.y * gProj[1][1]);
	//float3 wPos = mul(float4(vPosPrime, 1.0f, 1.0f), gInvView).xyz;

	RayDesc ray;
	ray.Origin = float3(ndc.x, -ndc.y, -15.0f);
	ray.Direction = float3(0.0f, 0.0f, 1.0f);
	ray.TMin = 0.1f;
	ray.TMax = 1000.0f;

	// Trace the ray
	HitInfo payload;
	payload.ShadedColorAndHitT = float4(0.0f, 0.0f, 0.0f, 0.0f);

	TraceRay(
		gSceneBVH,
		RAY_FLAG_NONE,
		0xFF,
		0,
		0,
		0,
		ray,
		payload);

	gRTOutput[launchIndex.xy] = float4(payload.ShadedColorAndHitT.rgb, 1.0f);
}

#endif // __RAYGEN_HLSL__