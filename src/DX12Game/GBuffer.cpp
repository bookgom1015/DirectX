#include "DX12Game/GBuffer.h"

GameResult GBuffer::Initialize(
	ID3D12Device* inDevice,
	UINT inClientWidth,
	UINT inClientHeight,
	DXGI_FORMAT inDiffuseMapFormat,
	DXGI_FORMAT inNormalMapFormat) {

	md3dDevice = inDevice;

	mClientWidth = inClientWidth;
	mClientHeight = inClientHeight;

	mDiffuseMapFormat = inDiffuseMapFormat;
	mNormalMapFormat = inNormalMapFormat;

	return GameResult(S_OK);
}

GameResult GBuffer::BuildDescriptors(
	ID3D12Resource* depthStencilBuffer,
	CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
	CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
	CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
	UINT cbvSrvUavDescriptorSize,
	UINT rtvDescriptorSize) {

	mhCpuSrv = hCpuSrv;
	mhGpuSrv = hGpuSrv;
	mhCpuRtv = hCpuRtv;
	mCbvSrvUavDescriptorSize = cbvSrvUavDescriptorSize;
	mRtvDescriptorSize = rtvDescriptorSize;

	CheckGameResult(CreateGBuffer(mClientWidth, mClientHeight));

	return GameResult(S_OK);
}

GameResult GBuffer::OnResize(UINT inClientWidth, UINT inClientHeight) {
	CheckGameResult(CreateGBuffer(mClientWidth, mClientHeight));

	return GameResult(S_OK);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::GetGBufferSrv() {
	return mhGpuSrv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE GBuffer::GetGBufferRtv() {
	return mhCpuRtv;
}

GameResult GBuffer::CreateGBuffer(UINT inClientWidth, UINT inClientHeight) {
	D3D12_RESOURCE_DESC rscDesc;
	rscDesc.Alignment = 0;
	rscDesc.Width = mClientWidth;
	rscDesc.Height = mClientHeight;
	rscDesc.DepthOrArraySize = 1;
	rscDesc.MipLevels = 1;
	rscDesc.SampleDesc.Count = 1;
	rscDesc.SampleDesc.Quality = 0;
	rscDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rscDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

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

	D3D12_CLEAR_VALUE optClear;
	optClear.Color[0] = 0.0f;
	optClear.Color[1] = 0.0f;
	optClear.Color[2] = 0.0f;
	optClear.Color[3] = 1.0f;

	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptorSrv(mhCpuSrv);
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptorRtv(mhCpuRtv);

	//
	// Creates resource for diffuse map.
	//
	rscDesc.Format = mDiffuseMapFormat;
	optClear.Format = mDiffuseMapFormat;

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

	md3dDevice->CreateShaderResourceView(mGBuffer[0].Get(), &srvDesc, hDescriptorSrv);

	//
	// Creates render target view for diffuse map.
	//
	rtvDesc.Format = mDiffuseMapFormat;

	md3dDevice->CreateRenderTargetView(mGBuffer[0].Get(), &rtvDesc, hDescriptorRtv);


	//
	// Creates resource for normal map.
	//
	rscDesc.Format = mNormalMapFormat;
	optClear.Format = mNormalMapFormat;

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
	hDescriptorSrv.Offset(mCbvSrvUavDescriptorSize);

	md3dDevice->CreateShaderResourceView(mGBuffer[1].Get(), &srvDesc, hDescriptorSrv);

	//
	// Create render target view for normal map.
	//
	rtvDesc.Format = mNormalMapFormat;
	hDescriptorRtv.Offset(mRtvDescriptorSize);

	md3dDevice->CreateRenderTargetView(mGBuffer[1].Get(), &rtvDesc, hDescriptorRtv);

	return GameResult(S_OK);
}