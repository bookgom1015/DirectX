//=============================================================================
// Performs a separable Guassian blur with a blur radius up to 5 pixels.
//=============================================================================

#define N 32

struct InputData {
	float3 V;
};

struct OutputData {
	float S;
};

ConsumeStructuredBuffer<InputData> gInput : register(u0);
AppendStructuredBuffer<OutputData> gOutput : register(u1);

[numthreads(N, 1, 1)]
void LengthCS(int3 groupThreadID : SV_GroupThreadID,
				int3 dispatchThreadID : SV_DispatchThreadID) {
	InputData inputData = gInput.Consume();

	OutputData outData;
	outData.S = sqrt(dot(inputData.V, inputData.V));
	gOutput.Append(outData);
	
}