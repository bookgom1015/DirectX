//=============================================================================
// Performs a separable Guassian blur with a blur radius up to 5 pixels.
//=============================================================================

#define N 10

static const int NumDataElements = 64;

struct InputData {
	float3 V;
};

struct OutputData {
	float S;
};

Buffer<float3> gInput : register(t0);
RWStructuredBuffer<OutputData> gOutput : register(u0);

[numthreads(1, N, 1)]
void LengthCS(int3 groupThreadID : SV_GroupThreadID,
				int3 dispatchThreadID : SV_DispatchThreadID) {
	int index = groupThreadID.y + dispatchThreadID.x * N;
	float3 vec = gInput[index];
	float length = sqrt(dot(vec, vec));
	
	gOutput[index].S = length;
}