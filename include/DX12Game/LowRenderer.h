#pragma once

#include "DX12Game/GameCore.h"

class LowRenderer {
public:
	LowRenderer();
	virtual ~LowRenderer();

private:
	LowRenderer(const LowRenderer& src) = delete;
	LowRenderer(LowRenderer&& src) = delete;

	LowRenderer& operator=(const LowRenderer& rhs) = delete;
	LowRenderer& operator=(LowRenderer&& rhs) = delete;

protected:
	virtual ID3D12Device* GetDevice() const;
	virtual ID3D12GraphicsCommandList* GetCommandList() const;

	float AspectRatio() const;

	virtual bool IsValid() const;

	virtual bool Initialize(HWND hMainWnd, UINT inWidth, UINT inHeight);
	virtual void Update(const GameTimer& gt) = 0;
	virtual void Draw(const GameTimer& gt) = 0;

	virtual void OnResize(UINT inClientWidth, UINT inClientHeight);

	virtual void CreateRtvAndDsvDescriptorHeaps();

	void FlushCommandQueue();

	ID3D12Resource* CurrentBackBuffer() const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

private:
	bool InitDirect3D();
	void CreateCommandObjects();
	void CreateSwapChain();

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
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;

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

	UINT mClientWidth = 0;
	UINT mClientHeight = 0;
};