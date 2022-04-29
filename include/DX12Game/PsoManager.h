#pragma once

#include "DX12Game/GameCore.h"

class PsoManager {
public:
	PsoManager() = default;
	virtual ~PsoManager() = default;

public:
	GameResult Initialize(ID3D12Device* inDevice);

	GameResult BuildSkeletonPso(
		ID3D12RootSignature* inRootSignature,
		Microsoft::WRL::ComPtr<ID3DBlob>& inVShader,
		Microsoft::WRL::ComPtr<ID3DBlob>& inGShader,
		Microsoft::WRL::ComPtr<ID3DBlob>& inPShader,
		DXGI_FORMAT inRTVFormat);
	
	GameResult BuildShadowPso(
		ID3D12RootSignature* inRootSignature,
		Microsoft::WRL::ComPtr<ID3DBlob>& inVShader,
		Microsoft::WRL::ComPtr<ID3DBlob>& inPShader,
		DXGI_FORMAT inDSVFormat);

	GameResult BuildSkinnedShadowPso(
		ID3D12RootSignature* inRootSignature,
		Microsoft::WRL::ComPtr<ID3DBlob>& inVShader,
		Microsoft::WRL::ComPtr<ID3DBlob>& inPShader,
		DXGI_FORMAT inDSVFormat);

	GameResult BuildDebugPso(
		ID3D12RootSignature* inRootSignature,
		Microsoft::WRL::ComPtr<ID3DBlob>& inVShader,
		Microsoft::WRL::ComPtr<ID3DBlob>& inPShader,
		DXGI_FORMAT inRTVFormat);

	GameResult BuildSsaoPso(
		ID3D12RootSignature* inRootSignature,
		Microsoft::WRL::ComPtr<ID3DBlob>& inVShader,
		Microsoft::WRL::ComPtr<ID3DBlob>& inPShader,
		DXGI_FORMAT inRTVFormat);

	GameResult BuildSsaoBlurPso(
		ID3D12RootSignature* inRootSignature,
		Microsoft::WRL::ComPtr<ID3DBlob>& inVShader,
		Microsoft::WRL::ComPtr<ID3DBlob>& inPShader,
		DXGI_FORMAT inRTVFormat);

	GameResult BuildSsrPso(
		ID3D12RootSignature* inRootSignature,
		Microsoft::WRL::ComPtr<ID3DBlob>& inVShader,
		Microsoft::WRL::ComPtr<ID3DBlob>& inPShader,
		DXGI_FORMAT inRTVFormat);

	GameResult BuildSsrBlurPso(
		ID3D12RootSignature* inRootSignature,
		Microsoft::WRL::ComPtr<ID3DBlob>& inVShader,
		Microsoft::WRL::ComPtr<ID3DBlob>& inPShader,
		DXGI_FORMAT inRTVFormat);

	GameResult BuildSkyPso(
		ID3D12RootSignature* inRootSignature,
		Microsoft::WRL::ComPtr<ID3DBlob>& inVShader,
		Microsoft::WRL::ComPtr<ID3DBlob>& inPShader,
		DXGI_FORMAT inRTVFormat,
		DXGI_FORMAT inDSVFormat);

	GameResult BuildGBufferPso(
		ID3D12RootSignature* inRootSignature,
		Microsoft::WRL::ComPtr<ID3DBlob>& inVShader,
		Microsoft::WRL::ComPtr<ID3DBlob>& inPShader,
		DXGI_FORMAT inRTVFormat1,
		DXGI_FORMAT inRTVFormat2,
		DXGI_FORMAT inRTVFormat3,
		DXGI_FORMAT inDSVFormat);

	GameResult BuildSkinnedGBufferPso(
		ID3D12RootSignature* inRootSignature,
		Microsoft::WRL::ComPtr<ID3DBlob>& inVShader,
		Microsoft::WRL::ComPtr<ID3DBlob>& inPShader,
		DXGI_FORMAT inRTVFormat1,
		DXGI_FORMAT inRTVFormat2,
		DXGI_FORMAT inRTVFormat3,
		DXGI_FORMAT inDSVFormat);

	GameResult BuildAlphaBlendingGBufferPso(
		ID3D12RootSignature* inRootSignature,
		Microsoft::WRL::ComPtr<ID3DBlob>& inVShader,
		Microsoft::WRL::ComPtr<ID3DBlob>& inPShader,
		DXGI_FORMAT inRTVFormat1,
		DXGI_FORMAT inRTVFormat2,
		DXGI_FORMAT inRTVFormat3,
		DXGI_FORMAT inDSVFormat);

	GameResult BuildMainPassPso(
		ID3D12RootSignature* inRootSignature,
		Microsoft::WRL::ComPtr<ID3DBlob>& inVShader,
		Microsoft::WRL::ComPtr<ID3DBlob>& inPShader,
		DXGI_FORMAT inRTVFormat1,
		DXGI_FORMAT inRTVFormat2);

	GameResult BuildPostPassPso(
		ID3D12RootSignature* inRootSignature,
		Microsoft::WRL::ComPtr<ID3DBlob>& inVShader,
		Microsoft::WRL::ComPtr<ID3DBlob>& inPShader,
		DXGI_FORMAT inRTVFormat);

	GameResult BuildBloomPso(
		ID3D12RootSignature* inRootSignature,
		Microsoft::WRL::ComPtr<ID3DBlob>& inVShader,
		Microsoft::WRL::ComPtr<ID3DBlob>& inPShader,
		DXGI_FORMAT inRTVFormat);

	GameResult BuildBloomBlurPso(
		ID3D12RootSignature* inRootSignature,
		Microsoft::WRL::ComPtr<ID3DBlob>& inVShader,
		Microsoft::WRL::ComPtr<ID3DBlob>& inPShader,
		DXGI_FORMAT inRTVFormat);

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