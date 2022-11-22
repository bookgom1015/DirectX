#ifndef __MISS_HLSL__
#define __MISS_HLSL__

#include "RTXCommon.hlsl"

[shader("miss")]
void Miss(inout RayPayload payload) {
	float4 background = float4(0.941176534f, 0.972549081f, 1.0f, 1.0f);
	payload.Color = background;
}

#endif // __MISS_HLSL__