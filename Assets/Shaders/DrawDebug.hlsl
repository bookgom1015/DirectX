//***************************************************************************************
// ShadowDebug.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

// Include common HLSL code.
#include "Common.hlsl"

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float2 TexC		: TEXCOORD;
	uint   InstID	: INSTID;
};

static const float2 gTexCoords[6] = {
	float2(0.0f, 1.0f),
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f),
	float2(0.0f, 1.0f),
	float2(1.0f, 0.0f),
	float2(1.0f, 1.0f)
};

VertexOut VS(uint vid : SV_VertexID, uint instanceID : SV_InstanceID) {
	VertexOut vout = (VertexOut)0.0f;
	
	vout.TexC = gTexCoords[vid];
	vout.InstID = instanceID;

	// Quad covering screen in NDC space.
	float3 pos = float4(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y, 0.0f, 1.0f);
	if (instanceID == 0)
		pos = pos * 0.2f + float3(0.8f, 0.8f, 0.0f);
	else if (instanceID == 1)
		pos = pos * 0.2f + float3(0.8f, 0.4f, 0.0f);
	else if (instanceID == 2)
		pos = pos * 0.2f + float3(0.8f, 0.0f, 0.0f);
	else if (instanceID == 3)
		pos = pos * 0.2f + float3(0.8f, -0.4f, 0.0f);
	else if (instanceID == 4)
		pos = pos * 0.2f + float3(0.8f, -0.8f, 0.0f);

    // Already in homogeneous clip space.
    vout.PosH = float4(pos, 1.0f);

    return vout;
}

float4 PS(VertexOut pin) : SV_Target{
	if (pin.InstID == 0)
		return gMainPassMap1.Sample(gsamLinearWrap, pin.TexC);
	else if (pin.InstID == 1)
		return gMainPassMap2.Sample(gsamLinearWrap, pin.TexC);
	else if (pin.InstID == 2)
		return float4(gDepthMap.Sample(gsamLinearWrap, pin.TexC).rrr, 1.0f);
	else if (pin.InstID == 3)
		return float4(gSsrMap.Sample(gsamLinearWrap, pin.TexC).rgb, 1.0f);
	else if (pin.InstID == 4)
		return float4(gBloomMap.Sample(gsamLinearWrap, pin.TexC).rrr, 1.0f);
	else
		return float4(0.0f, 0.0f, 0.0f, 1.0f);
}