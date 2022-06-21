#include "DX12Game/VkLowRenderer.h"

#ifdef UsingVulkan

#include <map>
#include <set>
#include <fstream>

namespace {
#ifdef _DEBUG
	const bool EnableValidationLayers = true;
#else
	const bool EnableValidationLayers = false;
#endif

	const std::vector<const char*> ValidationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

	const std::vector<const char*> DeviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	VkResult CreateDebugUtilsMessengerEXT(
		VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugUtilsMessengerEXT* pDebugMessenger) {

		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr)
			return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
		else
			return VK_ERROR_EXTENSION_NOT_PRESENT;
	}

	void DestroyDebugUtilsMessengerEXT(
		VkInstance instance,
		VkDebugUtilsMessengerEXT debugMessenger,
		const VkAllocationCallbacks* pAllocator) {

		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr)
			func(instance, debugMessenger, pAllocator);
	}

	VkSampleCountFlagBits GetMaxUsableSampleCount(VkPhysicalDevice inPhysicalDevice) {
		VkPhysicalDeviceProperties physicalDeviceProperties;
		vkGetPhysicalDeviceProperties(inPhysicalDevice, &physicalDeviceProperties);

		VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
		if (counts & VK_SAMPLE_COUNT_64_BIT) return VK_SAMPLE_COUNT_64_BIT;
		if (counts & VK_SAMPLE_COUNT_32_BIT) return VK_SAMPLE_COUNT_32_BIT;
		if (counts & VK_SAMPLE_COUNT_16_BIT) return VK_SAMPLE_COUNT_16_BIT;
		if (counts & VK_SAMPLE_COUNT_8_BIT) return VK_SAMPLE_COUNT_8_BIT;
		if (counts & VK_SAMPLE_COUNT_4_BIT) return VK_SAMPLE_COUNT_4_BIT;
		if (counts & VK_SAMPLE_COUNT_2_BIT) return VK_SAMPLE_COUNT_2_BIT;

		return VK_SAMPLE_COUNT_1_BIT;
	}
}

VkLowRenderer::~VkLowRenderer() {
	if (!bIsCleaned)
		CleanUp();
}

GameResult VkLowRenderer::Initialize(UINT inClientWidth, UINT inClientHeight, UINT inNumThreads, GLFWwindow* inMainWnd) {
	mClientWidth = inClientWidth;
	mClientHeight = inClientHeight;
	mNumThreads = inNumThreads;
	mMainWindow = inMainWnd;

	CheckGameResult(InitVulkan());

	return GameResultOk;
}

void VkLowRenderer::CleanUp() {
	vkDestroyDevice(mDevice, nullptr);

	vkDestroySurfaceKHR(mInstance, mSurface, nullptr);

	if (EnableValidationLayers)
		DestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger, nullptr);

	vkDestroyInstance(mInstance, nullptr);

	bIsCleaned = true;
}

GameResult VkLowRenderer::OnResize(UINT inClientWidth, UINT inClientHeight) {
	return GameResultOk;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VkLowRenderer::DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {

	Logln("[Validation layer]", pCallbackData->pMessage);

	return VK_FALSE;
}

GameResult VkLowRenderer::CreateShaderModule(const std::vector<char>& inCode, VkShaderModule& outModule) {
	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = inCode.size();
	createInfo.pCode = reinterpret_cast<const std::uint32_t*>(inCode.data());

	if (vkCreateShaderModule(mDevice, &createInfo, nullptr, &outModule) != VK_SUCCESS)
		ReturnGameResult(E_FAIL, L"Failed to create shader module");

	return GameResultOk;
}

GameResult VkLowRenderer::ReadFile(const std::string& inFileName, std::vector<char>& outData) {
	std::ifstream file(inFileName, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		std::wstringstream wsstream;
		wsstream << L"Failed to load file: " << inFileName.c_str();
		ReturnGameResult(S_OK, wsstream.str());
	}

	size_t fileSize = static_cast<size_t>(file.tellg());
	Logln("[Read file] ", inFileName, " size: ", std::to_string(fileSize), " bytes");

	outData.resize(fileSize);

	file.seekg(0);
	file.read(outData.data(), fileSize);

	file.close();

	return GameResultOk;
}

VkLowRenderer::QueueFamilyIndices VkLowRenderer::FindQueueFamilies(VkPhysicalDevice inDevice) {
	QueueFamilyIndices indices;

	std::uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(inDevice, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(inDevice, &queueFamilyCount, queueFamilies.data());

	std::uint32_t i = 0;
	for (const auto& queueFamily : queueFamilies) {
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			indices.mGraphicsFamily = i;

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(inDevice, i, mSurface, &presentSupport);

		if (presentSupport)
			indices.mPresentFamily = i;

		if (indices.IsComplete())
			break;

		++i;
	}

	return indices;
}

GameResult VkLowRenderer::RecreateSwapChain() {
	CheckGameResult(CreateSwapChain());

	return GameResultOk;
}

GameResult VkLowRenderer::CleanUpSwapChain() {
	vkDestroySwapchainKHR(mDevice, mSwapChain, nullptr);

	return GameResultOk;
}

GameResult VkLowRenderer::InitVulkan() {
	CheckGameResult(CreateInstance());
	CheckGameResult(SetUpDebugMessenger());
	CheckGameResult(CreateSurface());
	CheckGameResult(PickPhysicalDevice());
	CheckGameResult(CreateLogicalDevice());
	CheckGameResult(CreateSwapChain());

	return GameResultOk;
}

std::vector<const char*> VkLowRenderer::GetRequiredExtensions() {
	std::uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	WLogln(L"[VkLowRenderer] Required extensions by GLFW:");
	for (std::uint32_t i = 0; i < glfwExtensionCount; ++i)
		Logln("                   ", glfwExtensions[i]);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
	if (EnableValidationLayers)
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	return extensions;
}

GameResult VkLowRenderer::CheckValidationLayersSupport() {
	std::uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	std::vector<const char*> unsupportedLayer;
	bool status = true;

	for (const char* layerName : ValidationLayers) {
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers) {
			if (std::strcmp(layerName, layerProperties.layerName) == 0) {
				layerFound = true;
				break;
			}
		}

		if (!layerFound) {
			unsupportedLayer.push_back(layerName);
			status = false;
		}
	}

	if (!status) {
		std::wstringstream wsstream;

		for (const auto& layerName : unsupportedLayer)
			wsstream << layerName << L" is not supported" << std::endl;

		ReturnGameResult(E_FAIL, wsstream.str());
	}

	return GameResultOk;
}

void VkLowRenderer::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& inCreateInfo) {
	inCreateInfo = {};
	inCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	inCreateInfo.messageSeverity =
		//VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	inCreateInfo.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	inCreateInfo.pfnUserCallback = VkLowRenderer::DebugCallback;
	inCreateInfo.pUserData = nullptr;
}

bool VkLowRenderer::IsDeviceSuitable(VkPhysicalDevice inDevice) {
	QueueFamilyIndices indices = FindQueueFamilies(inDevice);

	bool extensionsSupported = CheckDeviceExtensionsSupport(inDevice);

	bool swapChainAdequate = false;
	if (extensionsSupported) {
		SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(inDevice);

		swapChainAdequate = !swapChainSupport.mFormats.empty() && !swapChainSupport.mPresentModes.empty();
	}

	VkPhysicalDeviceFeatures supportedFeatures;
	vkGetPhysicalDeviceFeatures(inDevice, &supportedFeatures);


	return indices.IsComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

bool VkLowRenderer::CheckDeviceExtensionsSupport(VkPhysicalDevice inDevice) {
	std::uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(inDevice, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(inDevice, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(DeviceExtensions.begin(), DeviceExtensions.end());

	for (const auto& extension : availableExtensions)
		requiredExtensions.erase(extension.extensionName);

	return requiredExtensions.empty();
}

int VkLowRenderer::RateDeviceSuitability(VkPhysicalDevice inDevice) {
	int score = 0;

	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(inDevice, &deviceProperties);

	WLogln(L"[VkLowRenderer] Physical device properties:");
	Logln("                    Device name    : ", deviceProperties.deviceName);
	WLog(L"                    Device type    : ");
	switch (deviceProperties.deviceType) {
	case VK_PHYSICAL_DEVICE_TYPE_OTHER:
		WLogln(L"Other type");
		score += 1000;
		break;
	case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
		WLogln(L"Integrated GPU");
		score += 500;
		break;
	case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
		WLogln(L"Discrete GPU");
		score += 250;
		break;
	case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
		WLogln(L"Virtual GPU");
		score += 100;
		break;
	case VK_PHYSICAL_DEVICE_TYPE_CPU:
		WLogln(L"CPU type");
		score += 50;
		break;
	}
	WLogln(L"                    Driver version : ", std::to_wstring(deviceProperties.driverVersion));
	WLogln(L"                    API version    : ", std::to_wstring(deviceProperties.apiVersion));

	score += deviceProperties.limits.maxImageDimension2D;

	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(inDevice, &deviceFeatures);

	if (!deviceFeatures.geometryShader || !IsDeviceSuitable(inDevice)) {
		WLogln(L"                    Doesn't support geometry shaders or graphics queues");
		return 0;
	}

	WLogln(L"                Supports geometry shaders and graphics queues");

	return score;
}

VkLowRenderer::SwapChainSupportDetails VkLowRenderer::QuerySwapChainSupport(VkPhysicalDevice inDevice) {
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(inDevice, mSurface, &details.mCapabilities);

	std::uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(inDevice, mSurface, &formatCount, nullptr);
	if (formatCount != 0) {
		details.mFormats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(inDevice, mSurface, &formatCount, details.mFormats.data());
	}

	std::uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(inDevice, mSurface, &presentModeCount, nullptr);
	if (presentModeCount != 0) {
		details.mPresentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(inDevice, mSurface, &presentModeCount, details.mPresentModes.data());
	}

	return details;
}

VkSurfaceFormatKHR VkLowRenderer::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& inAvailableFormats) {
	for (const auto& availableFormat : inAvailableFormats) {
		if (availableFormat.format == VK_FORMAT_R8G8B8A8_SRGB &&
			availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			return availableFormat;
	}

	return inAvailableFormats[0];
}

VkPresentModeKHR VkLowRenderer::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& inAvailablePresentModes) {
	for (const auto& availablePresentMode : inAvailablePresentModes) {
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			return availablePresentMode;
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VkLowRenderer::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& inCapabilities) {
	if (inCapabilities.currentExtent.width != UINT32_MAX) {
		return inCapabilities.currentExtent;
	}
	else {
		int width, height;
		glfwGetFramebufferSize(mMainWindow, &width, &height);

		VkExtent2D actualExtent = { static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height) };

		actualExtent.width = std::max(inCapabilities.minImageExtent.width,
			std::min(inCapabilities.maxImageExtent.width, actualExtent.width));

		actualExtent.height = std::max(inCapabilities.minImageExtent.height,
			std::min(inCapabilities.maxImageExtent.height, actualExtent.height));

		return actualExtent;
	}
}

GameResult VkLowRenderer::CreateInstance() {
	if (EnableValidationLayers)
		CheckGameResult(CheckValidationLayersSupport());

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "VkGame";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "Game Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	std::uint32_t availableExtensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(availableExtensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, availableExtensions.data());

	auto requiredExtensions = GetRequiredExtensions();
	std::vector<const char*> missingExtensions;

	for (const auto& requiredExt : requiredExtensions) {
		bool supported = false;

		for (const auto& availableExt : availableExtensions) {
			if (std::strcmp(requiredExt, availableExt.extensionName) == 0) {
				supported = true;
				break;
			}
		}

		if (!supported)
			missingExtensions.push_back(requiredExt);
	}

	WLogln(L"[VkLowRenderer] Unsupported extensions:")
		if (missingExtensions.size() > 0) {
			for (const auto& missingExt : missingExtensions)
				Logln("                    ", missingExt);
			ReturnGameResult(E_FAIL, L"that extensions are not supported");
		}
		else {
			Logln("                    None");
		}

	createInfo.enabledExtensionCount = static_cast<std::uint32_t>(requiredExtensions.size());
	createInfo.ppEnabledExtensionNames = requiredExtensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
	if (EnableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<std::uint32_t>(ValidationLayers.size());
		createInfo.ppEnabledLayerNames = ValidationLayers.data();

		PopulateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
	}
	else {
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = nullptr;
	}

	if (vkCreateInstance(&createInfo, nullptr, &mInstance) != VK_SUCCESS)
		ReturnGameResult(E_FAIL, L"Failed to create instance");

	return GameResultOk;
}

GameResult VkLowRenderer::SetUpDebugMessenger() {
	if (!EnableValidationLayers)
		return GameResultOk;

	VkDebugUtilsMessengerCreateInfoEXT createInfo;
	PopulateDebugMessengerCreateInfo(createInfo);

	if (CreateDebugUtilsMessengerEXT(mInstance, &createInfo, nullptr, &mDebugMessenger) != VK_SUCCESS)
		ReturnGameResult(E_FAIL, L"Failed to create debug messenger");

	return GameResultOk;
}

GameResult VkLowRenderer::CreateSurface() {
	if (glfwCreateWindowSurface(mInstance, mMainWindow, nullptr, &mSurface) != VK_SUCCESS)
		ReturnGameResult(E_FAIL, L"Failed to create window surface");

	return GameResultOk;
}

GameResult VkLowRenderer::PickPhysicalDevice() {
	std::uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr);
	if (deviceCount == 0)
		ReturnGameResult(E_FAIL, L"Failed to find GPUs with Vulkan support");

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data());

	std::multimap<int, VkPhysicalDevice> candidates;

	for (const auto& device : devices) {
		int score = RateDeviceSuitability(device);
		candidates.insert(std::make_pair(score, device));
	}

	if (candidates.rbegin()->first > 0) {
		auto physicalDevice = candidates.rbegin()->second;
		mPhysicalDevice = physicalDevice;
		mMsaaSamples = GetMaxUsableSampleCount(physicalDevice);
		WLogln(L"[VkLowRenderer] Mutil Sample Counts: ", std::to_wstring(mMsaaSamples));
	}
	else
		ReturnGameResult(E_FAIL, L"Failed to find a suitable GPU");

	return GameResultOk;
}

GameResult VkLowRenderer::CreateLogicalDevice() {
	QueueFamilyIndices indices = FindQueueFamilies(mPhysicalDevice);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<std::uint32_t> uniqueQueueFamilies = {
		indices.GetGraphicsFamilyIndex(),
		indices.GetPresentFamilyIndex()
	};

	float queuePriority = 1.0f;
	for (std::uint32_t queueFamily : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeatures = {};
	deviceFeatures.samplerAnisotropy = VK_TRUE;
	deviceFeatures.sampleRateShading = VK_TRUE;

	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queueCreateInfos.size());
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = static_cast<std::uint32_t>(DeviceExtensions.size());
	createInfo.ppEnabledExtensionNames = DeviceExtensions.data();

	if (EnableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<std::uint32_t>(ValidationLayers.size());
		createInfo.ppEnabledLayerNames = ValidationLayers.data();
	}
	else {
		createInfo.enabledLayerCount = 0;
	}

	if (vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mDevice) != VK_SUCCESS)
		ReturnGameResult(E_FAIL, L"Failed to create logical device");

	vkGetDeviceQueue(mDevice, indices.GetGraphicsFamilyIndex(), 0, &mGraphicsQueue);
	vkGetDeviceQueue(mDevice, indices.GetPresentFamilyIndex(), 0, &mPresentQueue);

	return GameResultOk;
}

GameResult VkLowRenderer::CreateSwapChain() {
	SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(mPhysicalDevice);

	VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.mFormats);
	VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.mPresentModes);
	VkExtent2D extent = ChooseSwapExtent(swapChainSupport.mCapabilities);

	std::uint32_t imageCount = SwapChainImageCount;

	if (imageCount < swapChainSupport.mCapabilities.minImageCount)
		imageCount = swapChainSupport.mCapabilities.minImageCount;
	else if (swapChainSupport.mCapabilities.maxImageCount > 0 &&
		imageCount > swapChainSupport.mCapabilities.maxImageCount)
		imageCount = swapChainSupport.mCapabilities.maxImageCount;

	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = mSurface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices indices = FindQueueFamilies(mPhysicalDevice);
	std::uint32_t queueFamilyIndices[] = {
		indices.GetGraphicsFamilyIndex(),
		indices.GetPresentFamilyIndex()
	};

	if (indices.mGraphicsFamily != indices.mPresentFamily) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 1;
		createInfo.pQueueFamilyIndices = nullptr;
	}
	createInfo.preTransform = swapChainSupport.mCapabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(mDevice, &createInfo, nullptr, &mSwapChain) != VK_SUCCESS)
		ReturnGameResult(E_FAIL, L"Failed to create swap chain");

	vkGetSwapchainImagesKHR(mDevice, mSwapChain, &imageCount, nullptr);
	mSwapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(mDevice, mSwapChain, &imageCount, mSwapChainImages.data());

	mSwapChainImageFormat = surfaceFormat.format;
	mSwapChainExtent = extent;

	return GameResultOk;
}

#endif // UsingVulkan