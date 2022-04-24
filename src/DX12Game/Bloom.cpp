#include "DX12Game/Bloom.h"

GameResult Bloom::Initialize(
		ID3D12Device* inDevice,
		UINT inClientWidth,
		UINT inClientHeight,
		DXGI_FORMAT inBloomMapFormat) {
	md3dDevice = inDevice;
	mBloomMapFormat = inBloomMapFormat;

	CheckGameResult(BuildResources(inClientWidth, inClientHeight));

	return GameResultOk;
}

void Bloom::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv) {
	mhBloomMapCpuSrv = hCpuSrv;
	mhBloomMapGpuSrv = hGpuSrv;
	mhBloomMapCpuRtv = hCpuRtv;

	RebuildDescriptors();
}

void Bloom::RebuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = mBloomMapFormat;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = mBloomMapFormat;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	// Creates shader resource view for bloom map.
	md3dDevice->CreateShaderResourceView(mBloomMap.Get(), &srvDesc, mhBloomMapCpuSrv);

	// Create render target view for bloom map.
	md3dDevice->CreateRenderTargetView(mBloomMap.Get(), &rtvDesc, mhBloomMapCpuRtv);
}

GameResult Bloom::OnResize(UINT inClientWidth, UINT inClientHeight) {
	CheckGameResult(BuildResources(inClientWidth, inClientHeight));

	return GameResultOk;
}

ID3D12Resource* Bloom::GetBloomMap() {
	return mBloomMap.Get();
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Bloom::GetBloomMapView() {
	return mhBloomMapCpuRtv;
}

DXGI_FORMAT Bloom::GetBloomMapFormat() {
	return mBloomMapFormat;
}

GameResult Bloom::BuildResources(UINT inClientWidth, UINT inClientHeight) {
	// To remove the previous bloom map.
	mBloomMap = nullptr;

	D3D12_RESOURCE_DESC rscDesc = {};
	rscDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rscDesc.Alignment = 0;
	rscDesc.Width = inClientWidth;
	rscDesc.Height = inClientHeight;
	rscDesc.Format = mBloomMapFormat;
	rscDesc.DepthOrArraySize = 1;
	rscDesc.MipLevels = 1;
	rscDesc.SampleDesc.Count = 1;
	rscDesc.SampleDesc.Quality = 0;
	rscDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rscDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	//
	// Creates resource for bloom map.
	//
	float diffuseClearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	CD3DX12_CLEAR_VALUE optClear(mBloomMapFormat, diffuseClearColor);

	ReturnIfFailed(
		md3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&rscDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&optClear,
			IID_PPV_ARGS(mBloomMap.GetAddressOf())
		)
	);

	return GameResultOk;
}