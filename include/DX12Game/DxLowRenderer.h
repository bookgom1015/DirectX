#pragma once

#include "DX12Game/LowRenderer.h"

class DxLowRenderer : public LowRenderer {
public:
	DxLowRenderer();
	virtual ~DxLowRenderer();

private:
	DxLowRenderer(const DxLowRenderer& src) = delete;
	DxLowRenderer(DxLowRenderer&& src) = delete;
	DxLowRenderer& operator=(const DxLowRenderer& rhs) = delete;
	DxLowRenderer& operator=(DxLowRenderer&& rhs) = delete;

protected:
	virtual GameResult Initialize(HWND hMainWnd, 
		UINT inClientWidth, UINT inClientHeight, UINT inNumThreads = 1) override;
	virtual void CleanUp() override;
	virtual GameResult OnResize(UINT inClientWidth, UINT inClientHeight) override;

	virtual GameResult CreateRtvAndDsvDescriptorHeaps();

	GameResult FlushCommandQueue();
	GameResult ExecuteCommandLists();

	ID3D12Resource* CurrentBackBuffer() const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

	ID3D12Device* GetDevice() const;
#ifdef MT_World
	const GVector<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>>& GetCommandLists() const;
	ID3D12GraphicsCommandList* GetCommandList(UINT inIdx) const;
#else
	ID3D12GraphicsCommandList* GetCommandList() const;
#endif

private:
	virtual GameResult Initialize(GLFWwindow* inMainWnd, 
		UINT inClientWidth, UINT inClientHeight, UINT inNumThreads = 1) override;

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
#ifdef MT_World
	GVector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> mCommandAllocators;
	GVector< Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>> mCommandLists;
#else
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;
#endif

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

private:
	bool bIsCleaned = false;
};