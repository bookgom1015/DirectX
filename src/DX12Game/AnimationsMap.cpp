#include "DX12Game/AnimationsMap.h"

using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace Microsoft::WRL;

namespace {
	const size_t LineSize = 2048;
	const double InvLineSize = static_cast<double>(1.0 / LineSize);
}

AnimationsMap::AnimationsMap(ID3D12Device* inDevice, ID3D12GraphicsCommandList* inCmdList) {
	md3dDevice = inDevice;
	mCommandList = inCmdList;

	mAnimations.resize(LineSize * LineSize);

	BuildResource();
}

UINT AnimationsMap::AddAnimation(const std::string& inClipName, 
		const std::vector<std::vector<DirectX::XMFLOAT4>>& inAnimCurves) {
	size_t numFrames = inAnimCurves.size();
	UINT ret = mCurrIndex;
	
	for (size_t frame = 0; frame < numFrames; ++frame) {
		const auto& animCurves = inAnimCurves[frame];
		UINT paddingSize = static_cast<UINT>(LineSize - animCurves.size());

		for (const auto& curve : animCurves)
			mAnimations[mCurrIndex++] = curve;

		mCurrIndex += paddingSize;
	}

	return ret;
}

void AnimationsMap::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv) {
	// Save references to the descriptors.
	mhAnimsMapCpuSrv = hCpuSrv;
	mhAnimsMapGpuSrv = hGpuSrv;

	//  Create the descriptors
	BuildDescriptors();
}

void AnimationsMap::UpdateAnimationsMap() {
	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData = mAnimations.data();
	subResourceData.RowPitch = LineSize * sizeof(XMFLOAT4);
	subResourceData.SlicePitch = subResourceData.RowPitch * LineSize;

	//
	// Schedule to copy the data to the default resource, and change states.
	// Note that mCurrSol is put in the GENERIC_READ state so it can be 
	// read by a shader.
	//

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mAnimsMap.Get(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST));
	UpdateSubresources(mCommandList, mAnimsMap.Get(), mAnimsMapUploadBuffer.Get(),
		0, 0, mNumSubresources, &subResourceData);
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mAnimsMap.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));
}

ID3D12Resource* AnimationsMap::GetAnimationsMap() const {
	return mAnimsMap.Get();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE AnimationsMap::AnimationsMapSrv() const {
	return mhAnimsMapGpuSrv;
}

UINT AnimationsMap::GetLineSize() const {
	return LineSize;
}

double AnimationsMap::GetInvLineSize() const {
	return InvLineSize;
}

void AnimationsMap::BuildResource() {
	// Free the old resources if they exist.
	mAnimsMap = nullptr;

	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = LineSize;
	texDesc.Height = LineSize;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = AnimationsMap::AnimationsMapFormat;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mAnimsMap)));

	//
	// In order to copy CPU memory data into our default buffer,
	//  we need to create an intermediate upload heap. 
	//

	mNumSubresources = texDesc.DepthOrArraySize * texDesc.MipLevels;
	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mAnimsMap.Get(), 0, mNumSubresources);

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mAnimsMapUploadBuffer.GetAddressOf())));
}

void AnimationsMap::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = AnimationsMap::AnimationsMapFormat;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	md3dDevice->CreateShaderResourceView(mAnimsMap.Get(), &srvDesc, mhAnimsMapCpuSrv);
}