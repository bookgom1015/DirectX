#include "DX12Game/PsoManager.h"

using namespace Microsoft::WRL;

GameResult PsoManager::Initialize(ID3D12Device* inDevice) {
	md3dDevice = inDevice;

	mInputLayout = {
		{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,			0, 0,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",		0, DXGI_FORMAT_R32G32B32_FLOAT,			0, 12,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,			0, 24,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT",	0, DXGI_FORMAT_R32G32B32_FLOAT,			0, 32,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	mSkinnedInputLayout = {
		{ "POSITION",		0, DXGI_FORMAT_R32G32B32_FLOAT,		0, 0,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",			0, DXGI_FORMAT_R32G32B32_FLOAT,		0, 12,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",		0, DXGI_FORMAT_R32G32_FLOAT,		0, 24,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT",		0, DXGI_FORMAT_R32G32B32_FLOAT,		0, 32,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BONEWEIGHTS",	0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0, 44,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BONEWEIGHTS",	1, DXGI_FORMAT_R32G32B32A32_FLOAT,	0, 60,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BONEINDICES",	0, DXGI_FORMAT_R32G32B32A32_SINT,	0, 76,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BONEINDICES",	1, DXGI_FORMAT_R32G32B32A32_SINT,	0, 92,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	return GameResultOk;
}

GameResult PsoManager::BuildPso(
		InputLayouts inType,
		ID3D12RootSignature* inRootSignature,
		ComPtr<ID3DBlob>& inVShader,
		ComPtr<ID3DBlob>& inPShader,
		D3D12_RASTERIZER_DESC inRasterizerDesc,
		D3D12_DEPTH_STENCIL_DESC inDepthStencilDesc,
		D3D12_BLEND_DESC inBlendDesc,
		D3D12_PRIMITIVE_TOPOLOGY_TYPE inTopologyType,
		const std::vector<DXGI_FORMAT>& inRTVFormats,
		DXGI_FORMAT inDSVFormat,
		const std::string& inName) {
	size_t rtNum = inRTVFormats.size();
	if (inRootSignature == nullptr || inVShader == nullptr || inPShader == nullptr || rtNum > 8) return GameResult(S_FALSE, L"Invalid arguments");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;

	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	if (inType == InputLayouts::EBasic) psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	else if (inType == InputLayouts::ESkinned) psoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
	else psoDesc.InputLayout = { nullptr, 0 };
	psoDesc.pRootSignature = inRootSignature;
	psoDesc.VS = {
		reinterpret_cast<BYTE*>(inVShader->GetBufferPointer()),
		inVShader->GetBufferSize()
	};
	psoDesc.PS = {
		reinterpret_cast<BYTE*>(inPShader->GetBufferPointer()),
		inPShader->GetBufferSize()
	};
	psoDesc.RasterizerState = inRasterizerDesc;
	psoDesc.BlendState = inBlendDesc;
	psoDesc.DepthStencilState = inDepthStencilDesc;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = inTopologyType;
	psoDesc.NumRenderTargets = (UINT)rtNum;
	for (int i = 0; i < rtNum; ++i)
		psoDesc.RTVFormats[i] = inRTVFormats[i];
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.DSVFormat = inDSVFormat;
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[inName])));

	return GameResultOk;
}

GameResult PsoManager::BuildPso(
		InputLayouts inType,
		ID3D12RootSignature* inRootSignature,
		ComPtr<ID3DBlob>& inVShader,
		ComPtr<ID3DBlob>& inGShader,
		ComPtr<ID3DBlob>& inPShader,
		D3D12_RASTERIZER_DESC inRasterizerDesc,
		D3D12_DEPTH_STENCIL_DESC inDepthStencilDesc,
		D3D12_BLEND_DESC inBlendDesc,
		D3D12_PRIMITIVE_TOPOLOGY_TYPE inTopologyType,
		const std::vector<DXGI_FORMAT>& inRTVFormats,
		DXGI_FORMAT inDSVFormat,
		const std::string& inName) {
	size_t rtNum = inRTVFormats.size();
	if (inRootSignature == nullptr || inVShader == nullptr || inPShader == nullptr || rtNum > 8) return GameResult(S_FALSE, L"Invalid arguments");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;

	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	if (inType == InputLayouts::EBasic) psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	else if (inType == InputLayouts::ESkinned) psoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
	else psoDesc.InputLayout = { nullptr, 0 };
	psoDesc.pRootSignature = inRootSignature;
	psoDesc.VS = {
		reinterpret_cast<BYTE*>(inVShader->GetBufferPointer()),
		inVShader->GetBufferSize()
	};
	psoDesc.GS = {
		reinterpret_cast<BYTE*>(inGShader->GetBufferPointer()),
		inGShader->GetBufferSize()
	};
	psoDesc.PS = {
		reinterpret_cast<BYTE*>(inPShader->GetBufferPointer()),
		inPShader->GetBufferSize()
	};
	psoDesc.RasterizerState = inRasterizerDesc;
	psoDesc.BlendState = inBlendDesc;
	psoDesc.DepthStencilState = inDepthStencilDesc;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = inTopologyType;
	psoDesc.NumRenderTargets = (UINT)rtNum;
	if (rtNum == 0) {
		psoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	}
	else {
		for (int i = 0; i < rtNum; ++i)
			psoDesc.RTVFormats[i] = inRTVFormats[i];
	}
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.DSVFormat = inDSVFormat;
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[inName])));

	return GameResultOk;
}

ID3D12PipelineState* PsoManager::GetPsoPtr(const std::string& inName) {
	return mPSOs[inName].Get();
}

ID3D12PipelineState* PsoManager::GetSkeletonPsoPtr() {
	return mPSOs["skeleton"].Get();
}

ID3D12PipelineState* PsoManager::GetShadowPsoPtr() {
	return mPSOs["shadow"].Get();
}

ID3D12PipelineState* PsoManager::GetSkinnedShadowPsoPtr() {
	return mPSOs["skinnedShadow"].Get();
}

ID3D12PipelineState* PsoManager::GetDebugPsoPtr() {
	return mPSOs["debug"].Get();
}

ID3D12PipelineState* PsoManager::GetSsaoPsoPtr() {
	return mPSOs["ssao"].Get();
}

ID3D12PipelineState* PsoManager::GetSsaoBlurPsoPtr() {
	return mPSOs["ssaoBlur"].Get();
}

ID3D12PipelineState* PsoManager::GetSsrPsoPtr() {
	return mPSOs["ssr"].Get();
}

ID3D12PipelineState* PsoManager::GetSsrBlurPsoPtr() {
	return mPSOs["ssrBlur"].Get();
}

ID3D12PipelineState* PsoManager::GetSkyPsoPtr() {
	return mPSOs["sky"].Get();
}

ID3D12PipelineState* PsoManager::GetGBufferPsoPtr() {
	return mPSOs["gbuffer"].Get();
}

ID3D12PipelineState* PsoManager::GetSkinnedGBufferPsoPtr() {
	return mPSOs["skinnedGBuffer"].Get();
}

ID3D12PipelineState* PsoManager::GetAlphaBlendingGBufferPsoPtr() {
	return mPSOs["alphaBlendingGBuffer"].Get();
}

ID3D12PipelineState* PsoManager::GetMainPassPsoPtr() {
	return mPSOs["mainPass"].Get();
}

ID3D12PipelineState* PsoManager::GetPostPassPsoPtr() {
	return mPSOs["postPass"].Get();
}

ID3D12PipelineState* PsoManager::GetBloomPsoPtr() {
	return mPSOs["bloom"].Get();
}

ID3D12PipelineState* PsoManager::GetBloomBlurPsoPtr() {
	return mPSOs["bloomBlur"].Get();
}