//***************************************************************************************
// Default.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

#ifndef __COMMON_HLSL__
#define __COMMOM_HLSL__

// Defaults for number of lights.
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

// Include structures and functions for lighting.
#include "LightingUtil.hlsl"

struct InstanceIdxData {
	uint		InstIdx;
	uint		InstIdxPad0;
	uint		InstIdxPad1;
	uint		InstIdxPad2;
};

struct InstanceData {
	float4x4	World;
	float4x4	TexTransform;
	float		TimePos;
	uint		MaterialIndex;
	int			AnimClipIndex;
	uint		InstPad0;
};

struct MaterialData {
	float4		DiffuseAlbedo;
	float3		FresnelR0;
	float		Roughness;
	float4x4	MatTransform;
	uint		DiffuseMapIndex;
	uint		NormalMapIndex;
	int			SpecularMapIndex;
	int			DispMapIndex;
	int			AlphaMapIndex;
	uint		MatPad0;
	uint		MatPad1;
	uint		MatPad2;
};

TextureCube				gCubeMap					: register(t0);
TextureCube				gBlurCubeMap				: register(t1);
Texture2D				gMainPassMap1				: register(t2);
Texture2D				gMainPassMap2				: register(t3);
Texture2D				gDiffuseMap					: register(t4);
Texture2D				gNormalMap					: register(t5);
Texture2D				gDepthMap					: register(t6);
Texture2D				gSpecularMap				: register(t7);
Texture2D				gShadowMap					: register(t8);
Texture2D				gSsaoMap					: register(t9);
Texture2D				gSsrMap						: register(t10);

// An array of textures, which is only supported in shader model 5.1+.  Unlike Texture2DArray, the textures
// in this array can be different sizes and formats, making it more flexible than texture arrays.
Texture2D				gTextureMaps[64]			: register(t11);

Texture2D				gAnimationsDataMap			: register(t75);

// Put in space1, so the texture array does not overlap with these resources.  
// The texture array will occupy registers t0, t1, ..., t3 in space0. 
StructuredBuffer<InstanceIdxData>	gInstIdxData	: register(t0, space1);
StructuredBuffer<InstanceData>		gInstanceData	: register(t1, space1);
StructuredBuffer<MaterialData>		gMaterialData	: register(t2, space1);

SamplerState			gsamPointWrap				: register(s0);
SamplerState			gsamPointClamp				: register(s1);
SamplerState			gsamLinearWrap				: register(s2);
SamplerState			gsamLinearClamp				: register(s3);
SamplerState			gsamAnisotropicWrap			: register(s4);
SamplerState			gsamAnisotropicClamp		: register(s5);
SamplerComparisonState	gsamShadow					: register(s6);
SamplerState			gsamDepthMap				: register(s7);

// Constant data that varies per frame.
cbuffer cbPerObject : register(b0) {
	uint gObjectIndex;
	uint gObjectPad0;
	uint gObjectPad1;
	uint gObjectPad2;
};

// Constant data that varies per material.
cbuffer cbPass : register(b1) {
	float4x4	gView;
	float4x4	gInvView;
	float4x4	gProj;
	float4x4	gInvProj;
	float4x4	gViewProj;
	float4x4	gInvViewProj;
	float4x4	gViewProjTex;
	float4x4	gShadowTransform;
	float3		gEyePosW;
	float		cbPerObjectPad1;
	float2		gRenderTargetSize;
	float2		gInvRenderTargetSize;
	float		gNearZ;
	float		gFarZ;
	float		gTotalTime;
	float		gDeltaTime;
	float4		gAmbientLight;

	// Indices [0, NUM_DIR_LIGHTS) are directional lights;
	// indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
	// indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
	// are spot lights for a maximum of MaxLights per object.
	Light gLights[MaxLights];
};

cbuffer cbRootConstants : register(b2) {
	uint	gMaxInstanceCount;
	uint	gEffectEnabled;
};

//---------------------------------------------------------------------------------------
// Transforms a normal map sample to world space.
//---------------------------------------------------------------------------------------
float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW) {
	// Uncompress each component from [0,1] to [-1,1].
	float3 normalT = 2.0f * normalMapSample - 1.0f;

	// Build orthonormal basis.
	float3 N = unitNormalW;
	float3 T = normalize(tangentW - dot(tangentW, N)*N);
	float3 B = cross(N, T);

	float3x3 TBN = float3x3(T, B, N);

	// Transform from tangent space to world space.
	float3 bumpedNormalW = mul(normalT, TBN);

	return bumpedNormalW;
}

//---------------------------------------------------------------------------------------
// PCF for shadow mapping.
//---------------------------------------------------------------------------------------
float CalcShadowFactor(float4 shadowPosH) {
	// Complete projection by doing division by w.
	shadowPosH.xyz /= shadowPosH.w;

	// Depth in NDC space.
	float depth = shadowPosH.z;

	uint width, height, numMips;
	gShadowMap.GetDimensions(0, width, height, numMips);

	// Texel size.
	float dx = 1.0f / (float)width;

	float percentLit = 0.0f;
#if defined(BLUR_RADIUS_2)
	float twodx = dx + dx;

	const float2 offsets[25] = {
		float2(-twodx, -twodx), float2(-dx, -twodx), float2(0.0f, -twodx), float2(dx,  -twodx), float2(twodx,  -twodx),
		float2(-twodx, -dx),    float2(-dx, -dx),    float2(0.0f, -dx),    float2(dx,  -dx),    float2(twodx,  -dx),
		float2(-twodx, 0.0f),   float2(-dx, 0.0f),   float2(0.0f, 0.0f),   float2(dx, 0.0f),    float2(twodx, 0.0f),
		float2(-twodx, dx),     float2(-dx, dx),     float2(0.0f, dx),     float2(dx,  dx),     float2(twodx,  dx),
		float2(-twodx, twodx),  float2(-dx, twodx),  float2(0.0f, twodx),  float2(dx,  twodx),  float2(twodx,  twodx)
	};

	[unroll]
	for (int i = 0; i < 25; ++i) {
		percentLit += gShadowMap.SampleCmpLevelZero(gsamShadow,
			shadowPosH.xy + offsets[i], depth).r;
	}

	return percentLit / 25.0f;
#elif defined(BLUR_RADIUS_3)
	float twodx = dx + dx;
	float thrdx = dx + dx + dx;

	const float2 offsets[49] = {
		float2(-thrdx, -thrdx), float2(-twodx, -thrdx), float2(-dx, -thrdx), float2(0.0f, -thrdx), float2(dx,  -thrdx), float2(twodx,  -thrdx),	float2(thrdx, -thrdx),
		float2(-thrdx, -twodx), float2(-twodx, -twodx),	float2(-dx, -twodx), float2(0.0f, -twodx), float2(dx,  -twodx), float2(twodx,  -twodx),	float2(thrdx, -twodx),
		float2(-thrdx, -dx),	float2(-twodx, -dx),    float2(-dx, -dx),    float2(0.0f, -dx),    float2(dx,  -dx),    float2(twodx,  -dx),	float2(thrdx, -dx),
		float2(-thrdx, 0.0f),	float2(-twodx, 0.0f),   float2(-dx, 0.0f),   float2(0.0f, 0.0f),   float2(dx, 0.0f),    float2(twodx,  0.0f),	float2(thrdx, 0.0f),
		float2(-thrdx, dx),		float2(-twodx, dx),     float2(-dx, dx),     float2(0.0f, dx),     float2(dx,  dx),     float2(twodx,  dx),		float2(thrdx, dx),
		float2(-thrdx, twodx),	float2(-twodx, twodx),  float2(-dx, twodx),  float2(0.0f, twodx),  float2(dx,  twodx),  float2(twodx,  twodx),	float2(thrdx, twodx),
		float2(-thrdx, thrdx),	float2(-twodx, thrdx),  float2(-dx, thrdx),  float2(0.0f, thrdx),  float2(dx,  thrdx),  float2(twodx,  thrdx),	float2(thrdx, thrdx)
	};

	[unroll]
	for (int i = 0; i < 49; ++i) {
		percentLit += gShadowMap.SampleCmpLevelZero(gsamShadow,
			shadowPosH.xy + offsets[i], depth).r;
	}

	return percentLit / 49.0f;
#else
	const float2 offsets[9] = {
		float2(-dx,  -dx), float2(0.0f,  -dx), float2(dx,  -dx),
		float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
		float2(-dx,   dx), float2(0.0f,  +dx), float2(dx,  +dx)
	};

	[unroll]
	for (int i = 0; i < 9; ++i) {
		percentLit += gShadowMap.SampleCmpLevelZero(gsamShadow,
			shadowPosH.xy + offsets[i], depth).r;
	}

	return percentLit / 9.0f;
#endif // defined(BLUR_RADIUS_2)
}

//---------------------------------------------------------------------------------------
// Slap method
//---------------------------------------------------------------------------------------
float3 BoxCubeMapLookup(float3 rayOrigin, float3 unitRayDir, float3 boxCenter, float3 boxExtents) {
	// Set the origin to box of center.
	float3 p = rayOrigin - boxCenter;

	// The formula for AABB's i-th plate ray versus plane intersection is as follows.
	//
	// t1 = (-dot(n_i, p) + h_i) / dot(n_i, d) = (-p_i + h_i) / d_i
	// t2 = (-dot(n_i, p) - h_i) / dot(n_i, d) = (-p_i - h_i) / d_i
	float3 t1 = (-p + boxExtents) / unitRayDir;
	float3 t2 = (-p - boxExtents) / unitRayDir;

	// Find the maximum value for each coordinate component.
	// We assume that ray is inside the box, so we only need to find the maximum value of the intersection parameter. 
	float3 tmax = max(t1, t2);

	// Find the minimum value of all components for tmax.
	float t = min(min(tmax.x, tmax.y), tmax.z);

	// To use a lookup vector for a cube map, 
	// create coordinate relative to center of box.
	return p + t * unitRayDir;
}

float NdcDepthToViewDepth(float z_ndc) {
	// z_ndc = A + B/viewZ, where gProj[2,2]=A and gProj[3,2]=B.
	float viewZ = gProj[3][2] / (z_ndc - gProj[2][2]);
	return viewZ;
}

#endif // __COMMON_HLSL__