#pragma once

#include "DX12Game/GameCore.h"
#include "DX12Game/FrameResource.h"

class Bloom {
public:
	Bloom() = default;
	virtual ~Bloom() = default;

public:
	GameResult Initialize(
		ID3D12Device* inDevice,
		UINT inWidth,
		UINT inHeight,
		DXGI_FORMAT inBloomMapFormat);

	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hAdditionalMapCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hAdditionalMapGpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hSourceMapGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
		UINT inCbvSrvUavDescriptorSize,
		UINT inRtvDescriptorSize);

	void RebuildDescriptors();

	void ComputeBloom(ID3D12GraphicsCommandList* outCmdList, const FrameResource* inCurrFrame, int inBlurCount);

	void SetPSOs(ID3D12PipelineState* inBloomPso, ID3D12PipelineState* inBloomBlurPso);

	GameResult OnResize(UINT inClientWidth, UINT inClientHeight);

	ID3D12Resource* GetBloomMap();
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetBloomMapView();

	DXGI_FORMAT GetBloomMapFormat() const;

	UINT GetBloomMapWidth() const;
	UINT GetBloomMapHeight() const;

private:
	GameResult BuildResources();

	void BlurBloomMap(ID3D12GraphicsCommandList* outCmdList, const FrameResource* inCurrFrame, int inBlurCount);
	void BlurBloomMap(ID3D12GraphicsCommandList* outCmdList, bool inHorzBlur);

public:
	static const UINT NumRenderTargets = 3;

private:
	ID3D12Device* md3dDevice;

	Microsoft::WRL::ComPtr<ID3D12Resource> mBloomMap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mBlurMap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mAdditionalMap;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhBloomMapCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhBloomMapGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhBloomMapCpuRtv;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhBlurMapCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhBlurMapGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhBlurMapCpuRtv;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAdditionalMapCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhAdditionalMapGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAdditionalMapCpuRtv;

	DXGI_FORMAT mBloomMapFormat;

	ID3D12PipelineState* mBloomPso = nullptr;
	ID3D12PipelineState* mBlurPso = nullptr;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;

	UINT mBloomMapWidth;
	UINT mBloomMapHeight;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mhSourceMapGpuSrv;
};