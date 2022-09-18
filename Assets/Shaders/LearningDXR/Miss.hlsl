#ifndef __MISS_HLSL__
#define __MISS_HLSL__

#include "RTXCommon.hlsl"

[shader("miss")]
void Miss(inout RayPayload payload) {
	float4 background = float4(0.941176534, 0.972549081, 1.0, 1.0);
	payload.Color = background;
}

#endif // __MISS_HLSL__