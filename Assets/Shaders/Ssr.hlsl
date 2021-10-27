#ifndef __SSR_HLSL__
#define __SSR_HLSL__

#include "SsrCommon.hlsl"

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float3 PosV		: POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout;

	vout.TexC = gTexCoords[vid];

	// Quad covering screen in NDC space.
	vout.PosH = float4(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y, 0.0f, 1.0f);

	// Transform quad corners to view space near plane.
	float4 ph = mul(vout.PosH, gInvProj);
	vout.PosV = ph.xyz / ph.w;

	return vout;
}

float NdcDepthToViewDepth(float z_ndc) {
	// z_ndc = A + B/viewZ, where gProj[2,2]=A and gProj[3,2]=B.
	float viewZ = gProj[3][2] / (z_ndc - gProj[2][2]);
	return viewZ;
}

float4 GetReflectionColor(float3 posW, float3 normalW) {
	// Vector from point being lit to eye. 
	float3 toEyeW = normalize(gEyePosW - posW);

	float3 r = normalize(reflect(-toEyeW, normalW));

	for (int i = 0; i < 8; ++i) {
		// 
		float3 deltaPosW = posW + r * (i + 1);
		float4 posH = mul(float4(deltaPosW, 1.0f), gViewProj);
		posH /= posH.w;

		float2 texC = float2(posH.x, -posH.y) * 0.5f + (float2)0.5f;
		float depthSample = gDepthMap.Sample(gsamDepthMap, texC).r;

		if (posH.z > depthSample) {
			// 1. 
			//        --------------------------------------------------------------------->
			//
			// 2. 
			//                                           <----------------------------------
			//
			// 3.
			//                         <------------------ 
			//                         ------------------>
			//
			// 4.                      
			//                                  <---------
			//        
			//        | step n-1       | step n-1/4      | step n-1/2                      | step n
			// -------o----------------o--------o-0------o---------------------------------o-------
			//                                  | |-Object
			//                       step n-1/8 |
			float3 half_r = r;

			for (int j = 0; j < 8; ++j) {				
				half_r *= 0.5f;
				deltaPosW -= half_r;
				posH = mul(float4(deltaPosW, 1.0f), gViewProj);
				posH /= posH.w;

				texC = float2(posH.x, -posH.y) * 0.5f + (float2)0.5f;
				depthSample = gDepthMap.Sample(gsamDepthMap, texC).r;

				if (posH.z < depthSample)
					deltaPosW += half_r;
			}

			posH = mul(float4(deltaPosW, 1.0f), gViewProj);
			posH /= posH.w;

			texC = float2(posH.x, -posH.y) * 0.5f + (float2)0.5f;
			depthSample = gDepthMap.Sample(gsamDepthMap, texC).r;

			// Since the current collision is determined using only difference in depth values, 
			//  an image is formed on the object even though it is a long distance. Therefore,
			//	after converting the distance value of the depth map to the world coordinate system, 
			//  compare it with the deltaPosW value and ignore it if it is more than a certain distance.
			float4 pv = mul(float4(posH.xy, 0.0f, 1.0f), gInvProj);
			pv.xyz /= pv.w;

			float pz = NdcDepthToViewDepth(depthSample);

			float3 samplePosV = (pz / pv.z) * pv.xyz;
			float4 samplePosW = mul(float4(samplePosV, 1.0f), gInvView);

			float dist = distance(samplePosW, deltaPosW);

			if (dist < 0.05f)
				return gDiffuseMap.Sample(gsamLinearWrap, texC);
		}
	}

	return (float4)0.0f;
}

float4 PS(VertexOut pin) : SV_Target{
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

	float3 normalW = normalize(gNormalMap.Sample(gsamLinearWrap, pin.TexC).xyz);

	// Add in specular reflections.
	float4 reflectionColor = GetReflectionColor(posW.xyz, normalW);

	return reflectionColor;
}

#endif // __SSR_HLSL__