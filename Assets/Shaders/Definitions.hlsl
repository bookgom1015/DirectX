#ifndef __DEFINITIONS_HLSL__
#define __DEFINITIONS_HLSL__

#ifndef NUM_DIR_LIGHTS
	#define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
	#define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
	#define NUM_SPOT_LIGHTS 0
#endif

#ifndef SSAO_ENABLED
	#define SSAO_ENABLED 1 << 0
#endif

#ifndef SSR_ENABLED
	#define SSR_ENABLED 1 << 1
#endif

#ifndef BLOOM_ENABLED
	#define BLOOM_ENABLED 1 << 2
#endif

#endif // __DEFINITIONS_HLSL__