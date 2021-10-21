//***************************************************************************************
// Default.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

#ifndef __DEFAULT_HLSL__
#define __DEFAULT_HLSL__

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
	float3 PosL		: POSITION;
	float2 TexC		: TEXCOORD;
};

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float3 PosV		: POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID) {
	VertexOut vout = (VertexOut)0.0f;

	vout.TexC = vin.TexC;

	float3 pos = vin.PosL * 2.0f + float3(-1.0f, 1.0f, 0.0f);

	// Already in homogeneous clip space.
	vout.PosH = float4(pos, 1.0f);

	// Transform quad corners to view space near plane.
	float4 ph = mul(vout.PosH, gInvProj);
	vout.PosV = ph.xyz / ph.w;

	return vout;
}

float4 PS(VertexOut pin) : SV_Target{
	// Get viewspace normal and z-coord of this pixel.  
	//float3 n = normalize(gNormalMap.Sample(gsamPointClamp, pin.TexC).xyz);
	float pz = gDepthMap.Sample(gsamDepthMap, pin.TexC).r;
	pz = NdcDepthToViewDepth(pz);

	//
	// Reconstruct full view space position (x,y,z).
	// Find t such that p = t*pin.PosV.
	// p.z = t*pin.PosV.z
	// t = p.z / pin.PosV.z
	//
	float3 posV = (pz / pin.PosV.z) * pin.PosV;
	float4 posW = mul(float4(posV, 1.0f), gInvView);

	float4 diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC);

	// Sample SSAO map.
	float ambientAccess = gSsaoMap.Sample(gsamLinearWrap, pin.TexC, 0.0f).r;
	
	// Light terms. 
	float4 ambient = ambientAccess * gAmbientLight * diffuseAlbedo;

	// Only the first light casts a shadow.
	float3 shadowFactor = float3(1.0f, 1.0f, 1.0f);
	float4 shadowPosH = mul(posW, gShadowTransform);
	shadowFactor[0] = CalcShadowFactor(shadowPosH);

	float4 normalMapSample = gNormalMap.Sample(gsamAnisotropicWrap, pin.TexC);
	float3 normalW = normalize(normalMapSample.xyz);

	const float4 specMapSample = gSpecularMap.Sample(gsamLinearWrap, pin.TexC);

	const float roughness = specMapSample.a;
	const float shininess = (1.0f - roughness) * normalMapSample.a;
	const float3 fresnelR0 = specMapSample.rgb;

	Material mat = { diffuseAlbedo, fresnelR0, shininess };
	 
	// Vector from point being lit to eye. 
	float3 toEyeW = normalize(gEyePosW - posW.xyz);
	float4 directLight = ComputeLighting(gLights, mat, posW.xyz, normalW, toEyeW, shadowFactor);
	
	float4 litColor = ambient + directLight;	

	// Add in specular reflections.
	float3 r = reflect(-toEyeW, normalW);
	float3 lookup = BoxCubeMapLookup(posW.xyz, r, gCubeMapCenter, gCubeMapExtents);

	float4 reflectionSample = gReflectionMap.Sample(gsamLinearWrap, pin.TexC);

	float4 reflectionColor = reflectionSample.a * reflectionSample + (1.0f - reflectionSample.a) * gBlurCubeMap.Sample(gsamLinearWrap, lookup);
	float3 fresnelFactor = SchlickFresnel(fresnelR0, normalW, r);
	litColor.rgb += shininess * fresnelFactor * reflectionColor.rgb;
	
	// Common convention to take alpha from diffuse albedo.
	litColor.a = diffuseAlbedo.a;
	
	return litColor;
	//return GetReflectionColor(posW.xyz, r);
}

#endif