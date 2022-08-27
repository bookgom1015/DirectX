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
	float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
	float3 TangentU : TANGENT;
};

struct VertexOut {
	float3 PosL    : POSITION;
	float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
	float3 TangentU : TANGENT;
};

VertexOut VS(VertexIn vin) {
	VertexOut vout = (VertexOut)0.0f;

	vout.PosL = vin.PosL;
	vout.NormalL = vin.NormalL;
	vout.TexC = vin.TexC;
	vout.TangentU = vin.TangentU;
	
	MaterialData matData = gMaterialData[gMaterialIndex];

    return vout;
}

struct PatchTess {
	float EdgeTess[3]   : SV_TessFactor;
	float InsideTess[1] : SV_InsideTessFactor;
};

PatchTess ConstantHS(InputPatch<VertexOut, 3> patch, uint patchID : SV_PrimitiveID)
{
	PatchTess pt;

	// Uniformly tessellate the patch.

	pt.EdgeTess[0] = 25;
	pt.EdgeTess[1] = 25;
	pt.EdgeTess[2] = 25;

	pt.InsideTess[0] = 25;

	return pt;
}

struct HullOut {
	float3 PosL    : POSITION;
	float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
	float3 TangentU : TANGENT;
};

[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64.0f)]
HullOut HS(InputPatch<VertexOut, 3> p,
			uint i : SV_OutputControlPointID,
			uint patchId : SV_PrimitiveID) {
	HullOut hout = (HullOut)0.0f;

	hout.PosL = p[i].PosL;
	hout.NormalL = p[i].NormalL;
	hout.TexC = p[i].TexC;
	hout.TangentU = p[i].TangentU;

	return hout;
}

struct DomainOut {
	float4 PosH    : SV_POSITION;
	float3 PosW    : POSITION;
	float3 NormalW : NORMAL;
	float3 TangentW : TANGENT;
	float2 TexC1    : TEXCOORD1;
	float2 TexC2    : TEXCOORD2;
};

// The domain shader is called for every vertex created by the tessellator.  
// It is like the vertex shader after tessellation.
[domain("tri")]
DomainOut DS(PatchTess patchTess,
			float3 uvw : SV_DomainLocation,
			const OutputPatch<HullOut, 3> tri) {
	DomainOut dout = (DomainOut)0.0f;

	// Fetch the material data.
	MaterialData matData = gMaterialData[gMaterialIndex];

	float2 texC1 = tri[0].TexC * uvw.x;
	float2 texC2 = tri[1].TexC * uvw.y;
	float2 texC3 = tri[2].TexC * uvw.z;
	float4 texC = float4(texC1 + texC2 + texC3, 0.0f, 1.0f);

	dout.TexC1 = mul(texC, gTexTransform).xy;
	dout.TexC2 = mul(texC, matData.MatTransform).xy;

	float3 n1 = tri[0].NormalL * uvw.x;
	float3 n2 = tri[1].NormalL * uvw.y;
	float3 n3 = tri[2].NormalL * uvw.z;
	float3 normalL = normalize(n1 + n2 + n3);

	dout.NormalW = mul(normalL, (float3x3)gWorld);

	// Bilinear interpolation.
	float3 v1 = tri[0].PosL * uvw.x;
	float3 v2 = tri[1].PosL * uvw.y;
	float3 v3 = tri[2].PosL * uvw.z;
	float3 p = v1 + v2 + v3;

	float4 posW = mul(float4(p, 1.0f), gWorld);

	uint dispMapIndex1 = matData.DispMapIndex;
	uint dispMapIndex2 = matData.DispMapIndex + 1;
	
	float dispMapSample1 = gTextureMaps[dispMapIndex1].SampleLevel(gsamLinearWrap, dout.TexC1, 0.0f).x * 0.1f;
	float dispMapSample2 = gTextureMaps[dispMapIndex2].SampleLevel(gsamLinearWrap, dout.TexC2, 0.0f).x;
	float dispMapScale = dispMapSample1 + dispMapSample2;
	
	posW += float4(dout.NormalW, 0.0f) * dispMapScale;

	dout.PosW = posW.xyz;
	dout.PosH = mul(posW, gViewProj);

	float3 t1 = tri[0].TangentU * uvw.x;
	float3 t2 = tri[1].TangentU * uvw.y;
	float3 t3 = tri[2].TangentU * uvw.z;
	float3 tangentU = normalize(t1 + t2 + t3);

	dout.TangentW = mul(tangentU, (float3x3)gWorld);

	return dout;
}

float4 PS(DomainOut pin) : SV_Target {
	// Fetch the material data.
	MaterialData matData = gMaterialData[gMaterialIndex];
	float4 diffuseAlbedo = matData.DiffuseAlbedo;
	float3 fresnelR0 = matData.FresnelR0;
	float  roughness = matData.Roughness;
	uint diffuseMapIndex = matData.DiffuseMapIndex;
	uint normalMapIndex1 = matData.NormalMapIndex;
	uint normalMapIndex2 = matData.NormalMapIndex + 1;

	// Interpolating normal can unnormalize it, so renormalize it.
	pin.NormalW = normalize(pin.NormalW);

	float4 normalMapSample = gTextureMaps[normalMapIndex1].Sample(gsamLinearWrap, pin.TexC1);
	float3 bumpedNormalW1 = NormalSampleToWorldSpace(normalMapSample.rgb, pin.NormalW, pin.TangentW);

	normalMapSample = gTextureMaps[normalMapIndex2].Sample(gsamLinearWrap, pin.TexC2);
	float3 bumpedNormalW2 = NormalSampleToWorldSpace(normalMapSample.rgb, pin.NormalW, pin.TangentW);

	float3 bumpedNormalW = normalize(bumpedNormalW1 + bumpedNormalW2);

	// Uncomment to turn off normal mapping.
	//bumpedNormalW = pin.NormalW;

	// Dynamically look up the texture in the array.
	diffuseAlbedo *= gTextureMaps[diffuseMapIndex].Sample(gsamAnisotropicWrap, (float2)0.0f);

	// Vector from point being lit to eye. 
	float3 toEyeW = normalize(gEyePosW - pin.PosW);

	// Light terms.
	float4 ambient = gAmbientLight * diffuseAlbedo;

	const float shininess = (1.0f - roughness) * normalMapSample.a;
	Material mat = { diffuseAlbedo, fresnelR0, shininess };
	float3 shadowFactor = 1.0f;
	float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
		bumpedNormalW, toEyeW, shadowFactor);

    float4 litColor = ambient + directLight;

	// Add in specular reflections.
	float3 r = reflect(-toEyeW, bumpedNormalW);
	float4 reflectionColor = gCubeMap.Sample(gsamLinearWrap, r);
	float3 fresnelFactor = SchlickFresnel(fresnelR0, bumpedNormalW, r);
    litColor.rgb += shininess * fresnelFactor * reflectionColor.rgb;
	
    // Common convention to take alpha from diffuse albedo.
    litColor.a = diffuseAlbedo.a;

    return litColor;
}