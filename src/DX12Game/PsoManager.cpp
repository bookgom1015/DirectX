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

GameResult PsoManager::BuildSkeletonPso(
		ID3D12RootSignature* inRootSignature,
		ComPtr<ID3DBlob>& inVShader,
		ComPtr<ID3DBlob>& inGShader,
		ComPtr<ID3DBlob>& inPShader,
		DXGI_FORMAT inRTVFormat) {
	if (inRootSignature == nullptr || inVShader == nullptr || inGShader == nullptr || inPShader == nullptr) return GameResult(S_FALSE, L"Invalid arguments");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC skeletonPsoDesc;
	
	ZeroMemory(&skeletonPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	skeletonPsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	skeletonPsoDesc.pRootSignature = inRootSignature;
	skeletonPsoDesc.VS = {
		reinterpret_cast<BYTE*>(inVShader->GetBufferPointer()),
		inVShader->GetBufferSize()
	};
	skeletonPsoDesc.GS = {
		reinterpret_cast<BYTE*>(inGShader->GetBufferPointer()),
		inGShader->GetBufferSize()
	};
	skeletonPsoDesc.PS = {
		reinterpret_cast<BYTE*>(inPShader->GetBufferPointer()),
		inPShader->GetBufferSize()
	};
	skeletonPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	skeletonPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	skeletonPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	skeletonPsoDesc.DepthStencilState.DepthEnable = FALSE;
	skeletonPsoDesc.SampleMask = UINT_MAX;
	skeletonPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	skeletonPsoDesc.NumRenderTargets = 1;
	skeletonPsoDesc.RTVFormats[0] = inRTVFormat;
	skeletonPsoDesc.SampleDesc.Count = 1;
	skeletonPsoDesc.SampleDesc.Quality = 0;
	skeletonPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&skeletonPsoDesc, IID_PPV_ARGS(&mPSOs["skeleton"])));

	return GameResultOk;
}

GameResult PsoManager::BuildShadowPso(
		ID3D12RootSignature* inRootSignature,
		ComPtr<ID3DBlob>& inVShader,
		ComPtr<ID3DBlob>& inPShader,
		DXGI_FORMAT inDSVFormat) {
	if (inRootSignature == nullptr || inVShader == nullptr || inPShader == nullptr) return GameResult(S_FALSE, L"Invalid arguments");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC smapPsoDesc;

	ZeroMemory(&smapPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	smapPsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	smapPsoDesc.pRootSignature = inRootSignature;
	smapPsoDesc.VS = {
		reinterpret_cast<BYTE*>(inVShader->GetBufferPointer()),
		inVShader->GetBufferSize()
	};
	smapPsoDesc.PS = {
		reinterpret_cast<BYTE*>(inPShader->GetBufferPointer()),
		inPShader->GetBufferSize()
	};
	smapPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	smapPsoDesc.RasterizerState.DepthBias = 100000;
	smapPsoDesc.RasterizerState.DepthBiasClamp = 0.001f;
	smapPsoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
	smapPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	smapPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	smapPsoDesc.SampleMask = UINT_MAX;
	smapPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	smapPsoDesc.NumRenderTargets = 0;
	// Shadow map pass does not have a render target.
	smapPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	smapPsoDesc.SampleDesc.Count = 1;
	smapPsoDesc.SampleDesc.Quality = 0;
	smapPsoDesc.DSVFormat = inDSVFormat;
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&smapPsoDesc, IID_PPV_ARGS(&mPSOs["shadow"])));

	return GameResultOk;
}

GameResult PsoManager::BuildSkinnedShadowPso(
		ID3D12RootSignature* inRootSignature,
		ComPtr<ID3DBlob>& inVShader,
		ComPtr<ID3DBlob>& inPShader,
		DXGI_FORMAT inDSVFormat) {
	if (inRootSignature == nullptr || inVShader == nullptr || inPShader == nullptr) return GameResult(S_FALSE, L"Invalid arguments");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedSmapPsoDesc;

	ZeroMemory(&skinnedSmapPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	skinnedSmapPsoDesc.InputLayout = { mSkinnedInputLayout.data(), static_cast<UINT>(mSkinnedInputLayout.size()) };
	skinnedSmapPsoDesc.pRootSignature = inRootSignature;
	skinnedSmapPsoDesc.VS = {
		reinterpret_cast<BYTE*>(inVShader->GetBufferPointer()),
		inVShader->GetBufferSize()
	};
	skinnedSmapPsoDesc.PS = {
		reinterpret_cast<BYTE*>(inPShader->GetBufferPointer()),
		inPShader->GetBufferSize()
	};
	skinnedSmapPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	skinnedSmapPsoDesc.RasterizerState.DepthBias = 100000;
	skinnedSmapPsoDesc.RasterizerState.DepthBiasClamp = 0.001f;
	skinnedSmapPsoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
	skinnedSmapPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	skinnedSmapPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	skinnedSmapPsoDesc.SampleMask = UINT_MAX;
	skinnedSmapPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	skinnedSmapPsoDesc.NumRenderTargets = 0;
	// Shadow map pass does not have a render target.
	skinnedSmapPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	skinnedSmapPsoDesc.SampleDesc.Count = 1;
	skinnedSmapPsoDesc.SampleDesc.Quality = 0;
	skinnedSmapPsoDesc.DSVFormat = inDSVFormat;
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedSmapPsoDesc, IID_PPV_ARGS(&mPSOs["skinnedShadow"])));

	return GameResultOk;
}

GameResult PsoManager::BuildDebugPso(
		ID3D12RootSignature* inRootSignature,
		ComPtr<ID3DBlob>& inVShader,
		ComPtr<ID3DBlob>& inPShader,
		DXGI_FORMAT inRTVFormat) {
	if (inRootSignature == nullptr || inVShader == nullptr || inPShader == nullptr) return GameResult(S_FALSE, L"Invalid arguments");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC debugPsoDesc;

	ZeroMemory(&debugPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	debugPsoDesc.InputLayout = { nullptr, 0 };
	debugPsoDesc.pRootSignature = inRootSignature;
	debugPsoDesc.VS = {
		reinterpret_cast<BYTE*>(inVShader->GetBufferPointer()),
		inVShader->GetBufferSize()
	};
	debugPsoDesc.PS = {
		reinterpret_cast<BYTE*>(inPShader->GetBufferPointer()),
		inPShader->GetBufferSize()
	};
	debugPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	debugPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	debugPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	debugPsoDesc.DepthStencilState.DepthEnable = FALSE;
	debugPsoDesc.SampleMask = UINT_MAX;
	debugPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	debugPsoDesc.NumRenderTargets = 1;
	debugPsoDesc.RTVFormats[0] = inRTVFormat;
	debugPsoDesc.SampleDesc.Count = 1;
	debugPsoDesc.SampleDesc.Quality = 0;
	debugPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&debugPsoDesc, IID_PPV_ARGS(&mPSOs["debug"])));

	return GameResultOk;
}

GameResult PsoManager::BuildSsaoPso(
		ID3D12RootSignature* inRootSignature,
		ComPtr<ID3DBlob>& inVShader,
		ComPtr<ID3DBlob>& inPShader,
		DXGI_FORMAT inRTVFormat) {
	if (inRootSignature == nullptr || inVShader == nullptr || inPShader == nullptr) return GameResult(S_FALSE, L"Invalid arguments");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC ssaoPsoDesc;

	ZeroMemory(&ssaoPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	ssaoPsoDesc.InputLayout = { nullptr, 0 };
	ssaoPsoDesc.pRootSignature = inRootSignature;
	ssaoPsoDesc.VS = {
		reinterpret_cast<BYTE*>(inVShader->GetBufferPointer()),
		inVShader->GetBufferSize()
	};
	ssaoPsoDesc.PS = {
		reinterpret_cast<BYTE*>(inPShader->GetBufferPointer()),
		inPShader->GetBufferSize()
	};
	ssaoPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	ssaoPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	ssaoPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	// SSAO effect does not need the depth buffer.
	ssaoPsoDesc.DepthStencilState.DepthEnable = FALSE;
	ssaoPsoDesc.SampleMask = UINT_MAX;
	ssaoPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	ssaoPsoDesc.NumRenderTargets = 1;
	ssaoPsoDesc.RTVFormats[0] = inRTVFormat;
	ssaoPsoDesc.SampleDesc.Count = 1;
	ssaoPsoDesc.SampleDesc.Quality = 0;
	ssaoPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&ssaoPsoDesc, IID_PPV_ARGS(&mPSOs["ssao"])));

	return GameResultOk;
}

GameResult PsoManager::BuildSsaoBlurPso(
		ID3D12RootSignature* inRootSignature,
		ComPtr<ID3DBlob>& inVShader,
		ComPtr<ID3DBlob>& inPShader,
		DXGI_FORMAT inRTVFormat) {
	if (inRootSignature == nullptr || inVShader == nullptr || inPShader == nullptr) return GameResult(S_FALSE, L"Invalid arguments");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC ssaoBlurPsoDesc;

	ZeroMemory(&ssaoBlurPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	ssaoBlurPsoDesc.InputLayout = { nullptr, 0 };
	ssaoBlurPsoDesc.pRootSignature = inRootSignature;
	ssaoBlurPsoDesc.VS = {
		reinterpret_cast<BYTE*>(inVShader->GetBufferPointer()),
		inVShader->GetBufferSize()
	};
	ssaoBlurPsoDesc.PS = {
		reinterpret_cast<BYTE*>(inPShader->GetBufferPointer()),
		inPShader->GetBufferSize()
	};
	ssaoBlurPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	ssaoBlurPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	ssaoBlurPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	// SSAO effect does not need the depth buffer.
	ssaoBlurPsoDesc.DepthStencilState.DepthEnable = FALSE;
	ssaoBlurPsoDesc.SampleMask = UINT_MAX;
	ssaoBlurPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	ssaoBlurPsoDesc.NumRenderTargets = 1;
	ssaoBlurPsoDesc.RTVFormats[0] = inRTVFormat;
	ssaoBlurPsoDesc.SampleDesc.Count = 1;
	ssaoBlurPsoDesc.SampleDesc.Quality = 0;
	ssaoBlurPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&ssaoBlurPsoDesc, IID_PPV_ARGS(&mPSOs["ssaoBlur"])));

	return GameResultOk;
}

GameResult PsoManager::BuildSsrPso(
	ID3D12RootSignature* inRootSignature,
	ComPtr<ID3DBlob>& inVShader,
	ComPtr<ID3DBlob>& inPShader,
	DXGI_FORMAT inRTVFormat) {
	if (inRootSignature == nullptr || inVShader == nullptr || inPShader == nullptr) return GameResult(S_FALSE, L"Invalid arguments");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC ssrPsoDesc;

	ZeroMemory(&ssrPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	ssrPsoDesc.InputLayout = { nullptr, 0 };
	ssrPsoDesc.pRootSignature = inRootSignature;
	ssrPsoDesc.VS = {
		reinterpret_cast<BYTE*>(inVShader->GetBufferPointer()),
		inVShader->GetBufferSize()
	};
	ssrPsoDesc.PS = {
		reinterpret_cast<BYTE*>(inPShader->GetBufferPointer()),
		inPShader->GetBufferSize()
	};
	ssrPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	ssrPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	ssrPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	// SSR effect does not need the depth buffer.
	ssrPsoDesc.DepthStencilState.DepthEnable = FALSE;
	ssrPsoDesc.SampleMask = UINT_MAX;
	ssrPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	ssrPsoDesc.NumRenderTargets = 1;
	ssrPsoDesc.RTVFormats[0] = inRTVFormat;
	ssrPsoDesc.SampleDesc.Count = 1;
	ssrPsoDesc.SampleDesc.Quality = 0;
	ssrPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&ssrPsoDesc, IID_PPV_ARGS(&mPSOs["ssr"])));

	return GameResultOk;
}

GameResult PsoManager::BuildSsrBlurPso(
	ID3D12RootSignature* inRootSignature,
	ComPtr<ID3DBlob>& inVShader,
	ComPtr<ID3DBlob>& inPShader,
	DXGI_FORMAT inRTVFormat) {
	if (inRootSignature == nullptr || inVShader == nullptr || inPShader == nullptr) return GameResult(S_FALSE, L"Invalid arguments");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC ssrBlurPsoDesc;

	ZeroMemory(&ssrBlurPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	ssrBlurPsoDesc.InputLayout = { nullptr, 0 };
	ssrBlurPsoDesc.pRootSignature = inRootSignature;
	ssrBlurPsoDesc.VS = {
		reinterpret_cast<BYTE*>(inVShader->GetBufferPointer()),
		inVShader->GetBufferSize()
	};
	ssrBlurPsoDesc.PS = {
		reinterpret_cast<BYTE*>(inPShader->GetBufferPointer()),
		inPShader->GetBufferSize()
	};
	ssrBlurPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	ssrBlurPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	ssrBlurPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	// SSR effect does not need the depth buffer.
	ssrBlurPsoDesc.DepthStencilState.DepthEnable = FALSE;
	ssrBlurPsoDesc.SampleMask = UINT_MAX;
	ssrBlurPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	ssrBlurPsoDesc.NumRenderTargets = 1;
	ssrBlurPsoDesc.RTVFormats[0] = inRTVFormat;
	ssrBlurPsoDesc.SampleDesc.Count = 1;
	ssrBlurPsoDesc.SampleDesc.Quality = 0;
	ssrBlurPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&ssrBlurPsoDesc, IID_PPV_ARGS(&mPSOs["ssrBlur"])));

	return GameResultOk;
}

GameResult PsoManager::BuildSkyPso(
		ID3D12RootSignature* inRootSignature,
		ComPtr<ID3DBlob>& inVShader,
		ComPtr<ID3DBlob>& inPShader,
		DXGI_FORMAT inRTVFormat,
		DXGI_FORMAT inDSVFormat) {
	if (inRootSignature == nullptr || inVShader == nullptr || inPShader == nullptr) return GameResult(S_FALSE, L"Invalid arguments");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc;

	ZeroMemory(&skyPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	skyPsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	skyPsoDesc.pRootSignature = inRootSignature;
	skyPsoDesc.VS = {
		reinterpret_cast<BYTE*>(inVShader->GetBufferPointer()),
		inVShader->GetBufferSize()
	};
	skyPsoDesc.PS = {
		reinterpret_cast<BYTE*>(inPShader->GetBufferPointer()),
		inPShader->GetBufferSize()
	};
	skyPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	// The camera is inside the sky sphere, so just turn off culling.
	skyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	skyPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	skyPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	// Make sure the depth function is LESS_EQUAL and not just LESS.  
	// Otherwise, the normalized depth values at z = 1 (NDC) will 
	// fail the depth test if the depth buffer was cleared to 1.
	skyPsoDesc.SampleMask = UINT_MAX;
	skyPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	skyPsoDesc.NumRenderTargets = 1;
	skyPsoDesc.RTVFormats[0] = inRTVFormat;
	skyPsoDesc.SampleDesc.Count = 1;
	skyPsoDesc.SampleDesc.Quality = 0;
	skyPsoDesc.DSVFormat = inDSVFormat;
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&mPSOs["sky"])));

	return GameResultOk;
}

GameResult PsoManager::BuildGBufferPso(
		ID3D12RootSignature* inRootSignature,
		ComPtr<ID3DBlob>& inVShader,
		ComPtr<ID3DBlob>& inPShader,
		DXGI_FORMAT inRTVFormat1,
		DXGI_FORMAT inRTVFormat2,
		DXGI_FORMAT inRTVFormat3,
		DXGI_FORMAT inDSVFormat) {
	if (inRootSignature == nullptr || inVShader == nullptr || inPShader == nullptr) return GameResult(S_FALSE, L"Invalid arguments");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gbufferPsoDesc;

	ZeroMemory(&gbufferPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	gbufferPsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	gbufferPsoDesc.pRootSignature = inRootSignature;
	gbufferPsoDesc.VS = {
		reinterpret_cast<BYTE*>(inVShader->GetBufferPointer()),
		inVShader->GetBufferSize()
	};
	gbufferPsoDesc.PS = {
		reinterpret_cast<BYTE*>(inPShader->GetBufferPointer()),
		inPShader->GetBufferSize()
	};
	gbufferPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	gbufferPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	gbufferPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	gbufferPsoDesc.SampleMask = UINT_MAX;
	gbufferPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	gbufferPsoDesc.NumRenderTargets = 3;
	gbufferPsoDesc.RTVFormats[0] = inRTVFormat1;
	gbufferPsoDesc.RTVFormats[1] = inRTVFormat2;
	gbufferPsoDesc.RTVFormats[2] = inRTVFormat3;
	gbufferPsoDesc.SampleDesc.Count = 1;
	gbufferPsoDesc.SampleDesc.Quality = 0;
	gbufferPsoDesc.DSVFormat = inDSVFormat;
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&gbufferPsoDesc, IID_PPV_ARGS(&mPSOs["gbuffer"])));

	return GameResultOk;
}

GameResult PsoManager::BuildSkinnedGBufferPso(
		ID3D12RootSignature* inRootSignature,
		ComPtr<ID3DBlob>& inVShader,
		ComPtr<ID3DBlob>& inPShader,
		DXGI_FORMAT inRTVFormat1,
		DXGI_FORMAT inRTVFormat2,
		DXGI_FORMAT inRTVFormat3,
		DXGI_FORMAT inDSVFormat) {
	if (inRootSignature == nullptr || inVShader == nullptr || inPShader == nullptr) return GameResult(S_FALSE, L"Invalid arguments");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedGBufferPsoDesc;

	ZeroMemory(&skinnedGBufferPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	skinnedGBufferPsoDesc.InputLayout = { mSkinnedInputLayout.data(), static_cast<UINT>(mSkinnedInputLayout.size()) };
	skinnedGBufferPsoDesc.pRootSignature = inRootSignature;
	skinnedGBufferPsoDesc.VS = {
		reinterpret_cast<BYTE*>(inVShader->GetBufferPointer()),
		inVShader->GetBufferSize()
	};
	skinnedGBufferPsoDesc.PS = {
		reinterpret_cast<BYTE*>(inPShader->GetBufferPointer()),
		inPShader->GetBufferSize()
	};
	skinnedGBufferPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	skinnedGBufferPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	skinnedGBufferPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	skinnedGBufferPsoDesc.SampleMask = UINT_MAX;
	skinnedGBufferPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	skinnedGBufferPsoDesc.NumRenderTargets = 3;
	skinnedGBufferPsoDesc.RTVFormats[0] = inRTVFormat1;
	skinnedGBufferPsoDesc.RTVFormats[1] = inRTVFormat2;
	skinnedGBufferPsoDesc.RTVFormats[2] = inRTVFormat3;
	skinnedGBufferPsoDesc.SampleDesc.Count = 1;
	skinnedGBufferPsoDesc.SampleDesc.Quality = 0;
	skinnedGBufferPsoDesc.DSVFormat = inDSVFormat;
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedGBufferPsoDesc, IID_PPV_ARGS(&mPSOs["skinnedGBuffer"])));

	return GameResultOk;
}

GameResult PsoManager::BuildAlphaBlendingGBufferPso(
		ID3D12RootSignature* inRootSignature,
		ComPtr<ID3DBlob>& inVShader,
		ComPtr<ID3DBlob>& inPShader,
		DXGI_FORMAT inRTVFormat1,
		DXGI_FORMAT inRTVFormat2,
		DXGI_FORMAT inRTVFormat3,
		DXGI_FORMAT inDSVFormat) {
	if (inRootSignature == nullptr || inVShader == nullptr || inPShader == nullptr) return GameResult(S_FALSE, L"Invalid arguments");

	D3D12_RENDER_TARGET_BLEND_DESC gbufferBlendDesc;
	gbufferBlendDesc.BlendEnable = true;
	gbufferBlendDesc.LogicOpEnable = false;
	gbufferBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	gbufferBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	gbufferBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	gbufferBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	gbufferBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	gbufferBlendDesc.SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
	gbufferBlendDesc.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	gbufferBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC alphaBlendingBufferPsoDesc;

	ZeroMemory(&alphaBlendingBufferPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	alphaBlendingBufferPsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	alphaBlendingBufferPsoDesc.pRootSignature = inRootSignature;
	alphaBlendingBufferPsoDesc.VS = {
		reinterpret_cast<BYTE*>(inVShader->GetBufferPointer()),
		inVShader->GetBufferSize()
	};
	alphaBlendingBufferPsoDesc.PS = {
		reinterpret_cast<BYTE*>(inPShader->GetBufferPointer()),
		inPShader->GetBufferSize()
	};
	alphaBlendingBufferPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	alphaBlendingBufferPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	alphaBlendingBufferPsoDesc.BlendState.RenderTarget[0] = gbufferBlendDesc;
	alphaBlendingBufferPsoDesc.BlendState.RenderTarget[1] = gbufferBlendDesc;
	alphaBlendingBufferPsoDesc.BlendState.RenderTarget[2] = gbufferBlendDesc;
	alphaBlendingBufferPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	alphaBlendingBufferPsoDesc.SampleMask = UINT_MAX;
	alphaBlendingBufferPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	alphaBlendingBufferPsoDesc.NumRenderTargets = 3;
	alphaBlendingBufferPsoDesc.RTVFormats[0] = inRTVFormat1;
	alphaBlendingBufferPsoDesc.RTVFormats[1] = inRTVFormat2;
	alphaBlendingBufferPsoDesc.RTVFormats[2] = inRTVFormat3;
	alphaBlendingBufferPsoDesc.SampleDesc.Count = 1;
	alphaBlendingBufferPsoDesc.SampleDesc.Quality = 0;
	alphaBlendingBufferPsoDesc.DSVFormat = inDSVFormat;
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&alphaBlendingBufferPsoDesc, IID_PPV_ARGS(&mPSOs["alphaBlendingGBuffer"])));

	return GameResultOk;
}

GameResult PsoManager::BuildMainPassPso(
		ID3D12RootSignature* inRootSignature,
		ComPtr<ID3DBlob>& inVShader,
		ComPtr<ID3DBlob>& inPShader,
		DXGI_FORMAT inRTVFormat1,
		DXGI_FORMAT inRTVFormat2) {
	if (inRootSignature == nullptr || inVShader == nullptr || inPShader == nullptr) return GameResult(S_FALSE, L"Invalid arguments");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC mainPassPsoDesc;

	ZeroMemory(&mainPassPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	mainPassPsoDesc.InputLayout = { nullptr, 0 };
	mainPassPsoDesc.pRootSignature = inRootSignature;
	mainPassPsoDesc.VS = {
		reinterpret_cast<BYTE*>(inVShader->GetBufferPointer()),
		inVShader->GetBufferSize()
	};
	mainPassPsoDesc.PS = {
		reinterpret_cast<BYTE*>(inPShader->GetBufferPointer()),
		inPShader->GetBufferSize()
	};
	mainPassPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	mainPassPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	mainPassPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	mainPassPsoDesc.DepthStencilState.DepthEnable = FALSE;
	mainPassPsoDesc.SampleMask = UINT_MAX;
	mainPassPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	mainPassPsoDesc.NumRenderTargets = 2;
	mainPassPsoDesc.RTVFormats[0] = inRTVFormat1;
	mainPassPsoDesc.RTVFormats[1] = inRTVFormat2;
	mainPassPsoDesc.SampleDesc.Count = 1;
	mainPassPsoDesc.SampleDesc.Quality = 0;
	mainPassPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&mainPassPsoDesc, IID_PPV_ARGS(&mPSOs["mainPass"])));

	return GameResultOk;
}

GameResult PsoManager::BuildPostPassPso(
		ID3D12RootSignature* inRootSignature,
		ComPtr<ID3DBlob>& inVShader,
		ComPtr<ID3DBlob>& inPShader,
		DXGI_FORMAT inRTVFormat) {
	if (inRootSignature == nullptr || inVShader == nullptr || inPShader == nullptr) return GameResult(S_FALSE, L"Invalid arguments");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC postPassPsoDesc;

	ZeroMemory(&postPassPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	postPassPsoDesc.InputLayout = { nullptr, 0 };
	postPassPsoDesc.pRootSignature = inRootSignature;
	postPassPsoDesc.VS = {
		reinterpret_cast<BYTE*>(inVShader->GetBufferPointer()),
		inVShader->GetBufferSize()
	};
	postPassPsoDesc.PS = {
		reinterpret_cast<BYTE*>(inPShader->GetBufferPointer()),
		inPShader->GetBufferSize()
	};
	postPassPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	postPassPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	postPassPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	postPassPsoDesc.DepthStencilState.DepthEnable = FALSE;
	postPassPsoDesc.SampleMask = UINT_MAX;
	postPassPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	postPassPsoDesc.NumRenderTargets = 1;
	postPassPsoDesc.RTVFormats[0] = inRTVFormat;
	postPassPsoDesc.SampleDesc.Count = 1;
	postPassPsoDesc.SampleDesc.Quality = 0;
	postPassPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&postPassPsoDesc, IID_PPV_ARGS(&mPSOs["postPass"])));

	return GameResultOk;
}

GameResult PsoManager::BuildBloomPso(
		ID3D12RootSignature* inRootSignature,
		ComPtr<ID3DBlob>& inVShader,
		ComPtr<ID3DBlob>& inPShader,
		DXGI_FORMAT inRTVFormat) {
	if (inRootSignature == nullptr || inVShader == nullptr || inPShader == nullptr) return GameResult(S_FALSE, L"Invalid arguments");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC bloomPsoDesc;

	ZeroMemory(&bloomPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	bloomPsoDesc.InputLayout = { nullptr, 0 };
	bloomPsoDesc.pRootSignature = inRootSignature;
	bloomPsoDesc.VS = {
		reinterpret_cast<BYTE*>(inVShader->GetBufferPointer()),
		inVShader->GetBufferSize()
	};
	bloomPsoDesc.PS = {
		reinterpret_cast<BYTE*>(inPShader->GetBufferPointer()),
		inPShader->GetBufferSize()
	};
	bloomPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	bloomPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	bloomPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	bloomPsoDesc.DepthStencilState.DepthEnable = FALSE;
	bloomPsoDesc.SampleMask = UINT_MAX;
	bloomPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	bloomPsoDesc.NumRenderTargets = 1;
	bloomPsoDesc.RTVFormats[0] = inRTVFormat;
	bloomPsoDesc.SampleDesc.Count = 1;
	bloomPsoDesc.SampleDesc.Quality = 0;
	bloomPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&bloomPsoDesc, IID_PPV_ARGS(&mPSOs["bloom"])));

	return GameResultOk;
}

GameResult PsoManager::BuildBloomBlurPso(
		ID3D12RootSignature* inRootSignature,
		ComPtr<ID3DBlob>& inVShader,
		ComPtr<ID3DBlob>& inPShader,
		DXGI_FORMAT inRTVFormat) {
	if (inRootSignature == nullptr || inVShader == nullptr || inPShader == nullptr) return GameResult(S_FALSE, L"Invalid arguments");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC bloomBlurPsoDesc;

	ZeroMemory(&bloomBlurPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	bloomBlurPsoDesc.InputLayout = { nullptr, 0 };
	bloomBlurPsoDesc.pRootSignature = inRootSignature;
	bloomBlurPsoDesc.VS = {
		reinterpret_cast<BYTE*>(inVShader->GetBufferPointer()),
		inVShader->GetBufferSize()
	};
	bloomBlurPsoDesc.PS = {
		reinterpret_cast<BYTE*>(inPShader->GetBufferPointer()),
		inPShader->GetBufferSize()
	};
	bloomBlurPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	bloomBlurPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	bloomBlurPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	bloomBlurPsoDesc.DepthStencilState.DepthEnable = FALSE;
	bloomBlurPsoDesc.SampleMask = UINT_MAX;
	bloomBlurPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	bloomBlurPsoDesc.NumRenderTargets = 1;
	bloomBlurPsoDesc.RTVFormats[0] = inRTVFormat;
	bloomBlurPsoDesc.SampleDesc.Count = 1;
	bloomBlurPsoDesc.SampleDesc.Quality = 0;
	bloomBlurPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&bloomBlurPsoDesc, IID_PPV_ARGS(&mPSOs["bloomBlur"])));
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