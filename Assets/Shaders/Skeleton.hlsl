//***************************************************************************************
// Default.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

// Defaults for number of lights.
#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 0
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
	float3		PosL			: POSITION;
	float2		TexC			: TEXCOORD;
};

struct VertexOut {
    float3		PosW			: POSITION;
	float2		TexC			: TEXCOORD;
	float		TimePos			: TIMEPOS;
	uint		AnimClipIndex	: CLIPIDX;
	float4x4	World			: WORLDMAT;
};

struct GeoOut {
	float4		PosH			: SV_POSITION;
	uint		PrimID			: SV_PrimitiveID;
};

VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID) {
	VertexOut vout = (VertexOut)0.0f; 

	InstanceIdxData instIdxData = gInstIdxData[gObjectIndex * gMaxInstanceCount + instanceID];
	InstanceData instData = gInstanceData[instIdxData.InstIdx];
	float4x4 world = instData.World;

	vout.PosW = vin.PosL;
	vout.TexC = vin.TexC;
	vout.TimePos = instData.TimePos;
	vout.AnimClipIndex = instData.AnimClipIndex;
	vout.World = world;

    return vout;
}

[maxvertexcount(2)]
void GS(line VertexOut gin[2],
		uint primID : SV_PrimitiveID,
		inout LineStream<GeoOut> lineStream) {
	[unroll]
	for (int i = 0; i < 2; ++i) {
		GeoOut gout = (GeoOut)0.0f;
		
		float3 posL = gin[i].PosW;		
		int boneIndex = (int)gin[i].TexC.x;

		int rowIndex = boneIndex * 4;
		int colIndex = gin[i].AnimClipIndex + (int)gin[i].TimePos;
		float pct = gin[i].TimePos - (int)gin[i].TimePos;

		float4 r1_f0 = gAnimationsDataMap.Load(int3(rowIndex, colIndex, 0));
		float4 r2_f0 = gAnimationsDataMap.Load(int3(rowIndex + 1, colIndex, 0));
		float4 r3_f0 = gAnimationsDataMap.Load(int3(rowIndex + 2, colIndex, 0));
		float4 r4_f0 = gAnimationsDataMap.Load(int3(rowIndex + 3, colIndex, 0));
		
		float4 r1_f1 = gAnimationsDataMap.Load(int3(rowIndex, colIndex + 1, 0));
		float4 r2_f1 = gAnimationsDataMap.Load(int3(rowIndex + 1, colIndex + 1, 0));
		float4 r3_f1 = gAnimationsDataMap.Load(int3(rowIndex + 2, colIndex + 1, 0));
		float4 r4_f1 = gAnimationsDataMap.Load(int3(rowIndex + 3, colIndex + 1, 0));
		
		float4 r1 = r1_f0 + pct * (r1_f1 - r1_f0);
		float4 r2 = r2_f0 + pct * (r2_f1 - r2_f0);
		float4 r3 = r3_f0 + pct * (r3_f1 - r3_f0);
		float4 r4 = r4_f0 + pct * (r4_f1 - r4_f0);
		
		float4x4 trans = { r1, r2, r3, r4 };

		if (boneIndex != -1)
			posL = mul(float4(posL, 1.0f), trans).xyz;
		
		// Transform to world space.
		float4 posW = mul(float4(posL, 1.0f), gin[i].World);
		
		// Transform to homogeneous clip space.
		gout.PosH = mul(posW, gViewProj);
		gout.PosH.z = 0.01f;
	
		gout.PrimID = primID;
	
		lineStream.Append(gout);
	}
}

float4 PS(GeoOut pin) : SV_Target {
	float4 litColor = (float4)0.0f;

	if (pin.PrimID & 0x1)
		litColor = float4(0.0f, 0.0f, 1.0f, 1.0f);
	else
		litColor = float4(1.0f, 0.0f, 0.0f, 1.0f);

    return litColor;
}