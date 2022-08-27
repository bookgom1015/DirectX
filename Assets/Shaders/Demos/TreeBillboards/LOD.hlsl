//***************************************************************************************
// Default.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//
// Default shader, currently supports lighting.
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

// Include structures and functions for lighting.
#include "LightingUtil.hlsl"

Texture2D    gDiffuseMap : register(t0);


SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

// Constant data that varies per frame.
cbuffer cbPerObject : register(b0) {
    float4x4 gWorld;
	float4x4 gTexTransform;
};

// Constant data that varies per material.
cbuffer cbPass : register(b1) {
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;

	float4 gFogColor;
	float gFogStart;
	float gFogRange;
	float2 cbPerObjectPad2;

    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light gLights[MaxLights];
};

cbuffer cbMaterial : register(b2) {
	float4   gDiffuseAlbedo;
    float3   gFresnelR0;
    float    gRoughness;
	float4x4 gMatTransform;
};

struct VertexIn {
	float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
};

struct VertexOut {
	float4 PosH    : POSITION_H;
    float3 PosW    : POSITION_W;
    float3 NormalW : NORMAL;
	float2 TexC    : TEXCOORD;
};

struct GeoOut {
	float4 PosH    : SV_POSITION;
	float3 PosW    : POSITION;
	float3 NormalW : NORMAL;
	float2 TexC    : TEXCOORD;
};

struct DivIn {
	VertexOut V0 : VERT_0;
	VertexOut V1 : VERT_1;
	VertexOut V2 : VERT_2;
};

struct DivOut {
	GeoOut M0 : GOUT_0;
	GeoOut M1 : GOUT_1;
	GeoOut M2 : GOUT_2;
};

VertexOut VS(VertexIn vin) {
	VertexOut vout = (VertexOut)0.0f;
	
    vout.PosW = vin.PosL;
	vout.NormalW = vin.NormalL;
	vout.TexC = vin.TexC;

    return vout;
}

GeoOut MidPoint(VertexOut v0, VertexOut v1) {
	GeoOut result = (GeoOut)0.0f;

	result.PosW = normalize(0.5f * (v0.PosW + v1.PosW));
	result.NormalW = normalize(0.5f * (v0.NormalW + v1.NormalW));
	result.TexC = 0.5f * (v0.TexC + v1.TexC);
	return result;
}

DivOut Subdivide(DivIn din) {
	DivOut dout = (DivOut)0.0f;

	dout.M0 = MidPoint(din.V0, din.V1);
	dout.M1 = MidPoint(din.V1, din.V2);
	dout.M2 = MidPoint(din.V0, din.V2);

	return dout;
}

VertexOut GeoToVert(GeoOut gout) {
	VertexOut vout = (VertexOut)0.0f;

	vout.PosH = gout.PosH;
	vout.PosW = gout.PosW;
	vout.NormalW = gout.NormalW;
	vout.TexC = gout.TexC;

	return vout;
}

inline GeoOut kkk(VertexOut vout) {
	GeoOut g;

	float4 posW = mul(float4(normalize(vout.PosW), 1.0f), gWorld);
	g.PosW = posW.xyz;
	g.PosH = mul(posW, gViewProj);
	g.NormalW = mul(vout.NormalW, (float3x3)gWorld);
	float4 texC = mul(float4(vout.TexC, 0.0f, 1.0f), gTexTransform);
	g.TexC = mul(texC, gMatTransform).xy;

	return g;
}

[maxvertexcount(48)]
void GS(triangle VertexOut gin[3],
		inout TriangleStream<GeoOut> triStream) {
	GeoOut gout[48];

	float distance = sqrt(gEyePosW.x * gEyePosW.x + gEyePosW.y * gEyePosW.y + gEyePosW.z * gEyePosW.z);
	if (distance < 15) {
		DivIn din = (DivIn)0.0f;
		din.V0 = gin[0];
		din.V1 = gin[1];
		din.V2 = gin[2];

		for (int i = 0; i < 3; i++)
			gout[i] = kkk(gin[i]);

		DivOut dout = Subdivide(din);
		gout[3] = kkk(dout.M0);
		gout[4] = kkk(dout.M1);
		gout[5] = kkk(dout.M2);

		din.V0 = GeoToVert(gout[0]);
		din.V1 = GeoToVert(gout[3]);
		din.V2 = GeoToVert(gout[5]);

		dout = Subdivide(din);
		gout[6] = kkk(dout.M0);
		gout[7] = kkk(dout.M1);
		gout[8] = kkk(dout.M2);

		din.V0 = GeoToVert(gout[5]);
		din.V1 = GeoToVert(gout[3]);
		din.V2 = GeoToVert(gout[4]);

		dout = Subdivide(din);
		gout[9] = kkk(dout.M0);
		gout[10] = kkk(dout.M1);
		gout[11] = kkk(dout.M2);

		din.V0 = GeoToVert(gout[5]);
		din.V1 = GeoToVert(gout[4]);
		din.V2 = GeoToVert(gout[2]);

		dout = Subdivide(din);
		gout[12] = kkk(dout.M0);
		gout[13] = kkk(dout.M1);
		gout[14] = kkk(dout.M2);

		din.V0 = GeoToVert(gout[3]);
		din.V1 = GeoToVert(gout[1]);
		din.V2 = GeoToVert(gout[4]);

		dout = Subdivide(din);
		gout[15] = kkk(dout.M0);
		gout[16] = kkk(dout.M1);
		gout[17] = kkk(dout.M2);

		triStream.Append(gout[0]);
		triStream.Append(gout[6]);
		triStream.Append(gout[8]);
		triStream.RestartStrip();

		triStream.Append(gout[6]);
		triStream.Append(gout[7]);
		triStream.Append(gout[8]);
		triStream.RestartStrip();

		triStream.Append(gout[8]);
		triStream.Append(gout[7]);
		triStream.Append(gout[5]);
		triStream.RestartStrip();

		triStream.Append(gout[7]);
		triStream.Append(gout[12]);
		triStream.Append(gout[5]);
		triStream.RestartStrip();

		triStream.Append(gout[5]);
		triStream.Append(gout[12]);
		triStream.Append(gout[14]);
		triStream.RestartStrip();

		triStream.Append(gout[12]);
		triStream.Append(gout[13]);
		triStream.Append(gout[14]);
		triStream.RestartStrip();

		triStream.Append(gout[14]);
		triStream.Append(gout[13]);
		triStream.Append(gout[2]);
		triStream.RestartStrip();

		triStream.Append(gout[6]);
		triStream.Append(gout[3]);
		triStream.Append(gout[7]);
		triStream.RestartStrip();

		triStream.Append(gout[3]);
		triStream.Append(gout[10]);
		triStream.Append(gout[9]);
		triStream.RestartStrip();

		triStream.Append(gout[9]);
		triStream.Append(gout[10]);
		triStream.Append(gout[11]);
		triStream.RestartStrip();

		triStream.Append(gout[10]);
		triStream.Append(gout[4]);
		triStream.Append(gout[11]);
		triStream.RestartStrip();

		triStream.Append(gout[11]);
		triStream.Append(gout[4]);
		triStream.Append(gout[13]);
		triStream.RestartStrip();

		triStream.Append(gout[3]);
		triStream.Append(gout[15]);
		triStream.Append(gout[17]);
		triStream.RestartStrip();

		triStream.Append(gout[15]);
		triStream.Append(gout[16]);
		triStream.Append(gout[17]);
		triStream.RestartStrip();

		triStream.Append(gout[17]);
		triStream.Append(gout[16]);
		triStream.Append(gout[4]);
		triStream.RestartStrip();

		triStream.Append(gout[15]);
		triStream.Append(gout[1]);
		triStream.Append(gout[16]);
	}
	else if (distance < 50) {
		DivIn din = (DivIn)0.0f;
		din.V0 = gin[0];
		din.V1 = gin[1];
		din.V2 = gin[2];

		for (int i = 0; i < 3; i++) 
			gout[i] = kkk(gin[i]);

		DivOut dout = Subdivide(din);
		gout[3] = kkk(dout.M0);
		gout[4] = kkk(dout.M1);
		gout[5] = kkk(dout.M2);

		triStream.Append(gout[0]);
		triStream.Append(gout[3]);
		triStream.Append(gout[5]);
		triStream.RestartStrip();
		
		triStream.Append(gout[3]);
		triStream.Append(gout[4]);
		triStream.Append(gout[5]);
		triStream.RestartStrip();
		
		triStream.Append(gout[5]);
		triStream.Append(gout[4]);
		triStream.Append(gout[2]);
		triStream.RestartStrip();
		
		triStream.Append(gout[3]);
		triStream.Append(gout[1]);
		triStream.Append(gout[4]);
		triStream.RestartStrip();
	}
	else {
		for (int i = 0; i < 3; i++) {
			gout[i] = kkk(gin[i]);
			triStream.Append(gout[i]);
		}
	}
}

float4 PS(GeoOut pin) : SV_Target {
    float4 diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC) * gDiffuseAlbedo;
	
#ifdef ALPHA_TEST
	// Discard pixel if texture alpha < 0.1.  We do this test as soon 
	// as possible in the shader so that we can potentially exit the
	// shader early, thereby skipping the rest of the shader code.
	clip(diffuseAlbedo.a - 0.1f);
#endif

    // Interpolating normal can unnormalize it, so renormalize it.
    pin.NormalW = normalize(pin.NormalW);

    // Vector from point being lit to eye. 
	float3 toEyeW = gEyePosW - pin.PosW;
	float distToEye = length(toEyeW);
	toEyeW /= distToEye; // normalize

    // Light terms.
    float4 ambient = gAmbientLight*diffuseAlbedo;

    const float shininess = 1.0f - gRoughness;
    Material mat = { diffuseAlbedo, gFresnelR0, shininess };
    float3 shadowFactor = 1.0f;
    float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
        pin.NormalW, toEyeW, shadowFactor);

    float4 litColor = ambient + directLight;

#ifdef FOG
	float fogAmount = saturate((distToEye - gFogStart) / gFogRange);
	litColor = lerp(litColor, gFogColor, fogAmount);
#endif

    // Common convention to take alpha from diffuse albedo.
    litColor.a = diffuseAlbedo.a;

    //return litColor;
	return litColor;
}


