#ifndef __POSTPASS_HLSL__

cbuffer cbPostPass : register(b0) {
	float4x4	gInvView;
	float4x4	gProj;
	float4x4	gInvProj;
	float3		gEyePosW;
};

cbuffer cbRootConstants : register(b1) {
	uint	gMaxInstanceCount;
	float	gCubeMapCenter;
	float	gCubeMapExtents;
	uint	gEffectEnabled;
};

// Nonnumeric values cannot be added to a cbuffer.
TextureCube				gCubeMap					: register(t0);
TextureCube				gBlurCubeMap				: register(t1);
Texture2D				gMainPassMap1				: register(t2);
Texture2D				gMainPassMap2				: register(t3);
Texture2D				gDiffuseMap					: register(t4);
Texture2D				gNormalMap					: register(t5);
Texture2D				gDepthMap					: register(t6);
Texture2D				gSpecularMap				: register(t7);
Texture2D				gShadowMap					: register(t8);
Texture2D				gSsaoMap					: register(t9);
Texture2D				gSsrMap						: register(t10);

SamplerState			gsamPointWrap				: register(s0);
SamplerState			gsamPointClamp				: register(s1);
SamplerState			gsamLinearWrap				: register(s2);
SamplerState			gsamLinearClamp				: register(s3);
SamplerState			gsamAnisotropicWrap			: register(s4);
SamplerState			gsamAnisotropicClamp		: register(s5);
SamplerComparisonState	gsamShadow					: register(s6);
SamplerState			gsamDepthMap				: register(s7);

static const float2 gTexCoords[6] = {
	float2(0.0f, 1.0f),
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f),
	float2(0.0f, 1.0f),
	float2(1.0f, 0.0f),
	float2(1.0f, 1.0f)
};

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

//---------------------------------------------------------------------------------------
// Slap method
//---------------------------------------------------------------------------------------
float3 BoxCubeMapLookup(float3 rayOrigin, float3 unitRayDir, float3 boxCenter, float3 boxExtents) {
	// Set the origin to box of center.
	float3 p = rayOrigin - boxCenter;

	// The formula for AABB's i-th plate ray versus plane intersection is as follows.
	//
	// t1 = (-dot(n_i, p) + h_i) / dot(n_i, d) = (-p_i + h_i) / d_i
	// t2 = (-dot(n_i, p) - h_i) / dot(n_i, d) = (-p_i - h_i) / d_i
	float3 t1 = (-p + boxExtents) / unitRayDir;
	float3 t2 = (-p - boxExtents) / unitRayDir;

	// Find the maximum value for each coordinate component.
	// We assume that ray is inside the box, so we only need to find the maximum value of the intersection parameter. 
	float3 tmax = max(t1, t2);

	// Find the minimum value of all components for tmax.
	float t = min(min(tmax.x, tmax.y), tmax.z);

	// To use a lookup vector for a cube map, 
	// create coordinate relative to center of box.
	return p + t * unitRayDir;
}

float4 PS(VertexOut pin) : SV_Target{
	// Get viewspace normal and z-coord of this pixel.  
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

	float4 normalMapSample = gNormalMap.Sample(gsamAnisotropicWrap, pin.TexC);
	float3 normalW = mul(normalMapSample.xyz, (float3x3)gInvView);
	normalW = normalize(normalW);

	// Vector from point being lit to eye. 
	float3 toEyeW = normalize(gEyePosW - posW.xyz);

	// Add in specular reflections.
	float3 r = reflect(-toEyeW, normalW);
	float3 lookup = BoxCubeMapLookup(posW.xyz, r, gCubeMapCenter, gCubeMapExtents);

	float4 reflectionSample = gSsrMap.Sample(gsamLinearWrap, pin.TexC);
	float3 cubeMapSample = gBlurCubeMap.Sample(gsamLinearWrap, lookup).rgb;

	float4 mainPassSample1 = gMainPassMap1.Sample(gsamLinearClamp, pin.TexC);
	float4 mainPassSample2 = gMainPassMap2.Sample(gsamLinearClamp, pin.TexC);

	float3 reflectionColor = mainPassSample2.rgb * (reflectionSample.a * reflectionSample.rgb + (1.0f - reflectionSample.a) * cubeMapSample);

	return mainPassSample1 + float4(reflectionColor, 0.0f);
}

#endif // __POSTPASS_HLSL__