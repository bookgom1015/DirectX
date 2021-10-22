#pragma once

#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <d3d12.h>
#include <vector>
#include <wrl.h>

#include "common/d3dx12.h"
#include "DX12Game/GameResult.h"

class AnimationsMap {
public:
	AnimationsMap() = default;
	virtual ~AnimationsMap() = default;

public:
	GameResult Initialize(ID3D12Device* inDevice);

	UINT AddAnimation(const std::string& inClipName, const DirectX::XMFLOAT4* inAnimCurves, 
					  size_t inNumFrames, size_t inNumCurves);

	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv);

	void UpdateAnimationsMap(ID3D12GraphicsCommandList* outCmdList);

	ID3D12Resource* GetAnimationsMap() const;
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetAnimationsMapSrv() const;

	UINT GetLineSize() const;
	double GetInvLineSize() const;

private:
	GameResult BuildResource();
	void BuildDescriptors();

public:
	static const DXGI_FORMAT AnimationsMapFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;

private:
	ID3D12Device* md3dDevice;

	Microsoft::WRL::ComPtr<ID3D12Resource> mAnimsMap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mAnimsMapUploadBuffer;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAnimsMapCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhAnimsMapGpuSrv;

	std::vector<DirectX::XMFLOAT4> mAnimations;

	UINT mCurrIndex = 0;
	UINT mNumSubresources;
};