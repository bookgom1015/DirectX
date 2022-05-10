#pragma once

#include "DX12Game/GameCore.h"

class PsoManager {
public:
	enum InputLayouts {
		EBasic,
		ESkinned,
		ENone
	};

public:
	PsoManager() = default;
	virtual ~PsoManager() = default;

public:
	GameResult Initialize(ID3D12Device* inDevice);

	GameResult BuildPso(
		InputLayouts inType,
		ID3D12RootSignature* inRootSignature,
		Microsoft::WRL::ComPtr<ID3DBlob>& inVShader,
		Microsoft::WRL::ComPtr<ID3DBlob>& inPShader,
		D3D12_RASTERIZER_DESC inRasterizerDesc,
		D3D12_DEPTH_STENCIL_DESC inDepthStencilDesc,
		D3D12_BLEND_DESC inBlendDesc,
		D3D12_PRIMITIVE_TOPOLOGY_TYPE inTopologyType,
		const std::vector<DXGI_FORMAT>& inRTVFormats,
		DXGI_FORMAT inDSVFormat,
		const std::string& inName);

	GameResult BuildPso(
		InputLayouts inType,
		ID3D12RootSignature* inRootSignature,
		Microsoft::WRL::ComPtr<ID3DBlob>& inVShader,
		Microsoft::WRL::ComPtr<ID3DBlob>& inGShader,
		Microsoft::WRL::ComPtr<ID3DBlob>& inPShader,
		D3D12_RASTERIZER_DESC inRasterizerDesc,
		D3D12_DEPTH_STENCIL_DESC inDepthStencilDesc,
		D3D12_BLEND_DESC inBlendDesc,
		D3D12_PRIMITIVE_TOPOLOGY_TYPE inTopologyType,
		const std::vector<DXGI_FORMAT>& inRTVFormats,
		DXGI_FORMAT inDSVFormat,
		const std::string& inName);

	ID3D12PipelineState* GetPsoPtr(const std::string& inName);	

	ID3D12PipelineState* GetSkeletonPsoPtr();
	ID3D12PipelineState* GetShadowPsoPtr();
	ID3D12PipelineState* GetSkinnedShadowPsoPtr();
	ID3D12PipelineState* GetDebugPsoPtr();
	ID3D12PipelineState* GetSsaoPsoPtr();
	ID3D12PipelineState* GetSsaoBlurPsoPtr();
	ID3D12PipelineState* GetSsrPsoPtr();
	ID3D12PipelineState* GetSsrBlurPsoPtr();
	ID3D12PipelineState* GetSkyPsoPtr();
	ID3D12PipelineState* GetGBufferPsoPtr();
	ID3D12PipelineState* GetSkinnedGBufferPsoPtr();
	ID3D12PipelineState* GetAlphaBlendingGBufferPsoPtr();
	ID3D12PipelineState* GetMainPassPsoPtr();
	ID3D12PipelineState* GetPostPassPsoPtr();
	ID3D12PipelineState* GetBloomPsoPtr();
	ID3D12PipelineState* GetBloomBlurPsoPtr();

private:
	ID3D12Device* md3dDevice;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mSkinnedInputLayout;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;
};