#include "DX12Game/Ssr.h"

GameResult Ssr::Initialize(
	ID3D12Device*				inDevice,
	UINT						inSsrMapWidth, 
	UINT						inSsrMapHeight,
	UINT						inDistance) {

	md3dDevice = inDevice;
	mSsrDistance = inDistance;

	CheckGameResult(OnResize(inSsrMapWidth, inSsrMapHeight));

	return GameResultOk;
}

GameResult Ssr::OnResize(UINT inNewWidth, UINT inNewHeight) {
	if (mSsrMapWidth != inNewWidth || mSsrMapHeight != inNewHeight) {
		mSsrMapWidth = inNewWidth;
		mSsrMapHeight = inNewHeight;

		// We render to ambient map at half the resolution.
		mViewport.TopLeftX = 0.0f;
		mViewport.TopLeftY = 0.0f;
		mViewport.Width = static_cast<float>(mSsrMapWidth);
		mViewport.Height = static_cast<float>(mSsrMapHeight);
		mViewport.MinDepth = 0.0f;
		mViewport.MaxDepth = 1.0f;

		mScissorRect = { 0, 0, static_cast<int>(mSsrMapWidth), static_cast<int>(mSsrMapHeight) };

		CheckGameResult(BuildResources());
	}

	return GameResultOk;
}

void Ssr::BuildDescriptors(
	CD3DX12_GPU_DESCRIPTOR_HANDLE hDiffuseMapGpuSrv,
	CD3DX12_GPU_DESCRIPTOR_HANDLE hNormalMapGpuSrv,
	CD3DX12_GPU_DESCRIPTOR_HANDLE hDepthMapGpuSrv,
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

	mhAmbientMap0CpuRtv = hAmbientMapCpuRtv;
	mhAmbientMap1CpuRtv = hAmbientMapCpuRtv.Offset(1, inRtvDescriptorSize);

	//  Create the descriptors
	RebuildDescriptors(hDiffuseMapGpuSrv, hNormalMapGpuSrv, hDepthMapGpuSrv);
}

void Ssr::RebuildDescriptors(
	CD3DX12_GPU_DESCRIPTOR_HANDLE hDiffuseMapGpuSrv,
	CD3DX12_GPU_DESCRIPTOR_HANDLE hNormalMapGpuSrv,
	CD3DX12_GPU_DESCRIPTOR_HANDLE hDepthMapGpuSrv) {

	mhDiffuseMapGpuSrv = hDiffuseMapGpuSrv;
	mhNormalMapGpuSrv = hNormalMapGpuSrv;
	mhDepthMapGpuSrv = hDepthMapGpuSrv;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = Ssr::AmbientMapFormat;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	md3dDevice->CreateShaderResourceView(mAmbientMap0.Get(), &srvDesc, mhAmbientMap0CpuSrv);
	md3dDevice->CreateShaderResourceView(mAmbientMap1.Get(), &srvDesc, mhAmbientMap1CpuSrv);

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = Ssr::AmbientMapFormat;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;
	md3dDevice->CreateRenderTargetView(mAmbientMap0.Get(), &rtvDesc, mhAmbientMap0CpuRtv);
	md3dDevice->CreateRenderTargetView(mAmbientMap1.Get(), &rtvDesc, mhAmbientMap1CpuRtv);
}

void Ssr::ComputeSsr(
	ID3D12GraphicsCommandList*	outCmdList,
	const FrameResource*		inCurrFrame,
	int							inBlurCount) {

	outCmdList->RSSetViewports(1, &mViewport);
	outCmdList->RSSetScissorRects(1, &mScissorRect);

	// We compute the initial SSAO to AmbientMap0.

	// Change to RENDER_TARGET.
	outCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mAmbientMap0.Get(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));

	float clearValue[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	outCmdList->ClearRenderTargetView(mhAmbientMap0CpuRtv, clearValue, 0, nullptr);

	// Specify the buffers we are going to render to.
	outCmdList->OMSetRenderTargets(1, &mhAmbientMap0CpuRtv, true, nullptr);

	// Bind the constant buffer for this pass.
	auto ssrCBAddress = inCurrFrame->mSsrCB.Resource()->GetGPUVirtualAddress();
	outCmdList->SetGraphicsRootConstantBufferView(0, ssrCBAddress);
	outCmdList->SetGraphicsRoot32BitConstant(1, mSsrDistance, 0);
	outCmdList->SetGraphicsRoot32BitConstant(1, 0, 1);

	// Bind the diffuse, normal and depth maps.
	outCmdList->SetGraphicsRootDescriptorTable(2, mhDiffuseMapGpuSrv);

	outCmdList->SetPipelineState(mSsrPso);

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

void Ssr::SetPSOs(ID3D12PipelineState* inSsrPso, ID3D12PipelineState* inSsrBlurPso) {
	mSsrPso = inSsrPso;
	mBlurPso = inSsrBlurPso;
}

void Ssr::BlurAmbientMap(ID3D12GraphicsCommandList* outCmdList, const FrameResource* inCurrFrame, int inBlurCount) {
	outCmdList->SetPipelineState(mBlurPso);

	auto ssrCBAddress = inCurrFrame->mSsrCB.Resource()->GetGPUVirtualAddress();
	outCmdList->SetGraphicsRootConstantBufferView(0, ssrCBAddress);

	for (int i = 0; i < inBlurCount; ++i) {
		BlurAmbientMap(outCmdList, true);
		BlurAmbientMap(outCmdList, false);
	}
}

ID3D12Resource* Ssr::GetAmbientMap() const  {
	return mAmbientMap0.Get();
}

UINT Ssr::GetSsrMapWidth() const {
	return mSsrMapWidth;
}

UINT Ssr::GetSsrMapHeight() const {
	return mSsrMapHeight;
}

void Ssr::BlurAmbientMap(ID3D12GraphicsCommandList* outCmdList, bool inHorzBlur) {
	ID3D12Resource* output = nullptr;
	CD3DX12_GPU_DESCRIPTOR_HANDLE inputSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE outputRtv;

	// Ping-pong the two ambient map textures as we apply
	// horizontal and vertical blur passes.
	if (inHorzBlur == true) {
		output = mAmbientMap1.Get();
		inputSrv = mhAmbientMap0GpuSrv;
		outputRtv = mhAmbientMap1CpuRtv;
		outCmdList->SetGraphicsRoot32BitConstant(1, 1, 1);
	}
	else {
		output = mAmbientMap0.Get();
		inputSrv = mhAmbientMap1GpuSrv;
		outputRtv = mhAmbientMap0CpuRtv;
		outCmdList->SetGraphicsRoot32BitConstant(1, 0, 1);
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
	//inCmdList->SetGraphicsRootDescriptorTable(2, mhNormalMapGpuSrv);

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

GameResult Ssr::BuildResources() {
	// Free the old resources if they exist.
	mAmbientMap0 = nullptr;
	mAmbientMap1 = nullptr;

	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = mSsrMapWidth;
	texDesc.Height = mSsrMapHeight;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = Ssr::AmbientMapFormat;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	float ambientClearColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	CD3DX12_CLEAR_VALUE optClear(Ssr::AmbientMapFormat, ambientClearColor);

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