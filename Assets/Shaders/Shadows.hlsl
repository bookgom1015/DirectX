//***************************************************************************************
// Shadows.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

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

// Include common HLSL code.
#include "Common.hlsl"

struct VertexIn {
	float3 PosL				: POSITION;
	float2 TexC				: TEXCOORD;
#ifdef SKINNED
	float4 BoneWeights0		: BONEWEIGHTS0;
	float4 BoneWeights1		: BONEWEIGHTS1;
	int4 BoneIndices0		: BONEINDICES0;
	int4 BoneIndices1		: BONEINDICES1;
#endif
};

struct VertexOut {
	float4 PosH						: SV_POSITION;
	float2 TexC						: TEXCOORD;
	nointerpolation uint MatIndex	: MATINDEX;
};

VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID) {
	VertexOut vout = (VertexOut)0.0f;

	InstanceData instData = gInstanceData[gInstanceIndex * 32 + instanceID];
	float4x4 world = instData.World;
	float4x4 texTransform = instData.TexTransform;
	uint matIndex = instData.MaterialIndex;

	vout.MatIndex = matIndex;

	MaterialData matData = gMaterialData[matIndex];
	
#ifdef SKINNED
	float weights[8];
	weights[0] = vin.BoneWeights0.x;
	weights[1] = vin.BoneWeights0.y;
	weights[2] = vin.BoneWeights0.z;
	weights[3] = vin.BoneWeights0.w;
	weights[4] = vin.BoneWeights1.x;
	weights[5] = vin.BoneWeights1.y;
	weights[6] = vin.BoneWeights1.z;
	weights[7] = vin.BoneWeights1.w;

	int indices[8];
	indices[0] = vin.BoneIndices0.x;
	indices[1] = vin.BoneIndices0.y;
	indices[2] = vin.BoneIndices0.z;
	indices[3] = vin.BoneIndices0.w;
	indices[4] = vin.BoneIndices1.x;
	indices[5] = vin.BoneIndices1.y;
	indices[6] = vin.BoneIndices1.z;
	indices[7] = vin.BoneIndices1.w;
	
	float3 posL = float3(0.0f, 0.0f, 0.0f);
	for (int i = 0; i < 8; ++i) {
		// Assume no nonuniform scaling when transforming normals, so 
		// that we do not have to use the inverse-transpose.	
		if (indices[i] == -1)
			break;
		posL += weights[i] * mul(float4(vin.PosL, 1.0f), gBoneTransforms[indices[i]]).xyz;
	}
	
	vin.PosL = posL;
#endif

    // Transform to world space.
    float4 posW = mul(float4(vin.PosL, 1.0f), world);

    // Transform to homogeneous clip space.
    vout.PosH = mul(posW, gViewProj);

	// Output vertex attributes for interpolation across triangle.
	float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), texTransform);
	vout.TexC = mul(texC, matData.MatTransform).xy;
	
    return vout;
}

// This is only used for alpha cut out geometry, so that shadows 
// show up correctly.  Geometry that does not need to sample a
// texture can use a NULL pixel shader for depth pass.
void PS(VertexOut pin) {
	// Fetch the material data.
	MaterialData matData = gMaterialData[pin.MatIndex];
	float4 diffuseAlbedo = matData.DiffuseAlbedo;
    uint diffuseMapIndex = matData.DiffuseMapIndex;
	
	// Dynamically look up the texture in the array.
	diffuseAlbedo *= gTextureMaps[diffuseMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);

#ifdef ALPHA_TEST
    // Discard pixel if texture alpha < 0.1.  We do this test as soon 
    // as possible in the shader so that we can potentially exit the
    // shader early, thereby skipping the rest of the shader code.
    clip(diffuseAlbedo.a - 0.1f);
#endif
}


