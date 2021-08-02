#include "DX12Game/GBuffer.h"

GameResult GBuffer::Initialize(
	ID3D12Device* inDevice,
	UINT inClientWidth,
	UINT inClientHeight,
	DXGI_FORMAT inDiffuseMapFormat,
	DXGI_FORMAT inNormalMapFormat,
	DXGI_FORMAT inDepthMapFormat) {

	md3dDevice = inDevice;

	mClientWidth = inClientWidth;
	mClientHeight = inClientHeight;

	mDiffuseMapFormat = inDiffuseMapFormat;
	mNormalMapFormat = inNormalMapFormat;
	mDepthMapFormat = inDepthMapFormat;

	return GameResult(S_OK);
}

GameResult GBuffer::BuildDescriptors(
	ID3D12Resource* inDepthStencilBuffer,
	CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
	CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
	CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
	UINT inCbvSrvUavDescriptorSize,
	UINT inRtvDescriptorSize) {

	mhDiffuseMapCpuSrv = hCpuSrv;
	mhDiffuseMapGpuSrv = hGpuSrv;
	mhDiffuseMapCpuRtv = hCpuRtv;

	mhNormalMapCpuSrv = hCpuSrv.Offset(1, inCbvSrvUavDescriptorSize);
	mhNormalMapGpuSrv = hGpuSrv.Offset(1, inCbvSrvUavDescriptorSize);
	mhNormalMapCpuRtv = hCpuRtv.Offset(1, inRtvDescriptorSize);

	mhDepthMapCpuSrv = hCpuSrv.Offset(1, inCbvSrvUavDescriptorSize);
	mhDepthMapGpuSrv = hGpuSrv.Offset(1, inCbvSrvUavDescriptorSize);

	mCbvSrvUavDescriptorSize = inCbvSrvUavDescriptorSize;
	mRtvDescriptorSize = inRtvDescriptorSize;

	CheckGameResult(CreateGBuffer(mClientWidth, mClientHeight, inDepthStencilBuffer));

	return GameResult(S_OK);
}

GameResult GBuffer::OnResize(UINT inClientWidth, UINT inClientHeight, ID3D12Resource* inDepthStencilBuffer) {
	CheckGameResult(CreateGBuffer(mClientWidth, mClientHeight, inDepthStencilBuffer));

	return GameResult(S_OK);
}

ID3D12Resource* GBuffer::GetDiffuseMap() {
	return mGBuffer[0].Get();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::GetDiffuseMapSrv() const {
	mhDiffuseMapGpuSrv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE GBuffer::GetDiffuseMapRtv() const {
	mhDiffuseMapCpuRtv;
}

ID3D12Resource* GBuffer::GetNormalMap() {
	return mGBuffer[1].Get();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::GetNormalMapSrv() const {
	return mhNormalMapGpuSrv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE GBuffer::GetNormalMapRtv() const {
	return mhNormalMapCpuRtv;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::GetDepthMapSrv() const {
	return mhDepthMapGpuSrv;
}

DXGI_FORMAT GBuffer::GetDiffuseMapFormat() const {
	return mDiffuseMapFormat;
}

DXGI_FORMAT GBuffer::GetNormalMapFormat() const {
	return mNormalMapFormat;
}

DXGI_FORMAT GBuffer::GetDepthMapFormat() const {
	return mDepthMapFormat;
}

GameResult GBuffer::CreateGBuffer(UINT inClientWidth, UINT inClientHeight, ID3D12Resource* inDepthStencilBuffer) {
	for (size_t i = 0; i < mGBufferSize; ++i)
		mGBuffer[i] = nullptr;

	D3D12_RESOURCE_DESC rscDesc;
	rscDesc.Alignment = 0;
	rscDesc.Width = mClientWidth;
	rscDesc.Height = mClientHeight;
	rscDesc.DepthOrArraySize = 1;
	rscDesc.MipLevels = 1;
	rscDesc.SampleDesc.Count = 1;
	rscDesc.SampleDesc.Quality = 0;
	rscDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rscDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = rscDesc.MipLevels;

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	//
	// Creates resource for diffuse map.
	//
	rscDesc.Format = mDiffuseMapFormat;
	
	float diffuseClearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	CD3DX12_CLEAR_VALUE optClear(mDiffuseMapFormat, diffuseClearColor);

	md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&rscDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(mGBuffer[0].GetAddressOf())
	);

	//
	// Creates shader resource view for diffuse map.
	//
	srvDesc.Format = mDiffuseMapFormat;

	md3dDevice->CreateShaderResourceView(mGBuffer[0].Get(), &srvDesc, mhDiffuseMapCpuSrv);

	//
	// Creates render target view for diffuse map.
	//
	rtvDesc.Format = mDiffuseMapFormat;

	md3dDevice->CreateRenderTargetView(mGBuffer[0].Get(), &rtvDesc, mhDiffuseMapCpuRtv);


	//
	// Creates resource for normal map.
	//
	rscDesc.Format = mNormalMapFormat;

	float normalClearColor[] = { 0.0f, 0.0f, 1.0f, 0.0f };
	optClear = CD3DX12_CLEAR_VALUE(mNormalMapFormat, normalClearColor);

	md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&rscDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(mGBuffer[1].GetAddressOf())
	);

	//
	// Create shader resource view for normal map.
	//
	srvDesc.Format = mNormalMapFormat;

	md3dDevice->CreateShaderResourceView(mGBuffer[1].Get(), &srvDesc, mhNormalMapCpuSrv);

	//
	// Create render target view for normal map.
	//
	rtvDesc.Format = mNormalMapFormat;

	md3dDevice->CreateRenderTargetView(mGBuffer[1].Get(), &rtvDesc, mhNormalMapCpuRtv);

	//
	// Create shader resource view for depth map.
	//
	srvDesc.Format = mDepthMapFormat;

	md3dDevice->CreateShaderResourceView(inDepthStencilBuffer, &srvDesc, mhDepthMapCpuSrv);

	return GameResult(S_OK);
}