#include "LearningDXR/Renderer.h"

using namespace DirectX;
using namespace DirectX::PackedVector;

Renderer::Renderer() {

}

Renderer::~Renderer() {

}

bool Renderer::Initialize(HWND hMainWnd, UINT inClientWidth /* = 800 */, UINT inClientHeight /* = 600 */) {
	if (!LowRenderer::Initialize(hMainWnd, inClientWidth, inClientHeight))
		return false;

	return true;
}

void Renderer::CleanUp() {
	LowRenderer::CleanUp();


}

bool Renderer::Update(const GameTimer& gt) {
	return true;
}

bool Renderer::Draw() {
	// Reuse the memory associated with command recording
	// We can only reset when the associated command lists have finished execution on the GPU
	if (FAILED(mDirectCmdListAlloc->Reset())) {
		WErrln(L"Failed to reset command-list allocator");
		return false;
	}

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList
	if (FAILED(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr))) {
		WErrln(L"Failed to reset command-list");
		return false;
	}

	// Indicate a state transition on the resource usage
	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)
	);

	// Set the viewport and scissor rect
	// This needs to be reset whenever the command list is reset
	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// Clear the back buffer and depth buffer
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(
		DepthStencilView(), 
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 
		1.0f,
		0, 
		0, 
		nullptr
	);

	// Specify the buffers we are going to render to
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	// Indicate a state transition on the resource usage
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands
	if (FAILED(mCommandList->Close())) {
		WErrln(L"Failed to close command list");
		return false;
	}

	// Add the command list to the queue for execution
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// swap the bakc and front buffers
	if (FAILED(mSwapChain->Present(0, 0))) {
		WErrln(L"Failed to swap back and front buffers");
		return false;
	}
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Wait until frame commands are complete
	// This wating is inefficent and is done for simplicity
	// Later we will show how to organize out rendering code so we do not have to wait per frame
	FlushCommandQueue();

	return true;
}

bool Renderer::OnResize(UINT inClientWidth, UINT inClientHeight) {
	if (!LowRenderer::OnResize(inClientWidth, inClientHeight))
		return false;

	return true;
}