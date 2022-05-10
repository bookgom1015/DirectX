#pragma once

#include "DX12Game/LowRenderer.h"

class VkLowRenderer {
private:
	struct QueueFamilyIndices {
		std::optional<std::uint32_t> mGraphicsFamily;
		std::optional<std::uint32_t> mPresentFamily;

		std::uint32_t GetGraphicsFamilyIndex() {
			return mGraphicsFamily.value();
		}

		std::uint32_t GetPresentFamilyIndex() {
			return mPresentFamily.value();
		}

		bool IsComplete() {
			return mGraphicsFamily.has_value() && mPresentFamily.has_value();
		}
	};

	struct SwapChainSupportDetails {
		VkSurfaceCapabilitiesKHR mCapabilities;
		std::vector<VkSurfaceFormatKHR> mFormats;
		std::vector<VkPresentModeKHR> mPresentModes;
	};

protected:
	VkLowRenderer();

public:
	virtual ~VkLowRenderer();

public:
	GameResult Initialize(
		UINT inClientWidth,
		UINT inClientHeight,
		UINT inNumThreads = 1,
		GLFWwindow* inMainWnd = nullptr);

	void CleanUp();

	GameResult OnResize(UINT inClientWidth, UINT inClientHeight);

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);

protected:
	GameResult CreateShaderModule(const std::vector<char>& inCode, VkShaderModule& outModule);
	GameResult ReadFile(const std::string& inFileName, std::vector<char>& outData);

private:
	GameResult InitVulkan();

	std::vector<const char*> GetRequiredExtensions();
	GameResult CheckValidationLayersSupport();
	GameResult CreateInstance();

	GameResult SetUpDebugMessenger();
	void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& inCreateInfo);

	GameResult CreateSurface();

	GameResult PickPhysicalDevice();

	bool IsDeviceSuitable(VkPhysicalDevice inDevice);

	bool CheckDeviceExtensionsSupport(VkPhysicalDevice inDevice);

	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice inDevice);

	int RateDeviceSuitability(VkPhysicalDevice inDevice);
	VkLowRenderer::QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice inDevice);

	GameResult CreateLogicalDevice();

	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& inAvailableFormats);
	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& inAvailablePresentModes);
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& inCapabilities);

	GameResult CreateSwapChain();
	GameResult CreateImageViews();

	GameResult CreateRenderPass();

	GameResult CreateFramebuffers();

	GameResult CreateCommandPool();

protected:
	GLFWwindow* mMainWindow;

	VkInstance mInstance;
	VkDebugUtilsMessengerEXT mDebugMessenger;

	VkSurfaceKHR mSurface;

	VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
	VkDevice mDevice;

	VkQueue mGraphicsQueue;
	VkQueue mPresentQueue;

	VkSwapchainKHR mSwapChain;
	std::vector<VkImage> mSwapChainImages;
	const std::uint32_t mSwapChainImageCount = 3;
	VkFormat mSwapChainImageFormat;
	VkExtent2D mSwapChainExtent;

	std::vector<VkImageView> mSwapChainImageViews;

	VkRenderPass mRenderPass;
	VkPipelineLayout mPipelineLayout;
	VkPipeline mGraphicsPipeline;

	std::vector<VkFramebuffer> mSwapChainFramebuffers;

	VkCommandPool mCommandPool;
	std::vector<VkCommandBuffer> mCommandBuffers;

	UINT mClientWidth = 0;
	UINT mClientHeight = 0;

	UINT mNumThreads;

private:
	bool bIsCleaned = false;
};