#pragma once

#include "DX12Game/GameCore.h"
#include "DX12Game/FrameResource.h"

class MainPass {
public:
	MainPass() = default;
	virtual ~MainPass() = default;

public:
	GameResult Initialize(
		ID3D12Device* inDevice,
		UINT inClientWidth,
		UINT inClientHeight,
		DXGI_FORMAT inMainPassMapFormat);

	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
		UINT inCbvSrvUavDescriptorSize,
		UINT inRtvDescriptorSize);

	void RebuildDescriptors();

	GameResult OnResize(UINT inClientWidth, UINT inClientHeight);

	ID3D12Resource* GetMainPassMap1();
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetMainPassMapView1();

	ID3D12Resource* GetMainPassMap2();
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetMainPassMapView2();

	DXGI_FORMAT GetMainMpassMapFormat();
	
private:
	GameResult BuildResources(
		UINT inClientWidth,
		UINT inClientHeight);

public:
	static const UINT NumRenderTargets = 2;

private:
	ID3D12Device* md3dDevice;
	
	///
	// Save the results of the main pass in two maps.
	// The reason is to defer the calculation of the addition of ambient and reflected light to the post pass.
	///

	//* The map in which to store the specular light.
	Microsoft::WRL::ComPtr<ID3D12Resource> mMainPassMap1;
	//* The map in which to store ambient and direct light. It also stores alpha values.
	Microsoft::WRL::ComPtr<ID3D12Resource> mMainPassMap2;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhMainPassMapCpuSrv1;	
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhMainPassMapGpuSrv1;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhMainPassMapCpuRtv1;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhMainPassMapCpuSrv2;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhMainPassMapGpuSrv2;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhMainPassMapCpuRtv2;

	DXGI_FORMAT mMainPassMapFormat;
};