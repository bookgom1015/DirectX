//***************************************************************************************
// ShadowDebug.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

// Include common HLSL code.
#include "Common.hlsl"

struct VertexIn {
	float3 PosL		: POSITION;
	float2 TexC		: TEXCOORD;
};

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float2 TexC		: TEXCOORD;
	uint   InstID	: INSTID;
};

VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID) {
	VertexOut vout = (VertexOut)0.0f;
	
	float3 pos;
	if (instanceID != 0)
		pos = vin.PosL * 0.5f + float3(0.5f, 0.0f, 0.0f);
	else
		pos = vin.PosL * 0.5f + float3(0.5f, -0.5f, 0.0f);

    // Already in homogeneous clip space.
    vout.PosH = float4(pos, 1.0f);
	
	vout.TexC = vin.TexC;
	
	vout.InstID = instanceID;

    return vout;
}

float4 PS(VertexOut pin) : SV_Target {
	if (pin.InstID != 0)
		return float4(gSsaoMap.Sample(gsamLinearWrap, pin.TexC).rrr, 1.0f);
	else
		return float4(gShadowMap.Sample(gsamLinearWrap, pin.TexC).rrr, 1.0f);
}