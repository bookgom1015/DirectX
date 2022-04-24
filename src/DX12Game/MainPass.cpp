#include "DX12Game\MainPass.h"

GameResult MainPass::Initialize(
		ID3D12Device* inDevice,
		UINT inClientWidth,
		UINT inClientHeight,
		DXGI_FORMAT inBackBufferFormat) {
	md3dDevice = inDevice;
	mMainPassMapFormat = inBackBufferFormat;

	CheckGameResult(BuildResources(inClientWidth, inClientHeight));

	return GameResultOk;
}

void MainPass::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
		UINT inCbvSrvUavDescriptorSize,
		UINT inRtvDescriptorSize) {
	mhMainPassMapCpuSrv1 = hCpuSrv;
	mhMainPassMapGpuSrv1 = hGpuSrv;
	mhMainPassMapCpuRtv1 = hCpuRtv;

	mhMainPassMapCpuSrv2 = hCpuSrv.Offset(1, inCbvSrvUavDescriptorSize);
	mhMainPassMapGpuSrv2 = hGpuSrv.Offset(1, inCbvSrvUavDescriptorSize);
	mhMainPassMapCpuRtv2 = hCpuRtv.Offset(1, inRtvDescriptorSize);

	RebuildDescriptors();
}

void MainPass::RebuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = mMainPassMapFormat;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = mMainPassMapFormat;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	// Creates shader resource view for main pass maps.
	md3dDevice->CreateShaderResourceView(mMainPassMap1.Get(), &srvDesc, mhMainPassMapCpuSrv1);
	md3dDevice->CreateShaderResourceView(mMainPassMap2.Get(), &srvDesc, mhMainPassMapCpuSrv2);

	// Create render target view for main pass maps.
	md3dDevice->CreateRenderTargetView(mMainPassMap1.Get(), &rtvDesc, mhMainPassMapCpuRtv1);
	md3dDevice->CreateRenderTargetView(mMainPassMap2.Get(), &rtvDesc, mhMainPassMapCpuRtv2);
}

GameResult MainPass::OnResize(UINT inClientWidth, UINT inClientHeight) {
	CheckGameResult(BuildResources(inClientWidth, inClientHeight));

	return GameResultOk;
}

ID3D12Resource* MainPass::GetMainPassMap1() {
	return mMainPassMap1.Get();
}

CD3DX12_CPU_DESCRIPTOR_HANDLE MainPass::GetMainPassMapView1() {
	return mhMainPassMapCpuRtv1;
}

ID3D12Resource* MainPass::GetMainPassMap2() {
	return mMainPassMap2.Get();
}

CD3DX12_CPU_DESCRIPTOR_HANDLE MainPass::GetMainPassMapView2() {
	return mhMainPassMapCpuRtv2;
}

DXGI_FORMAT MainPass::GetMainMpassMapFormat() {
	return mMainPassMapFormat;
}

GameResult MainPass::BuildResources(
		UINT inClientWidth,
		UINT inClientHeight) {
	// To remove the previous main pass maps.
	mMainPassMap1 = nullptr;
	mMainPassMap2 = nullptr;

	D3D12_RESOURCE_DESC rscDesc = {};
	rscDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rscDesc.Alignment = 0;
	rscDesc.Width = inClientWidth;
	rscDesc.Height = inClientHeight;
	rscDesc.Format = mMainPassMapFormat;
	rscDesc.DepthOrArraySize = 1;
	rscDesc.MipLevels = 1;
	rscDesc.SampleDesc.Count = 1;
	rscDesc.SampleDesc.Quality = 0;
	rscDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rscDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	//
	// Creates resource for main pass map.
	//
	float diffuseClearColor[] = { 0.0, 0.0f, 0.0f, 1.0f };
	CD3DX12_CLEAR_VALUE optClear(mMainPassMapFormat, diffuseClearColor);

	ReturnIfFailed(
		md3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&rscDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&optClear,
			IID_PPV_ARGS(mMainPassMap1.GetAddressOf())
		)
	);

	ReturnIfFailed(
		md3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&rscDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&optClear,
			IID_PPV_ARGS(mMainPassMap2.GetAddressOf())
		)
	);

	return GameResultOk;
}