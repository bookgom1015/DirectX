//***************************************************************************************
// Default.hlsl by Frank Luna (C) 2015 All Rights Reserved.
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
	float3 PosL			: POSITION;
    float3 NormalL		: NORMAL;
	float2 TexC			: TEXCOORD;
	float3 TangentL		: TANGENT;
#ifdef SKINNED
	float4 BoneWeights0	: BONEWEIGHTS0;
	float4 BoneWeights1	: BONEWEIGHTS1;
	int4 BoneIndices0	: BONEINDICES0;
	int4 BoneIndices1	: BONEINDICES1;
#endif
};

struct VertexOut {
	float4 PosH						: SV_POSITION;
    float4 ShadowPosH				: POSITION0;
    float4 SsaoPosH					: POSITION1;
    float3 PosW						: POSITION2;
    float3 NormalW					: NORMAL;
	float3 TangentW					: TANGENT;
	float2 TexC						: TEXCOORD;
	// nointerpolation is used so the index is
	//  not interpolated across the triangle.
	nointerpolation uint MatIndex	: MATINDEX;
};

VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID) {
	VertexOut vout = (VertexOut)0.0f;

	InstanceData instData = gInstanceData[gInstanceIndex * MAX_INSTANCE_DATA + instanceID];
	float4x4 world = instData.World;
	//	float4x4(1.0f, 0.0f, 0.0f, 0.0f,
	//		0.0f, 1.0f, 0.0f, 0.0f,
	//		0.0f, 0.0f, 1.0f, 0.0f,
	//		instanceID * 4.0f, 0.0f, 0.0f, 1.0f);
	float4x4 texTransform = instData.TexTransform;
	uint matIndex = instData.MaterialIndex;

	vout.MatIndex = matIndex;

	// Fetch the material data.
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
	float3 normalL = float3(0.0f, 0.0f, 0.0f);
	float3 tangentL = float3(0.0f, 0.0f, 0.0f);
	for(int i = 0; i < 8; ++i) {
		// Assume no nonuniform scaling when transforming normals, so 
		// that we do not have to use the inverse-transpose.
		if (indices[i] == -1)
			break;

		posL += weights[i] * mul(float4(vin.PosL, 1.0f), gBoneTransforms[indices[i]]).xyz;
		normalL += weights[i] * mul(vin.NormalL, (float3x3)gBoneTransforms[indices[i]]);
		tangentL += weights[i] * mul(vin.TangentL.xyz, (float3x3)gBoneTransforms[indices[i]]);
	}
	
	vin.PosL = posL;
	vin.NormalL = normalL;
	vin.TangentL.xyz = tangentL;
#endif
	
    // Transform to world space.
    float4 posW = mul(float4(vin.PosL, 1.0f), world);
    vout.PosW = posW.xyz;

    // Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
    vout.NormalW = mul(vin.NormalL, (float3x3)world);
	
	vout.TangentW = mul(vin.TangentL, (float3x3)world);

    // Transform to homogeneous clip space.
    vout.PosH = mul(posW, gViewProj);

    // Generate projective tex-coords to project SSAO map onto scene.
    vout.SsaoPosH = mul(posW, gViewProjTex);
	
	// Output vertex attributes for interpolation across triangle.
	float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), texTransform);
	vout.TexC = mul(texC, matData.MatTransform).xy;

    // Generate projective tex-coords to project shadow map onto scene.
    vout.ShadowPosH = mul(posW, gShadowTransform);
	
    return vout;
}

float4 PS(VertexOut pin) : SV_Target {
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

	float3 fresnelR0 = matData.FresnelR0;
	float  roughness = matData.Roughness;
	uint normalMapIndex = matData.NormalMapIndex;
	int specularMapIndex = matData.SpecularMapIndex;

	if (specularMapIndex != -1)
		fresnelR0 = gTextureMaps[specularMapIndex].Sample(gsamAnisotropicWrap, pin.TexC).rgb;

	// Interpolating normal can unnormalize it, so renormalize it.
    pin.NormalW = normalize(pin.NormalW);
	
    float4 normalMapSample = gTextureMaps[normalMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);
	float3 bumpedNormalW = NormalSampleToWorldSpace(normalMapSample.rgb, pin.NormalW, pin.TangentW);

	// Uncomment to turn off normal mapping.
    //bumpedNormalW = pin.NormalW;

    // Vector from point being lit to eye. 
    float3 toEyeW = normalize(gEyePosW - pin.PosW);

    // Finish texture projection and sample SSAO map.
    pin.SsaoPosH /= pin.SsaoPosH.w;
    float ambientAccess = gSsaoMap.Sample(gsamLinearClamp, pin.SsaoPosH.xy, 0.0f).r;

    // Light terms.
    float4 ambient = ambientAccess * gAmbientLight * diffuseAlbedo;

    // Only the first light casts a shadow.
    float3 shadowFactor = float3(1.0f, 1.0f, 1.0f);
    shadowFactor[0] = CalcShadowFactor(pin.ShadowPosH);
    const float shininess = (1.0f - roughness) * normalMapSample.a;
    Material mat = { diffuseAlbedo, fresnelR0, shininess };
    float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
        bumpedNormalW, toEyeW, shadowFactor);

    float4 litColor = ambient + directLight;

	// Add in specular reflections.
    float3 r = reflect(-toEyeW, bumpedNormalW);
	float3 lookup = BoxCubeMapLookup(pin.PosW, r, 0.0f, 100.0f);
    float4 reflectionColor = gBlurCubeMap.Sample(gsamLinearWrap, lookup);
    float3 fresnelFactor = SchlickFresnel(fresnelR0, bumpedNormalW, r);
    litColor.rgb += shininess * fresnelFactor * reflectionColor.rgb;
	
    // Common convention to take alpha from diffuse albedo.
    litColor.a = diffuseAlbedo.a;

	float4 rgb = gAnimationsDataMap.Sample(gsamPointClamp, float2(0.9f, 0.9f));
	litColor = rgb;

    return litColor;
}