#pragma once

#include "DX12Game/GameCore.h"

class Bloom {
public:
	Bloom() = default;
	virtual ~Bloom() = default;

public:
	GameResult Initialize(
		ID3D12Device* inDevice,
		UINT inClientWidth,
		UINT inClientHeight,
		DXGI_FORMAT inBloomMapFormat);

	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv);

	void RebuildDescriptors();

	GameResult OnResize(UINT inClientWidth, UINT inClientHeight);

	ID3D12Resource* GetBloomMap();
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetBloomMapView();

	DXGI_FORMAT GetBloomMapFormat();

private:
	GameResult BuildResources(UINT inClientWidth, UINT inClientHeight);

public:
	static const UINT NumRenderTargets = 1;

private:
	ID3D12Device* md3dDevice;

	Microsoft::WRL::ComPtr<ID3D12Resource> mBloomMap;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhBloomMapCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhBloomMapGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhBloomMapCpuRtv;

	DXGI_FORMAT mBloomMapFormat;
};