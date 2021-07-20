#pragma once

#include "common/d3dUtil.h"
#include "DX12Game/StringUtil.h"

class LowRenderer {
protected:
	LowRenderer();

private:
	LowRenderer(const LowRenderer& inRef) = delete;
	LowRenderer(LowRenderer&& inRVal) = delete;
	LowRenderer& operator=(const LowRenderer& inRef) = delete;
	LowRenderer& operator=(LowRenderer&& inRVal) = delete;

public:
	virtual ~LowRenderer();

public:
	virtual bool Initialize(HWND hMainWnd, UINT inClientWidth = 800, UINT inClientHeight = 600);
	virtual void CleanUp();

	virtual bool OnResize(UINT inClientWidth, UINT inClientHeight);

	bool FlushCommandQueue();

	ID3D12Resource* CurrentBackBuffer() const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

	bool IsValid() const;

private:
	void LogAdapters();
	void LogAdapterOutputs(IDXGIAdapter* inAdapter);
	void LogOutputDisplayModes(IDXGIOutput* inOutput, DXGI_FORMAT inFormat);

	bool InitDirect3D();

	bool CreateCommandObjects();
	bool CreateSwapChain();
	virtual bool CreateRtvAndDsvDescriptorHeaps();

	bool OnResize();

protected:
	static const int SwapChainBufferCount = 2;

	bool bIsCleanedUp;
	bool bIsValid;

	Microsoft::WRL::ComPtr<IDXGIFactory4> mdxgiFactory;
	Microsoft::WRL::ComPtr<ID3D12Device5> md3dDevice;

	Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
	UINT64 mCurrentFence = 0;

	Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;
	Microsoft::WRL::ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferCount];
	Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;		
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> mCommandList;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;

	D3D12_VIEWPORT mScreenViewport;
	D3D12_RECT mScissorRect;

	D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	UINT mRtvDescriptorSize;
	UINT mDsvDescriptorSize;
	UINT mCbvSrvUavDescriptorSize;

	HWND mhMainWnd;
	UINT mClientWidth;
	UINT mClientHeight;
	UINT mRefreshRate = 60;

	int mCurrBackBuffer = 0;
};