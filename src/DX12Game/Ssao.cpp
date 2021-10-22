//***************************************************************************************
// Ssao.cpp by Frank Luna (C) 2011 All Rights Reserved.
//***************************************************************************************

#include "DX12Game/Ssao.h"

using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace Microsoft::WRL;

GameResult Ssao::Initialize(
	ID3D12Device* inDevice,
	ID3D12GraphicsCommandList* outCmdList,
	UINT inSsaoMapWidth, UINT inSsaoMapHeight) {

	md3dDevice = inDevice;
	mSsaoMapWidth = inSsaoMapWidth;
	mSsaoMapHeight = inSsaoMapHeight;

	CheckGameResult(OnResize(mSsaoMapWidth, mSsaoMapHeight));

	BuildOffsetVectors();
	CheckGameResult(BuildRandomVectorTexture(outCmdList));

	return GameResultOk;
}

void Ssao::BuildDescriptors(
	CD3DX12_GPU_DESCRIPTOR_HANDLE hNormalMapGpuSrv,
	CD3DX12_CPU_DESCRIPTOR_HANDLE hAmbientMapCpuSrv,
	CD3DX12_GPU_DESCRIPTOR_HANDLE hAmbientMapGpuSrv,
	CD3DX12_CPU_DESCRIPTOR_HANDLE hAdditionalMapCpuSrv,
	CD3DX12_GPU_DESCRIPTOR_HANDLE hAdditionalMapGpuSrv,
	CD3DX12_CPU_DESCRIPTOR_HANDLE hAmbientMapCpuRtv,
	UINT inCbvSrvUavDescriptorSize,
	UINT inRtvDescriptorSize) {

	mhAmbientMap0CpuSrv = hAmbientMapCpuSrv;
	mhAmbientMap0GpuSrv = hAmbientMapGpuSrv;

	mhAmbientMap1CpuSrv = hAdditionalMapCpuSrv;
	mhAmbientMap1GpuSrv = hAdditionalMapGpuSrv;

	mhRandomVectorMapCpuSrv = hAdditionalMapCpuSrv.Offset(1, inCbvSrvUavDescriptorSize);
	mhRandomVectorMapGpuSrv = hAdditionalMapGpuSrv.Offset(1, inCbvSrvUavDescriptorSize);

	mhAmbientMap0CpuRtv = hAmbientMapCpuRtv;
	mhAmbientMap1CpuRtv = hAmbientMapCpuRtv.Offset(1, inRtvDescriptorSize);

	//  Create the descriptors
	RebuildDescriptors(hNormalMapGpuSrv);
}

void Ssao::RebuildDescriptors(CD3DX12_GPU_DESCRIPTOR_HANDLE hNormalMapGpuSrv) {

	mhNormalMapGpuSrv = hNormalMapGpuSrv;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	md3dDevice->CreateShaderResourceView(mRandomVectorMap.Get(), &srvDesc, mhRandomVectorMapCpuSrv);

	srvDesc.Format = Ssao::AmbientMapFormat;
	md3dDevice->CreateShaderResourceView(mAmbientMap0.Get(), &srvDesc, mhAmbientMap0CpuSrv);
	md3dDevice->CreateShaderResourceView(mAmbientMap1.Get(), &srvDesc, mhAmbientMap1CpuSrv);

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = Ssao::AmbientMapFormat;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;
	md3dDevice->CreateRenderTargetView(mAmbientMap0.Get(), &rtvDesc, mhAmbientMap0CpuRtv);
	md3dDevice->CreateRenderTargetView(mAmbientMap1.Get(), &rtvDesc, mhAmbientMap1CpuRtv);
}

void Ssao::SetPSOs(ID3D12PipelineState* inSsaoPso, ID3D12PipelineState* inSsaoBlurPso) {
	mSsaoPso = inSsaoPso;
	mBlurPso = inSsaoBlurPso;
}

GameResult Ssao::OnResize(UINT inNewWidth, UINT inNewHeight) {
	if (mSsaoMapWidth != inNewWidth || mSsaoMapHeight != inNewHeight) {
		mSsaoMapWidth = inNewWidth;
		mSsaoMapHeight = inNewHeight;

		// We render to ambient map at half the resolution.
		mViewport.TopLeftX = 0.0f;
		mViewport.TopLeftY = 0.0f;
		mViewport.Width = static_cast<float>(mSsaoMapWidth);
		mViewport.Height = static_cast<float>(mSsaoMapHeight);
		mViewport.MinDepth = 0.0f;
		mViewport.MaxDepth = 1.0f;

		mScissorRect = { 0, 0, static_cast<int>(mSsaoMapWidth), static_cast<int>(mSsaoMapHeight) };

		CheckGameResult(BuildResources());
	}

	return GameResultOk;
}

void Ssao::ComputeSsao(
	ID3D12GraphicsCommandList* outCmdList,
	const FrameResource* inCurrFrame,
	int inBlurCount) {

	outCmdList->SetPipelineState(mSsaoPso);

	outCmdList->RSSetViewports(1, &mViewport);
	outCmdList->RSSetScissorRects(1, &mScissorRect);
	
	// We compute the initial SSAO to AmbientMap0.
	
	// Change to RENDER_TARGET.
	outCmdList->ResourceBarrier(
		1, 
		&CD3DX12_RESOURCE_BARRIER::Transition(
			mAmbientMap0.Get(),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		)
	);
	
	float clearValue[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	outCmdList->ClearRenderTargetView(mhAmbientMap0CpuRtv, clearValue, 0, nullptr);
	
	// Specify the buffers we are going to render to.
	outCmdList->OMSetRenderTargets(1, &mhAmbientMap0CpuRtv, true, nullptr);
	
	// Bind the constant buffer for this pass.
	auto ssaoCBAddress = inCurrFrame->mSsaoCB.Resource()->GetGPUVirtualAddress();
	outCmdList->SetGraphicsRootConstantBufferView(0, ssaoCBAddress);
	outCmdList->SetGraphicsRoot32BitConstant(1, 0, 0);
	
	// Bind the normal and depth maps.
	outCmdList->SetGraphicsRootDescriptorTable(2, mhNormalMapGpuSrv);
	
	// Bind the random vector map.
	outCmdList->SetGraphicsRootDescriptorTable(3, mhRandomVectorMapGpuSrv);
	
	// Draw fullscreen quad.
	outCmdList->IASetVertexBuffers(0, 0, nullptr);
	outCmdList->IASetIndexBuffer(nullptr);
	outCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	outCmdList->DrawInstanced(6, 1, 0, 0);
	
	// Change back to GENERIC_READ so we can read the texture in a shader.
	outCmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			mAmbientMap0.Get(), 
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_GENERIC_READ
		)
	);
	
	BlurAmbientMap(outCmdList, inCurrFrame, inBlurCount);
}

UINT Ssao::GetSsaoMapWidth() const {
	return mSsaoMapWidth;
}

UINT Ssao::GetSsaoMapHeight() const {
	return mSsaoMapHeight;
}

void Ssao::GetOffsetVectors(DirectX::XMFLOAT4 inOffsets[14]) {
	std::copy(&mOffsets[0], &mOffsets[14], &inOffsets[0]);
}

ID3D12Resource* Ssao::GetAmbientMap() {
	return mAmbientMap0.Get();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Ssao::GetAmbientMapSrv() const {
	return mhAmbientMap0GpuSrv;
}

void Ssao::BlurAmbientMap(ID3D12GraphicsCommandList* outCmdList, const FrameResource* inCurrFrame, int inBlurCount) {
	outCmdList->SetPipelineState(mBlurPso);

	auto ssaoCBAddress = inCurrFrame->mSsaoCB.Resource()->GetGPUVirtualAddress();
	outCmdList->SetGraphicsRootConstantBufferView(0, ssaoCBAddress);

	for (int i = 0; i < inBlurCount; ++i) {
		BlurAmbientMap(outCmdList, true);
		BlurAmbientMap(outCmdList, false);
	}
}

void Ssao::BlurAmbientMap(ID3D12GraphicsCommandList* outCmdList, bool inHorzBlur) {
	ID3D12Resource* output = nullptr;
	CD3DX12_GPU_DESCRIPTOR_HANDLE inputSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE outputRtv;

	// Ping-pong the two ambient map textures as we apply
	// horizontal and vertical blur passes.
	if (inHorzBlur == true) {
		output = mAmbientMap1.Get();
		inputSrv = mhAmbientMap0GpuSrv;
		outputRtv = mhAmbientMap1CpuRtv;
		outCmdList->SetGraphicsRoot32BitConstant(1, 1, 0);
	}
	else {
		output = mAmbientMap0.Get();
		inputSrv = mhAmbientMap1GpuSrv;
		outputRtv = mhAmbientMap0CpuRtv;
		outCmdList->SetGraphicsRoot32BitConstant(1, 0, 0);
	}

	outCmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			output, 
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		)
	);

	float clearValue[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	outCmdList->ClearRenderTargetView(outputRtv, clearValue, 0, nullptr);

	outCmdList->OMSetRenderTargets(1, &outputRtv, true, nullptr);

	// Normal/depth map still bound.
	// Bind the normal and depth maps.
	outCmdList->SetGraphicsRootDescriptorTable(2, mhNormalMapGpuSrv);

	// Bind the input ambient map to second texture table.
	outCmdList->SetGraphicsRootDescriptorTable(3, inputSrv);

	// Draw fullscreen quad.
	outCmdList->IASetVertexBuffers(0, 0, nullptr);
	outCmdList->IASetIndexBuffer(nullptr);
	outCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	outCmdList->DrawInstanced(6, 1, 0, 0);

	outCmdList->ResourceBarrier(
		1, 
		&CD3DX12_RESOURCE_BARRIER::Transition(
			output, 
			D3D12_RESOURCE_STATE_RENDER_TARGET, 
			D3D12_RESOURCE_STATE_GENERIC_READ
		)
	);
}

GameResult Ssao::BuildResources() {
	// Free the old resources if they exist.
	mAmbientMap0 = nullptr;
	mAmbientMap1 = nullptr;

	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	// Ambient occlusion maps are at half resolution.
	texDesc.Width = mSsaoMapWidth;
	texDesc.Height = mSsaoMapHeight;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = Ssao::AmbientMapFormat;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	float ambientClearColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	CD3DX12_CLEAR_VALUE optClear(AmbientMapFormat, ambientClearColor);

	ReturnIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		&optClear,
		IID_PPV_ARGS(&mAmbientMap0)
	));

	ReturnIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		&optClear,
		IID_PPV_ARGS(&mAmbientMap1)
	));

	return GameResultOk;
}

GameResult Ssao::BuildRandomVectorTexture(ID3D12GraphicsCommandList* outCmdList) {
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = 256;
	texDesc.Height = 256;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ReturnIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mRandomVectorMap)
	));

	//
	// In order to copy CPU memory data into our default buffer,
	//  we need to create an intermediate upload heap. 
	//

	const UINT num2DSubresources = texDesc.DepthOrArraySize * texDesc.MipLevels;
	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mRandomVectorMap.Get(), 0, num2DSubresources);

	ReturnIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mRandomVectorMapUploadBuffer.GetAddressOf())
	));

	XMCOLOR initData[256 * 256];
	for (int i = 0; i < 256; ++i) {
		for (int j = 0; j < 256; ++j) {
			// Random vector in [0,1].  We will decompress in shader to [-1,1].
			XMFLOAT3 v(MathHelper::RandF(), MathHelper::RandF(), MathHelper::RandF());

			initData[i * 256 + j] = XMCOLOR(v.x, v.y, v.z, 0.0f);
		}
	}

	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData = initData;
	subResourceData.RowPitch = 256 * sizeof(XMCOLOR);
	subResourceData.SlicePitch = subResourceData.RowPitch * 256;

	//
	// Schedule to copy the data to the default resource, and change states.
	// Note that mCurrSol is put in the GENERIC_READ state so it can be 
	// read by a shader.
	//

	outCmdList->ResourceBarrier(
		1, 
		&CD3DX12_RESOURCE_BARRIER::Transition(
			mRandomVectorMap.Get(),
			D3D12_RESOURCE_STATE_GENERIC_READ, 
			D3D12_RESOURCE_STATE_COPY_DEST
		)
	);
	UpdateSubresources(
		outCmdList, 
		mRandomVectorMap.Get(),
		mRandomVectorMapUploadBuffer.Get(),
		0, 
		0,
		num2DSubresources, 
		&subResourceData
	);
	outCmdList->ResourceBarrier(
		1, 
		&CD3DX12_RESOURCE_BARRIER::Transition(
			mRandomVectorMap.Get(),
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_GENERIC_READ
		)
	);

	return GameResultOk;
}

void Ssao::BuildOffsetVectors() {
	// Start with 14 uniformly distributed vectors.  We choose the 8 corners of the cube
	// and the 6 center points along each cube face.  We always alternate the points on 
	// opposites sides of the cubes.  This way we still get the vectors spread out even
	// if we choose to use less than 14 samples.

	// 8 cube corners
	mOffsets[0] = XMFLOAT4(+1.0f, +1.0f, +1.0f, 0.0f);
	mOffsets[1] = XMFLOAT4(-1.0f, -1.0f, -1.0f, 0.0f);

	mOffsets[2] = XMFLOAT4(-1.0f, +1.0f, +1.0f, 0.0f);
	mOffsets[3] = XMFLOAT4(+1.0f, -1.0f, -1.0f, 0.0f);

	mOffsets[4] = XMFLOAT4(+1.0f, +1.0f, -1.0f, 0.0f);
	mOffsets[5] = XMFLOAT4(-1.0f, -1.0f, +1.0f, 0.0f);

	mOffsets[6] = XMFLOAT4(-1.0f, +1.0f, -1.0f, 0.0f);
	mOffsets[7] = XMFLOAT4(+1.0f, -1.0f, +1.0f, 0.0f);

	// 6 centers of cube faces
	mOffsets[8] = XMFLOAT4(-1.0f, 0.0f, 0.0f, 0.0f);
	mOffsets[9] = XMFLOAT4(+1.0f, 0.0f, 0.0f, 0.0f);

	mOffsets[10] = XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
	mOffsets[11] = XMFLOAT4(0.0f, +1.0f, 0.0f, 0.0f);

	mOffsets[12] = XMFLOAT4(0.0f, 0.0f, -1.0f, 0.0f);
	mOffsets[13] = XMFLOAT4(0.0f, 0.0f, +1.0f, 0.0f);

	for (int i = 0; i < 14; ++i) {
		// Create random lengths in [0.25, 1.0].
		float s = MathHelper::RandF(0.25f, 1.0f);

		XMVECTOR v = s * XMVector4Normalize(XMLoadFloat4(&mOffsets[i]));

		XMStoreFloat4(&mOffsets[i], v);
	}
}