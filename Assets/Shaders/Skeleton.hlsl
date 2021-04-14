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
	float3	PosL		: POSITION;
	float2	TexC		: TEXCOORD;
};

struct VertexOut {
    float3		PosW	: POSITION;
	float2		TexC	: TEXCOORD;
	float4x4	World	: WORLDMAT;
};

struct GeoOut {
	float4	PosH		: SV_POSITION;
	uint	PrimID		: SV_PrimitiveID;
};

VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID) {
	VertexOut vout = (VertexOut)0.0f; 

	InstanceData instData = gInstanceData[gInstanceIndex * MAX_INSTANCE_DATA + instanceID];
	float4x4 world = instData.World;

	vout.PosW = vin.PosL;
	vout.TexC = vin.TexC;
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
		if (boneIndex != -1)
			posL = mul(float4(posL, 1.0f), gBoneTransforms[boneIndex]).xyz;
		
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