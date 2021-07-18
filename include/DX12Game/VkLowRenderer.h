#pragma once

#include "DX12Game/LowRenderer.h"

class VkLowRenderer : public LowRenderer {
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
	virtual GameResult Initialize(GLFWwindow* inMainWnd, UINT inClientWidth, UINT inClientHeight) override;
	virtual void CleanUp() override;

	virtual GameResult OnResize(UINT inClientWidth, UINT inClientHeight) override;

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);

private:
	virtual GameResult Initialize(HWND hMainWnd, UINT inClientWidth, UINT inClientHeight) override;

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

private:
	bool bIsCleaned = false;

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
	VkFormat mSwapChainImageFormat;
	VkExtent2D mSwapChainExtent;
};