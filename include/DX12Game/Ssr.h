#pragma once

#include "DX12Game/GameCore.h"
#include "DX12Game/FrameResource.h"

class Ssr {
public:
	Ssr() = default;
	virtual ~Ssr() = default;

public:
	GameResult Initialize(
		ID3D12Device* inDevice,
		UINT inClientWidth, 
		UINT inClientHeight,
		DXGI_FORMAT inAmbientMapFormat,
		UINT inDistance,
		UINT inMaxFadeDistance,
		UINT inMinFadeDistance,
		float inEdgeFadeLength);

	GameResult OnResize(UINT inNewWidth, UINT inNewHeight);

	void BuildDescriptors(
		D3D12_CPU_DESCRIPTOR_HANDLE   hDsv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hMainPassMapGpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hNormalMapGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hAmbientMapCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hAmbientMapGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hAdditionalMapCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hAdditionalMapGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hAmbientMapCpuRtv,
		UINT inCbvSrvUavDescriptorSize,
		UINT inRtvDescriptorSize);

	void RebuildDescriptors();

	void ComputeSsr(
		ID3D12GraphicsCommandList* outCmdList,
		const Game::FrameResource* inCurrFrame,
		int inBlurCount);

	void SetPSOs(ID3D12PipelineState* inSsrPso, ID3D12PipelineState* inSsaoBlurPso);

	ID3D12Resource* GetAmbientMap() const;

	UINT GetSsrMapWidth() const;
	UINT GetSsrMapHeight() const;

	DXGI_FORMAT GetAmbientMapFormat() const;

	UINT GetSsrDistance() const;
	UINT GetMaxFadeDistance() const;
	UINT GetMinFadeDistance() const;
	float GetEdgeFadeLength() const;

private:
	void BlurAmbientMap(ID3D12GraphicsCommandList* outCmdList, const Game::FrameResource* inCurrFrame, int inBlurCount);
	void BlurAmbientMap(ID3D12GraphicsCommandList* outCmdList, bool inHorzBlur);

	GameResult BuildResources();

public:
	static const UINT NumRenderTargets = 2;

private:
	ID3D12Device* md3dDevice;
	ID3D12GraphicsCommandList* mCmdList;

	UINT mSsrMapWidth = 0;
	UINT mSsrMapHeight = 0;

	DXGI_FORMAT mAmbientMapFormat;

	UINT mSsrDistance = 0;
	UINT mMaxFadeDistance = 0;
	UINT mMinFadeDistance = 0;
	float mEdgeFadeLength = 0.0f;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mSsrRootSig;

	ID3D12PipelineState* mSsrPso = nullptr;
	ID3D12PipelineState* mBlurPso = nullptr;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mhMainPassMapGpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhNormalMapGpuSrv;
	
	Microsoft::WRL::ComPtr<ID3D12Resource> mAmbientMap0;
	Microsoft::WRL::ComPtr<ID3D12Resource> mAmbientMap1;

	D3D12_CPU_DESCRIPTOR_HANDLE  mhDsv;

	// Need two for ping-ponging during blur.
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap0CpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhAmbientMap0GpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap0CpuRtv;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap1CpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhAmbientMap1GpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap1CpuRtv;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;

	UINT mCbvSrvUavDescriptorSize;
};