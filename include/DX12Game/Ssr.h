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
		UINT inDistance,
		UINT inMaxFadeDistance,
		UINT inMinFadeDistance,
		float inEdgeFadeLength);

	GameResult OnResize(UINT inNewWidth, UINT inNewHeight);

	void BuildDescriptors(
		CD3DX12_GPU_DESCRIPTOR_HANDLE hDiffuseMapGpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hNormalMapGpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hDepthMapGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hAmbientMapCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hAmbientMapGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hAdditionalMapCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hAdditionalMapGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hAmbientMapCpuRtv,
		UINT inCbvSrvUavDescriptorSize,
		UINT inRtvDescriptorSize);

	void RebuildDescriptors(
		CD3DX12_GPU_DESCRIPTOR_HANDLE hDiffuseMapGpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hNormalMapGpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hDepthMapGpuSrv);

	void ComputeSsr(
		ID3D12GraphicsCommandList* outCmdList,
		const FrameResource* inCurrFrame,
		int inBlurCount);

	void SetPSOs(ID3D12PipelineState* inSsrPso, ID3D12PipelineState* inSsaoBlurPso);

	ID3D12Resource* GetAmbientMap() const;

	UINT GetSsrMapWidth() const;
	UINT GetSsrMapHeight() const;

private:
	void BlurAmbientMap(ID3D12GraphicsCommandList* outCmdList, const FrameResource* inCurrFrame, int inBlurCount);
	void BlurAmbientMap(ID3D12GraphicsCommandList* outCmdList, bool inHorzBlur);

	GameResult BuildResources();

public:
	static const DXGI_FORMAT AmbientMapFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	static const UINT NumRenderTargets = 2;

private:
	ID3D12Device* md3dDevice;
	ID3D12GraphicsCommandList* mCmdList;

	UINT mSsrMapWidth = 0;
	UINT mSsrMapHeight = 0;

	UINT mSsrDistance = 0;
	UINT mMaxFadeDistance = 0;
	UINT mMinFadeDistance = 0;
	float mEdgeFadeLength = 0.0f;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mSsrRootSig;

	ID3D12PipelineState* mSsrPso = nullptr;
	ID3D12PipelineState* mBlurPso = nullptr;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mhDiffuseMapGpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhNormalMapGpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhDepthMapGpuSrv;
	
	Microsoft::WRL::ComPtr<ID3D12Resource> mAmbientMap0;
	Microsoft::WRL::ComPtr<ID3D12Resource> mAmbientMap1;

	// Need two for ping-ponging during blur.
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap0CpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhAmbientMap0GpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap0CpuRtv;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap1CpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhAmbientMap1GpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap1CpuRtv;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;
};