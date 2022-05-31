#pragma once

#include "DX12Game/GameCore.h"

#ifndef UsingVulkan

class DxLowRenderer {
protected:
	DxLowRenderer() = default;

public:
	virtual ~DxLowRenderer();

private:
	DxLowRenderer(const DxLowRenderer& src) = delete;
	DxLowRenderer(DxLowRenderer&& src) = delete;
	DxLowRenderer& operator=(const DxLowRenderer& rhs) = delete;
	DxLowRenderer& operator=(DxLowRenderer&& rhs) = delete;

protected:
	GameResult Initialize(
		UINT inClientWidth,
		UINT inClientHeight,
		UINT inNumThreads = 1,
		HWND hMainWnd = NULL);

	void CleanUp();
	GameResult OnResize(UINT inClientWidth, UINT inClientHeight);

	virtual GameResult CreateRtvAndDsvDescriptorHeaps();

	float AspectRatio() const;

	GameResult FlushCommandQueue();
	GameResult ExecuteCommandLists();

	ID3D12Resource* CurrentBackBuffer() const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

	ID3D12Device* GetDevice() const;
	const std::vector<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>>& GetCommandLists() const;
	ID3D12GraphicsCommandList* GetCommandList(UINT inIdx = 0) const;

private:
	GameResult InitDirect3D();
	GameResult CreateCommandObjects();
	GameResult CreateSwapChain();

	GameResult OnResize();

	void LogAdapters();
	void LogAdapterOutputs(IDXGIAdapter* inAdapter);
	void LogOutputDisplayModes(IDXGIOutput* inOutput, DXGI_FORMAT inFormat);

protected:
	HWND mhMainWnd = nullptr; // main window handle

	Microsoft::WRL::ComPtr<IDXGIFactory4> mdxgiFactory;
	Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;
	Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice;

	Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
	UINT64 mCurrentFence = 0;

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
	std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> mCommandAllocators;
	std::vector<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>> mCommandLists;

	static const int SwapChainBufferCount = 2;
	int mCurrBackBuffer = 0;
	Microsoft::WRL::ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferCount];
	Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;

	D3D12_VIEWPORT mScreenViewport;
	D3D12_RECT mScissorRect;

	UINT mRtvDescriptorSize = 0;
	UINT mDsvDescriptorSize = 0;
	UINT mCbvSrvUavDescriptorSize = 0;

	D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

protected:
	UINT mClientWidth = 0;
	UINT mClientHeight = 0;

	UINT mNumThreads;

private:
	bool bIsCleaned = false;
};

#endif // UsingVulkan