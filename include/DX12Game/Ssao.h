//***************************************************************************************
// Ssao.h by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

#pragma once
 
#include "DX12Game/GameCore.h"
#include "DX12Game/FrameResource.h"

class Ssao {
public:
	Ssao() = default;
	virtual ~Ssao() = default;

private:
	Ssao(const Ssao& inRef) = delete;
	Ssao(Ssao&& inRVal) = delete;
	Ssao& operator=(const Ssao& inRef) = delete;
	Ssao& operator=(Ssao&& inRVal) = delete;

public:
	GameResult Initialize(
		ID3D12Device* inDevice,
		ID3D12GraphicsCommandList* outCmdList,
		UINT inSsaoMapidth, 
		UINT inSsaoMapHeight,
		DXGI_FORMAT inAmbientMapFormat,
		DXGI_FORMAT inNormalMapFormat);

	void BuildDescriptors(
		CD3DX12_GPU_DESCRIPTOR_HANDLE hNormalMapGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hAmbientMapCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hAmbientMapGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hAdditionalMapCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hAdditionalMapGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hAmbientMapCpuRtv,
		UINT inCbvSrvUavDescriptorSize,
		UINT inRtvDescriptorSize);

	void RebuildDescriptors(CD3DX12_GPU_DESCRIPTOR_HANDLE hNormalMapGpuSrv);

	void SetPSOs(ID3D12PipelineState* inSsaoPso, ID3D12PipelineState* inSsaoBlurPso);

	///<summary>
	/// Call when the backbuffer is resized.  
	///</summary>
	GameResult OnResize(UINT inNewWidth, UINT inNewHeight);

	///<summary>
	/// Changes the render target to the Ambient render target and draws a fullscreen
	/// quad to kick off the pixel shader to compute the AmbientMap.  We still keep the
	/// main depth buffer binded to the pipeline, but depth buffer read/writes
	/// are disabled, as we do not need the depth buffer computing the Ambient map.
	///</summary>
	void ComputeSsao(
		ID3D12GraphicsCommandList* outCmdList,
		const Game::FrameResource* inCurrFrame,
		int inBlurCount);

	UINT GetSsaoMapWidth() const;
	UINT GetSsaoMapHeight() const;

	void GetOffsetVectors(DirectX::XMFLOAT4 inOffsets[14]);

	ID3D12Resource* GetAmbientMap();

	CD3DX12_GPU_DESCRIPTOR_HANDLE GetAmbientMapSrv() const;

	DXGI_FORMAT GetAmbientMapFormat() const;
	DXGI_FORMAT GetNormalMapFormat() const;

private:
	///<summary>
	/// Blurs the ambient map to smooth out the noise caused by only taking a
	/// few random samples per pixel.  We use an edge preserving blur so that 
	/// we do not blur across discontinuities--we want edges to remain edges.
	///</summary>
	void BlurAmbientMap(ID3D12GraphicsCommandList* outCmdList, const Game::FrameResource* inCurrFrame, int inBlurCount);
	void BlurAmbientMap(ID3D12GraphicsCommandList* outCmdList, bool inHorzBlur);

	GameResult BuildResources();
	GameResult BuildRandomVectorTexture(ID3D12GraphicsCommandList* inCmdList);

	void BuildOffsetVectors();

public:
	static const UINT NumRenderTargets = 2;

private:
	ID3D12Device* md3dDevice;

	UINT mSsaoMapWidth;
	UINT mSsaoMapHeight;

	DXGI_FORMAT mAmbientMapFormat;
	DXGI_FORMAT mNormalMapFormat;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mSsaoRootSig;

	ID3D12PipelineState* mSsaoPso = nullptr;
	ID3D12PipelineState* mBlurPso = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> mRandomVectorMap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mRandomVectorMapUploadBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> mAmbientMap0;
	Microsoft::WRL::ComPtr<ID3D12Resource> mAmbientMap1;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mhNormalMapGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhNormalMapCpuRtv;
	
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhRandomVectorMapCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhRandomVectorMapGpuSrv;

	// Need two for ping-ponging during blur.
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap0CpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhAmbientMap0GpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap0CpuRtv;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap1CpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhAmbientMap1GpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap1CpuRtv;

	DirectX::XMFLOAT4 mOffsets[14];

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;
};