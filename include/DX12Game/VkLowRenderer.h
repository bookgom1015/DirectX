#pragma once

#include "DX12Game/GameCore.h"

#ifdef UsingVulkan

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

class VkLowRenderer {
protected:
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
	VkLowRenderer() = default;

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

	VkLowRenderer::QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice inDevice);

	virtual GameResult RecreateSwapChain();
	virtual GameResult CleanUpSwapChain();

private:
	GameResult InitVulkan();

	std::vector<const char*> GetRequiredExtensions();
	GameResult CheckValidationLayersSupport();

	void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& inCreateInfo);

	bool IsDeviceSuitable(VkPhysicalDevice inDevice);
	bool CheckDeviceExtensionsSupport(VkPhysicalDevice inDevice);
	int RateDeviceSuitability(VkPhysicalDevice inDevice);

	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice inDevice);
	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& inAvailableFormats);
	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& inAvailablePresentModes);
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& inCapabilities);

	GameResult CreateInstance();
	GameResult SetUpDebugMessenger();
	GameResult CreateSurface();
	GameResult PickPhysicalDevice();
	GameResult CreateLogicalDevice();
	GameResult CreateSwapChain();

public:
	static const std::uint32_t SwapChainImageCount = 3;

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
	VkFormat mSwapChainImageFormat;
	VkExtent2D mSwapChainExtent;

	UINT mClientWidth = 0;
	UINT mClientHeight = 0;

	UINT mNumThreads;

	VkSampleCountFlagBits mMsaaSamples = VK_SAMPLE_COUNT_1_BIT;

private:
	bool bIsCleaned = false;
};

#endif // UsingVulkan