#include "DX12Game/Bloom.h"

GameResult Bloom::Initialize(
		ID3D12Device* inDevice,
		UINT inWidth,
		UINT inHeight,
		DXGI_FORMAT inBloomMapFormat) {
	md3dDevice = inDevice;
	mBloomMapWidth = inWidth;
	mBloomMapHeight = inHeight;
	mBloomMapFormat = inBloomMapFormat;

	CheckGameResult(BuildResources());

	return GameResultOk;
}

void Bloom::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hAdditionalMapCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hAdditionalMapGpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hSourceMapGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
		UINT inCbvSrvUavDescriptorSize,
		UINT inRtvDescriptorSize) {
	mhBloomMapCpuSrv = hCpuSrv;
	mhBloomMapGpuSrv = hGpuSrv;
	mhBloomMapCpuRtv = hCpuRtv;

	mhBlurMapCpuSrv = hCpuSrv.Offset(1, inCbvSrvUavDescriptorSize);
	mhBlurMapGpuSrv = hGpuSrv.Offset(1, inCbvSrvUavDescriptorSize);
	mhBlurMapCpuRtv = hCpuRtv.Offset(1, inRtvDescriptorSize);

	mhAdditionalMapCpuSrv = hAdditionalMapCpuSrv;
	mhAdditionalMapGpuSrv = hAdditionalMapGpuSrv;
	mhAdditionalMapCpuRtv = hCpuRtv.Offset(1, inRtvDescriptorSize);

	mhSourceMapGpuSrv = hSourceMapGpuSrv;

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
	md3dDevice->CreateShaderResourceView(mBlurMap.Get(), &srvDesc, mhBlurMapCpuSrv);
	md3dDevice->CreateShaderResourceView(mAdditionalMap.Get(), &srvDesc, mhAdditionalMapCpuSrv);

	// Create render target view for bloom map.
	md3dDevice->CreateRenderTargetView(mBloomMap.Get(), &rtvDesc, mhBloomMapCpuRtv);
	md3dDevice->CreateRenderTargetView(mBlurMap.Get(), &rtvDesc, mhBlurMapCpuRtv);
	md3dDevice->CreateRenderTargetView(mAdditionalMap.Get(), &rtvDesc, mhAdditionalMapCpuRtv);
}

void Bloom::ComputeBloom(ID3D12GraphicsCommandList* outCmdList, const Game::FrameResource* inCurrFrame, int inBlurCount) {
	outCmdList->SetPipelineState(mBloomPso);

	outCmdList->RSSetViewports(1, &mViewport);
	outCmdList->RSSetScissorRects(1, &mScissorRect);

	// We compute the initial SSAO to AmbientMap0.

	// Change to RENDER_TARGET.
	outCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		mBloomMap.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	));

	float clearValue[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	outCmdList->ClearRenderTargetView(mhBloomMapCpuRtv, clearValue, 0, nullptr);

	// Specify the buffers we are going to render to.
	outCmdList->OMSetRenderTargets(1, &mhBloomMapCpuRtv, true, nullptr);

	// Bind the source map to extract highlights.
	outCmdList->SetGraphicsRootDescriptorTable(0, mhSourceMapGpuSrv);

	// Bind the constant buffer for this pass.
	auto bloomCBAddress = inCurrFrame->mBloomCB.Resource()->GetGPUVirtualAddress();
	outCmdList->SetGraphicsRootConstantBufferView(2, bloomCBAddress);
	outCmdList->SetGraphicsRoot32BitConstant(3, 0, 0);
	
	// Draw fullscreen quad.
	outCmdList->IASetVertexBuffers(0, 0, nullptr);
	outCmdList->IASetIndexBuffer(nullptr);
	outCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	outCmdList->DrawInstanced(6, 1, 0, 0);

	// Change back to GENERIC_READ so we can read the texture in a shader.
	outCmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			mBloomMap.Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		)
	);

	BlurBloomMap(outCmdList, inCurrFrame, inBlurCount);
}

void Bloom::SetPSOs(ID3D12PipelineState* inBloomPso, ID3D12PipelineState* inBloomBlurPso) {
	mBloomPso = inBloomPso;
	mBlurPso = inBloomBlurPso;
}

GameResult Bloom::OnResize(UINT inNewWidth, UINT inNewHeight) {
	if (mBloomMapWidth != inNewWidth || mBloomMapHeight != inNewHeight) {
		mBloomMapWidth = inNewWidth;
		mBloomMapHeight = inNewHeight;

		// We render to ambient map at half the resolution.
		mViewport.TopLeftX = 0.0f;
		mViewport.TopLeftY = 0.0f;
		mViewport.Width = static_cast<float>(inNewWidth);
		mViewport.Height = static_cast<float>(inNewHeight);
		mViewport.MinDepth = 0.0f;
		mViewport.MaxDepth = 1.0f;

		mScissorRect = { 0, 0, static_cast<int>(inNewWidth), static_cast<int>(inNewHeight) };
	}

	CheckGameResult(BuildResources());

	return GameResultOk;
}

ID3D12Resource* Bloom::GetBloomMap() {
	return mBloomMap.Get();
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Bloom::GetBloomMapView() {
	return mhBloomMapCpuRtv;
}

DXGI_FORMAT Bloom::GetBloomMapFormat() const {
	return mBloomMapFormat;
}

UINT Bloom::GetBloomMapWidth() const {
	return mBloomMapWidth;
}

UINT Bloom::GetBloomMapHeight() const {
	return mBloomMapHeight;
}

GameResult Bloom::BuildResources() {
	// To remove the previous bloom map.
	mBloomMap = nullptr;

	D3D12_RESOURCE_DESC rscDesc = {};
	rscDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rscDesc.Alignment = 0;
	rscDesc.Width = mBloomMapWidth;
	rscDesc.Height = mBloomMapHeight;
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

	ReturnIfFailed(
		md3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&rscDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&optClear,
			IID_PPV_ARGS(mBlurMap.GetAddressOf())
		)
	);

	ReturnIfFailed(
		md3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&rscDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&optClear,
			IID_PPV_ARGS(mAdditionalMap.GetAddressOf())
		)
	);

	return GameResultOk;
}

void Bloom::BlurBloomMap(ID3D12GraphicsCommandList* outCmdList, const Game::FrameResource* inCurrFrame, int inBlurCount) {
	outCmdList->SetPipelineState(mBlurPso);

	auto bloomCBAddress = inCurrFrame->mBloomCB.Resource()->GetGPUVirtualAddress();
	outCmdList->SetGraphicsRootConstantBufferView(2, bloomCBAddress);

	for (int i = 0; i < inBlurCount; ++i) {
		BlurBloomMap(outCmdList, true);
		BlurBloomMap(outCmdList, false);
	}
}

void Bloom::BlurBloomMap(ID3D12GraphicsCommandList* outCmdList, bool inHorzBlur) {
	ID3D12Resource* output = nullptr;
	CD3DX12_GPU_DESCRIPTOR_HANDLE inputSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE outputRtv;

	// Ping-pong the two ambient map textures as we apply
	// horizontal and vertical blur passes.
	if (inHorzBlur == true) {
		output = mAdditionalMap.Get();
		inputSrv = mhBloomMapGpuSrv;
		outputRtv = mhAdditionalMapCpuRtv;
		outCmdList->SetGraphicsRoot32BitConstant(3, 1, 0);
	}
	else {
		output = mBlurMap.Get();
		inputSrv = mhAdditionalMapGpuSrv;
		outputRtv = mhBlurMapCpuRtv;
		outCmdList->SetGraphicsRoot32BitConstant(3, 0, 0);
	}

	outCmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			output,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		)
	);

	float clearValue[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	outCmdList->ClearRenderTargetView(outputRtv, clearValue, 0, nullptr);

	outCmdList->OMSetRenderTargets(1, &outputRtv, true, nullptr);

	// Bind the input map.
	outCmdList->SetGraphicsRootDescriptorTable(1, inputSrv);

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
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		)
	);
}