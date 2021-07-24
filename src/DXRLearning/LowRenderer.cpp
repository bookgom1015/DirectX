#include "DXRLearning/LowRenderer.h"

using namespace Microsoft::WRL;

LowRenderer::LowRenderer() {
	bIsCleanedUp = false;
	bIsValid = false;
}

LowRenderer::~LowRenderer() {
	if (!bIsCleanedUp)
		CleanUp();
}

bool LowRenderer::Initialize(HWND hMainWnd, UINT inClientWidth /* = 800 */, UINT inClientHeight /* = 600 */) {
	mhMainWnd = hMainWnd;
	mClientWidth = inClientWidth;
	mClientHeight = inClientHeight;

	if (!InitDirect3D()) {
		WErrln(L"Failed to initialize Direct 3D");
		return false;
	}

	if (!OnResize()) {
		WErrln(L"OnResize failed");
		return false;
	}

	bIsValid = true;

	return true;
}

void LowRenderer::CleanUp() {
	if (md3dDevice != nullptr) {
		if (FAILED(FlushCommandQueue()))
			WErrln(L"Failed to flush command queue during cleaning up");
	}

	bIsCleanedUp = true;
}

bool LowRenderer::OnResize(UINT inClientWidth, UINT inClientHeight) {	
	mClientWidth = inClientWidth;
	mClientHeight = inClientHeight;

	if (!OnResize())
		return false;

	return true;
}

bool LowRenderer::FlushCommandQueue() {
	// Advance the fence value to mark commands up to this fence point.
	++mCurrentFence;

	// Add an instruction to the command queue to set a new fence point.
	// Because we are on the GPU timeline, the new fence point won't be set until the GPU finishes
	// processing all the commands prior to this Signal().
	if (FAILED(mCommandQueue->Signal(mFence.Get(), mCurrentFence))) {
		WErrln(L"Failed to sign fence");
		return false;
	}

	// Wait until the GPU has compledted commands up to this fence point.
	if (mFence->GetCompletedValue() < mCurrentFence) {
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

		// Fire event when GPU hits current fence.
		if (FAILED(mFence->SetEventOnCompletion(mCurrentFence, eventHandle))) {
			WErrln(L"Failed to set event");
			return false;
		}

		// Wait until the GPU hits current fence.
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	return true;
}

ID3D12Resource* LowRenderer::CurrentBackBuffer() const {
	return mSwapChainBuffer[mCurrBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE LowRenderer::CurrentBackBufferView() const {
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), mCurrBackBuffer, mRtvDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE LowRenderer::DepthStencilView() const {
	return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}

bool LowRenderer::IsValid() const {
	return bIsValid;
}

void LowRenderer::LogAdapters() {
	UINT i = 0;
	IDXGIAdapter* adapter = nullptr;
	std::vector<IDXGIAdapter*> adapterList;
	while (mdxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND) {
		DXGI_ADAPTER_DESC desc;
		adapter->GetDesc(&desc);

		std::wstring text = L"***Adapter: ";
		text += desc.Description + L'\n';
		WLogln(text.c_str());

		adapterList.push_back(adapter);
		++i;
	}

	for (size_t i = 0; i < adapterList.size(); ++i) {
		LogAdapterOutputs(adapterList[i]);
		ReleaseCom(adapterList[i]);
	}
}

void LowRenderer::LogAdapterOutputs(IDXGIAdapter* inAdapter) {
	UINT i = 0;
	IDXGIOutput* output = nullptr;
	while (inAdapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND) {
		DXGI_OUTPUT_DESC desc;
		output->GetDesc(&desc);

		std::wstring text = L"***Output: ";
		text += desc.DeviceName + L'\n';
		WLogln(text);

		LogOutputDisplayModes(output, mBackBufferFormat);

		ReleaseCom(output);
		++i;
	}
}

void LowRenderer::LogOutputDisplayModes(IDXGIOutput* inOutput, DXGI_FORMAT inFormat) {
	UINT count = 0;
	UINT flags = 0;

	// Call with nullptr to get list count.
	inOutput->GetDisplayModeList(inFormat, flags, &count, nullptr);

	std::vector<DXGI_MODE_DESC> modelList(count);
	inOutput->GetDisplayModeList(inFormat, flags, &count, &modelList[0]);

	for (auto& x : modelList) {
		UINT n = x.RefreshRate.Numerator;
		UINT d = x.RefreshRate.Denominator;
		std::wstring text =
			L"        Width = " + std::to_wstring(x.Width) + L'\n' +
			L"        Height = " + std::to_wstring(x.Height) + L'\n' +
			L"        Refresh = " + std::to_wstring(n) + L'/' + std::to_wstring(d) + L'\n';
		WLogln(text);
	}
}

bool LowRenderer::InitDirect3D() {
	CreateDXGIFactory1(IID_PPV_ARGS(&mdxgiFactory));

#if defined(_DEBUG)
	LogAdapters();
#endif

	// Try to create hardware device.
	HRESULT hardwareResult =  D3D12CreateDevice(
		nullptr,
		D3D_FEATURE_LEVEL_12_1,
		__uuidof(ID3D12Device5),
		static_cast<void**>(&md3dDevice)
	);

	// Fallback to WARP device.
	if (FAILED(hardwareResult)) {
		ComPtr<IDXGIAdapter1> pWarpAdapter;

		HRESULT hr = mdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter));
		if (FAILED(hr)) {
			WErrln(L"Failed to enumerate warp adapter");
			return false;
		}

		hr = D3D12CreateDevice(pWarpAdapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&md3dDevice));
		if (FAILED(hr)) {
			WErrln(L"Failed to create device using warp adapter");
			return false;
		}
	}	
	
	// Checks that device supports ray-tracing.
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 ops = {};
	HRESULT featureSupport = md3dDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &ops, sizeof(ops));

	if (FAILED(featureSupport) || ops.RaytracingTier < D3D12_RAYTRACING_TIER_1_0) {
		WErrln(L"Device or driver does not support ray-tracing");
		return false;
	}

	if (FAILED(md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)))) {
		WErrln(L"Failed to create fence");
		return false;
	}

	mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	if (!CreateCommandObjects())
		return false;
	if (!CreateSwapChain())	
		return false;
	if (!CreateRtvAndDsvDescriptorHeaps())
		return false;

	return true;
}

bool LowRenderer::CreateCommandObjects() {
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	if (FAILED(md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)))) {
		WErrln(L"Failed to create command queue");
		return false;
	}

	if (FAILED(md3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mDirectCmdListAlloc.GetAddressOf())))) {
		WErrln(L"Failed to create command allocator");
		return false;
	}

	if (FAILED(md3dDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		mDirectCmdListAlloc.Get(),	// Associated command allocator
		nullptr,					// Initial PipelineStateObject
		IID_PPV_ARGS(mCommandList.GetAddressOf())))) {
		WErrln(L"Failed to create command list");
		return false;
	}

	// Start off in a closed state.  This is because the first time we refer 
	// to the command list we will Reset it, and it needs to be closed before
	// calling Reset.
	mCommandList->Close();

	return true;
}

bool LowRenderer::CreateSwapChain() {
	// Release the previous swapchain we will be recreating.
	mSwapChain.Reset();

	DXGI_SWAP_CHAIN_DESC sd;
	sd.BufferDesc.Width = mClientWidth;
	sd.BufferDesc.Height = mClientHeight;
	sd.BufferDesc.RefreshRate.Numerator = mRefreshRate;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = mBackBufferFormat;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = SwapChainBufferCount;
	sd.OutputWindow = mhMainWnd;
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// Note: Swap chain uses queue to perfrom flush.
	if (FAILED(mdxgiFactory->CreateSwapChain(mCommandQueue.Get(), &sd, mSwapChain.GetAddressOf()))) {
		WErrln(L"Failed to create swap chain");
		return false;
	}

	return true;
}

bool LowRenderer::CreateRtvAndDsvDescriptorHeaps() {
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	if (FAILED(md3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())))) {
		WErrln(L"Failed to create rtv descriptor heap");
		return false;
	}

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	if (FAILED(md3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())))) {
		WErrln(L"Failed to cretae dsv descriptor heap");
		return false;
	}

	return true;
}

bool LowRenderer::OnResize() {
	if (!md3dDevice) {
		WErrln(L"md3dDevice is invalid");
		return false;
	}
	if (!mSwapChain) {
		WErrln(L"mSwapChain is invalid");
		return false;
	}
	if (!mDirectCmdListAlloc) {
		WErrln(L"mDirectCmdListAlloc is invalid");
		return false;
	}

	// Flush before changing any resources.
	FlushCommandQueue();

	if (FAILED(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr))) {
		WErrln(L"Failed to reset command list");
		return false;
	}

	// Resize the previous resources we will be creating.
	for (int i = 0; i < SwapChainBufferCount; ++i)
		mSwapChainBuffer[i].Reset();

	// Resize the swap chain.
	if (FAILED(mSwapChain->ResizeBuffers(
		SwapChainBufferCount, mClientWidth, mClientHeight, mBackBufferFormat, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH))) {
		WErrln(L"Failed to resize swap chain");
		return false;
	}

	mCurrBackBuffer = 0;

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < SwapChainBufferCount; ++i) {
		if (FAILED(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffer[i])))) {
			WErrln(L"Failed to get buffer from swap chain");
			return false;
		}
		md3dDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
		rtvHeapHandle.Offset(1, mRtvDescriptorSize);
	}

	// Create the depth/stencil buffer and view.
	D3D12_RESOURCE_DESC depthStencilDesc;
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = mClientWidth;
	depthStencilDesc.Height = mClientHeight;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.Format = mDepthStencilFormat;
	depthStencilDesc.SampleDesc.Count = 1;
	depthStencilDesc.SampleDesc.Quality = 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = mDepthStencilFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;

	if (FAILED(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE, &depthStencilDesc, D3D12_RESOURCE_STATE_COMMON, &optClear,
		IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())))) {
		WErrln(L"Failed to create resource for depth/stencil buffer");
		return false;
	}

	// Create descriptor to mip level 0 of entire resource using the format of the resource.
	md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), nullptr, DepthStencilView());

	// Transition the resource from its initial state to be used as a depth buffer.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	// Execute the resize commands.
	if (FAILED(mCommandList->Close())) {
		WErrln(L"Failed to close command list");
		return false;
	}
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	// Wait until resize is complete.
	if (!FlushCommandQueue())
		return false;

	// Update the viewport
	mScreenViewport.TopLeftX = 0;
	mScreenViewport.TopLeftY = 0;
	mScreenViewport.Width = static_cast<float>(mClientWidth);
	mScreenViewport.Height = static_cast<float>(mClientHeight);
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;

	mScissorRect = { 0, 0, static_cast<LONG>(mClientWidth), static_cast<LONG>(mClientHeight) };

	return true;
}