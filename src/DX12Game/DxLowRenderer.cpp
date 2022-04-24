#include "DX12Game/DxLowRenderer.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

DxLowRenderer::DxLowRenderer()
	: LowRenderer() {}

DxLowRenderer::~DxLowRenderer() {
	if (!bIsCleaned)
		CleanUp();
}

GameResult DxLowRenderer::Initialize(HWND hMainWnd, UINT inClientWidth, UINT inClientHeight, UINT inNumThreads) {
	mhMainWnd = hMainWnd;
	mClientWidth = inClientWidth;
	mClientHeight = inClientHeight;
	mNumThreads = inNumThreads;

	CheckGameResult(InitDirect3D());

	// Do the initial resize case.
	CheckGameResult(OnResize());

	return GameResult(S_OK);
}

void DxLowRenderer::CleanUp() {
	if (md3dDevice != nullptr)
		FlushCommandQueue();

	bIsCleaned = true;
}

GameResult DxLowRenderer::OnResize(UINT inClientWidth, UINT inClientHeight) {
	mClientWidth = inClientWidth;
	mClientHeight = inClientHeight;

	CheckGameResult(OnResize());

	return GameResult(S_OK);
}

GameResult DxLowRenderer::CreateRtvAndDsvDescriptorHeaps() {
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ReturnIfFailed(md3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ReturnIfFailed(md3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));

	return GameResult(S_OK);
}

GameResult DxLowRenderer::FlushCommandQueue() {
	// Advance the fence value to mark commands up to this fence point.
	++mCurrentFence;

	// Add an instruction to the command queue to set a new fence point.
	// Because we are on the GPU timeline, the new fence point won't be set until the GPU finishes
	// processing all the commands prior to this Signal().
	ReturnIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));

	// Wait until the GPU has compledted commands up to this fence point.
	if (mFence->GetCompletedValue() < mCurrentFence) {
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

		// Fire event when GPU hits current fence.
		ReturnIfFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));

		// Wait until the GPU hits current fence.
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	return GameResult(S_OK);
}

GameResult DxLowRenderer::ExecuteCommandLists() {
	std::vector<ID3D12CommandList*> cmdLists;
	for (UINT i = 0; i < mNumThreads; ++i) {
		ReturnIfFailed(mCommandLists[i]->Close());
		cmdLists.push_back(mCommandLists[i].Get());
	}
	mCommandQueue->ExecuteCommandLists(static_cast<UINT>(cmdLists.size()), cmdLists.data());

	return GameResult(S_OK);
}

ID3D12Resource* DxLowRenderer::CurrentBackBuffer() const {
	return mSwapChainBuffer[mCurrBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE DxLowRenderer::CurrentBackBufferView() const {
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), mCurrBackBuffer, mRtvDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE DxLowRenderer::DepthStencilView() const {
	return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}

ID3D12Device* DxLowRenderer::GetDevice() const {
	return md3dDevice.Get();
}

const std::vector<ComPtr<ID3D12GraphicsCommandList>>& DxLowRenderer::GetCommandLists() const {
	return mCommandLists;
}

ID3D12GraphicsCommandList* DxLowRenderer::GetCommandList(UINT inIdx) const {
	return mCommandLists[inIdx].Get();
}

GameResult DxLowRenderer::Initialize(GLFWwindow* inMainWnd, UINT inClientWidth, UINT inClientHeight, UINT inNumThreads) {
	// Do nothing.
	// This is for Vulkan.

	return GameResult(S_OK);
}


GameResult DxLowRenderer::InitDirect3D() {
#if defined(_DEBUG) 
	// Enable the D3D12 debug layer.
	{
		ComPtr<ID3D12Debug> debugController;
		ReturnIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
		debugController->EnableDebugLayer();
	}
#endif

	ReturnIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mdxgiFactory)));

	// Does not support fullscreen mode.
	ReturnIfFailed(mdxgiFactory->MakeWindowAssociation(mhMainWnd, DXGI_MWA_NO_ALT_ENTER));

	// Try to create hardware device.
	HRESULT hardwareResult = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&md3dDevice));

	// Fallback to WARP device.
	if (FAILED(hardwareResult)) {
		ComPtr<IDXGIAdapter> pWarpAdapter;
		ReturnIfFailed(mdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));

		ReturnIfFailed(D3D12CreateDevice(pWarpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&md3dDevice)));
	}

	ReturnIfFailed(md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
	mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// Check 4X MSAA quality support for our back buffer format.
	// All Direct3D 11 capable devices support 4X MSAA for all render
	// target formats, so we only need to check quality support.
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
	msQualityLevels.Format = mBackBufferFormat;
	msQualityLevels.SampleCount = 4;
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msQualityLevels.NumQualityLevels = 0;
	ReturnIfFailed(md3dDevice->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&msQualityLevels,
		sizeof(msQualityLevels)
	));

#if defined(_DEBUG)
	LogAdapters();
#endif

	CheckGameResult(CreateCommandObjects());
	CheckGameResult(CreateSwapChain());
	CheckGameResult(CreateRtvAndDsvDescriptorHeaps());

	return GameResult(S_OK);
}

GameResult DxLowRenderer::CreateCommandObjects() {
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ReturnIfFailed(md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));

	mCommandAllocators.resize(mNumThreads);
	mCommandLists.resize(mNumThreads);

	for (UINT i = 0; i < mNumThreads; ++i) {
		ReturnIfFailed(
			md3dDevice->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT,
				IID_PPV_ARGS(mCommandAllocators[i].GetAddressOf())
			)
		);

		ReturnIfFailed(
			md3dDevice->CreateCommandList(
				0,
				D3D12_COMMAND_LIST_TYPE_DIRECT,
				mCommandAllocators[i].Get(),
				nullptr,
				IID_PPV_ARGS(mCommandLists[i].GetAddressOf())
			)
		);

		// Start off in a closed state.  This is because the first time we refer 
		//  to the command list we will Reset it, and it needs to be closed before calling Reset.
		mCommandLists[i]->Close();
	}

	return GameResult(S_OK);
}

GameResult DxLowRenderer::CreateSwapChain() {
	// Release the previous swapchain we will be recreating.
	mSwapChain.Reset();

	DXGI_SWAP_CHAIN_DESC sd;
	sd.BufferDesc.Width = mClientWidth;
	sd.BufferDesc.Height = mClientHeight;
	sd.BufferDesc.RefreshRate.Numerator = 60;
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
	ReturnIfFailed(mdxgiFactory->CreateSwapChain(mCommandQueue.Get(), &sd, mSwapChain.GetAddressOf()));

	return GameResult(S_OK);
}

GameResult DxLowRenderer::OnResize() {
	if (!md3dDevice)
		ReturnGameResult(S_FALSE, L"ID3D12Device does not exist");
	if (!mSwapChain)
		ReturnGameResult(S_FALSE, L"IDXGISwapChain does not exist");
#ifdef MT_World
	for (UINT i = 0; i < mNumThreads; ++i) {
		if (!mCommandAllocators[i])
			ReturnGameResult(S_FALSE, L"ID3D12CommandAllocator(idx: " + std::to_wstring(i) + L" does not exist");
	}
#else
	if (!mDirectCmdListAlloc)
		ReturnGameResult(S_FALSE, L"ID3D12CommandAllocator does not exist");
#endif

	// Flush before changing any resources.
	FlushCommandQueue();

	for (UINT i = 0; i < mNumThreads; ++i)
		ReturnIfFailed(mCommandLists[i]->Reset(mCommandAllocators[i].Get(), nullptr));

	// Resize the previous resources we will be creating.
	for (int i = 0; i < SwapChainBufferCount; ++i)
		mSwapChainBuffer[i].Reset();

	// Resize the swap chain.
	ReturnIfFailed(mSwapChain->ResizeBuffers(SwapChainBufferCount, mClientWidth, mClientHeight,
		mBackBufferFormat, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	mCurrBackBuffer = 0;

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < SwapChainBufferCount; ++i) {
		ReturnIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffer[i])));
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
	ReturnIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE, 
		&depthStencilDesc, 
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())
	));

	// Create descriptor to mip level 0 of entire resource using the format of the resource.
	md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), nullptr, DepthStencilView());

	// Transition the resource from its initial state to be used as a depth buffer.
	mCommandLists[0]->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_READ));

	// Execute the resize commands.
#ifdef MT_World
	ExecuteCommandLists();
#else
	ReturnIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
#endif

	// Wait until resize is complete.
	CheckGameResult(FlushCommandQueue());

	// Update the viewport
	mScreenViewport.TopLeftX = 0;
	mScreenViewport.TopLeftY = 0;
	mScreenViewport.Width = static_cast<float>(mClientWidth);
	mScreenViewport.Height = static_cast<float>(mClientHeight);
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;

	mScissorRect = { 0, 0, static_cast<LONG>(mClientWidth), static_cast<LONG>(mClientHeight) };

	return GameResult(S_OK);
}

void DxLowRenderer::LogAdapters() {
	UINT i = 0;
	IDXGIAdapter* adapter = nullptr;
	std::vector<IDXGIAdapter*> adapterList;
	while (mdxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND) {
		DXGI_ADAPTER_DESC desc;
		adapter->GetDesc(&desc);

		std::wstring text = L"***Adapter:\n";
		text += L'\t' + desc.Description;
		WLogln(text.c_str());

		adapterList.push_back(adapter);
		++i;
	}

	for (size_t i = 0; i < adapterList.size(); ++i) {
		LogAdapterOutputs(adapterList[i]);
		ReleaseCom(adapterList[i]);
	}
}

void DxLowRenderer::LogAdapterOutputs(IDXGIAdapter* inAdapter) {
	UINT i = 0;
	IDXGIOutput* output = nullptr;
	while (inAdapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND) {
		DXGI_OUTPUT_DESC desc;
		output->GetDesc(&desc);

		std::wstring text = L"***Output:\n";
		text += L'\t' + desc.DeviceName;
		WLogln(text);

		LogOutputDisplayModes(output, mBackBufferFormat);

		ReleaseCom(output);
		++i;
	}
}

void DxLowRenderer::LogOutputDisplayModes(IDXGIOutput* inOutput, DXGI_FORMAT inFormat) {
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
			L"Width = " + std::to_wstring(x.Width) + L' ' +
			L"Height = " + std::to_wstring(x.Height) + L' ' +
			L"Refresh = " + std::to_wstring(n) + L'/' + std::to_wstring(d);
		WLogln(text);
	}
}