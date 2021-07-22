#include "DX12Game/VkLowRenderer.h"

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

	GameResult ReadFile(const std::string& inFileName, std::vector<char>& outData) {
		std::ifstream file(inFileName, std::ios::ate | std::ios::binary);

		if (!file.is_open()) {
			std::wstringstream wsstream;
			wsstream << L"Failed to load file: " << inFileName.c_str();
			ReturnGameResult(S_OK, wsstream.str());
		}

		size_t fileSize = static_cast<size_t>(file.tellg());
		Logln(inFileName, " size: ", std::to_string(fileSize), " bytes");

		outData.resize(fileSize);

		file.seekg(0);
		file.read(outData.data(), fileSize);

		file.close();

		return GameResult(S_OK);
	}
}

VkLowRenderer::VkLowRenderer() {

}

VkLowRenderer::~VkLowRenderer() {
	if (!bIsCleaned)
		CleanUp();
}

GameResult VkLowRenderer::Initialize(GLFWwindow* inMainWnd, UINT inClientWidth, UINT inClientHeight) {
	mMainWindow = inMainWnd;
	mClientWidth = inClientWidth;
	mClientHeight = inClientHeight;

	CheckGameResult(InitVulkan());

	return GameResult(S_OK);
}

void VkLowRenderer::CleanUp() {
	for (auto framebuffer : mSwapChainFramebuffers)
		vkDestroyFramebuffer(mDevice, framebuffer, nullptr);

	vkDestroyPipeline(mDevice, mGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
	vkDestroyRenderPass(mDevice, mRenderPass, nullptr);

	for (auto imageView : mSwapChainImageViews)
		vkDestroyImageView(mDevice, imageView, nullptr);

	vkDestroySwapchainKHR(mDevice, mSwapChain, nullptr);

	vkDestroyDevice(mDevice, nullptr);

	vkDestroySurfaceKHR(mInstance, mSurface, nullptr);

	if (EnableValidationLayers)
		DestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger, nullptr);

	vkDestroyInstance(mInstance, nullptr);

	bIsCleaned = true;
}

GameResult VkLowRenderer::OnResize(UINT inClientWidth, UINT inClientHeight) {


	return GameResult(S_OK);
}

VKAPI_ATTR VkBool32 VKAPI_CALL VkLowRenderer::DebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {

	Logln("[Validation layer]", pCallbackData->pMessage);

	return VK_FALSE;
}

GameResult VkLowRenderer::Initialize(HWND hMainWnd, UINT inClientWidth, UINT inClientHeight) {
	// Do nothing.
	// This is for D3D12.

	return GameResult(S_OK);
}

GameResult VkLowRenderer::InitVulkan() {
	CheckGameResult(CreateInstance());
	CheckGameResult(SetUpDebugMessenger());
	CheckGameResult(CreateSurface());
	CheckGameResult(PickPhysicalDevice());
	CheckGameResult(CreateLogicalDevice());
	CheckGameResult(CreateSwapChain());
	CheckGameResult(CreateImageViews());
	CheckGameResult(CreateRenderPass());
	CheckGameResult(CreateGraphicsPipeline());
	CheckGameResult(CreateFramebuffers());

	return GameResult(S_OK);
}

std::vector<const char*> VkLowRenderer::GetRequiredExtensions() {
	std::uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	WLogln(L"Required extensions by GLFW:");
	for (std::uint32_t i = 0; i < glfwExtensionCount; ++i)
		Logln("   ", glfwExtensions[i]);

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

		ReturnGameResult(S_FALSE, wsstream.str());
	}

	return GameResult(S_OK);
}

GameResult VkLowRenderer::CreateInstance() {
	if (EnableValidationLayers)
		CheckGameResult(CheckValidationLayersSupport());

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "VkGame";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
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

	WLogln(L"Not supported extensions:")
	if (missingExtensions.size() > 0) {
		for (const auto& missingExt : missingExtensions)
			Logln("    ", missingExt);
		ReturnGameResult(S_FALSE, L"that extensions are not supported");
	}
	else {
		Logln("    None");
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
		ReturnGameResult(S_FALSE, L"Failed to create instance");

	return GameResult(S_OK);
}

GameResult VkLowRenderer::SetUpDebugMessenger() {
	if (!EnableValidationLayers)
		return GameResult(S_OK);

	VkDebugUtilsMessengerCreateInfoEXT createInfo;
	PopulateDebugMessengerCreateInfo(createInfo);

	if (CreateDebugUtilsMessengerEXT(mInstance, &createInfo, nullptr, &mDebugMessenger) != VK_SUCCESS)
		ReturnGameResult(S_FALSE, L"Failed to create debug messenger");

	return GameResult(S_OK);
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

GameResult VkLowRenderer::CreateSurface() {
	if (glfwCreateWindowSurface(mInstance, mMainWindow, nullptr, &mSurface) != VK_SUCCESS)
		ReturnGameResult(S_FALSE, L"Failed to create window surface");

	return GameResult(S_OK);
}

GameResult VkLowRenderer::PickPhysicalDevice() {
	std::uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr);
	if (deviceCount == 0)
		ReturnGameResult(S_FALSE, L"Failed to find GPUs with Vulkan support");

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data());

	std::multimap<int, VkPhysicalDevice> candidates;

	for (const auto& device : devices) {
		int score = RateDeviceSuitability(device);
		candidates.insert(std::make_pair(score, device));
	}

	if (candidates.rbegin()->first > 0)
		mPhysicalDevice = candidates.rbegin()->second;
	else
		ReturnGameResult(S_FALSE, L"Failed to find a suitable GPU");

	return GameResult(S_OK);
}

bool VkLowRenderer::IsDeviceSuitable(VkPhysicalDevice inDevice) {
	QueueFamilyIndices indices = FindQueueFamilies(inDevice);

	bool extensionsSupported = CheckDeviceExtensionsSupport(inDevice);

	bool swapChainAdequate = false;
	if (extensionsSupported) {
		SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(inDevice);

		swapChainAdequate = !swapChainSupport.mFormats.empty() && !swapChainSupport.mPresentModes.empty();
	}

	return indices.IsComplete() && extensionsSupported && swapChainAdequate;
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

int VkLowRenderer::RateDeviceSuitability(VkPhysicalDevice inDevice) {
	int score = 0;

	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(inDevice, &deviceProperties);

	WLogln(L"Physical device properties:");
	Logln("    Device name    : ", deviceProperties.deviceName);
	WLog(L"    Device type    : ");
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
	WLogln(L"    Driver version : ", std::to_wstring(deviceProperties.driverVersion));
	WLogln(L"    API version    : ", std::to_wstring(deviceProperties.apiVersion));

	score += deviceProperties.limits.maxImageDimension2D;

	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(inDevice, &deviceFeatures);

	if (!deviceFeatures.geometryShader || !IsDeviceSuitable(inDevice)) {
		WLogln(L"    Doesn't support geometry shaders or graphics queues");
		return 0;
	}

	WLogln(L"    Supports geometry shaders and graphics queues");

	return score;
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
		ReturnGameResult(S_FALSE, L"Failed to create logical device");

	vkGetDeviceQueue(mDevice, indices.GetGraphicsFamilyIndex(), 0, &mGraphicsQueue);
	vkGetDeviceQueue(mDevice, indices.GetPresentFamilyIndex(), 0, &mPresentQueue);

	return GameResult(S_OK);
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
		VkExtent2D actualExtent = { mClientWidth, mClientHeight };

		actualExtent.width = std::max(inCapabilities.minImageExtent.width,
			std::min(inCapabilities.maxImageExtent.width, actualExtent.width));

		actualExtent.height = std::max(inCapabilities.minImageExtent.height,
			std::min(inCapabilities.maxImageExtent.height, actualExtent.height));

		return actualExtent;
	}
}

GameResult VkLowRenderer::CreateSwapChain() {
	SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(mPhysicalDevice);

	VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.mFormats);
	VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.mPresentModes);
	VkExtent2D extent = ChooseSwapExtent(swapChainSupport.mCapabilities);

	std::uint32_t imageCount = swapChainSupport.mCapabilities.minImageCount + 1;

	if (swapChainSupport.mCapabilities.maxImageCount > 0 &&
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
		ReturnGameResult(S_FALSE, L"Failed to create swap chain");

	vkGetSwapchainImagesKHR(mDevice, mSwapChain, &imageCount, nullptr);
	mSwapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(mDevice, mSwapChain, &imageCount, mSwapChainImages.data());

	mSwapChainImageFormat = surfaceFormat.format;
	mSwapChainExtent = extent;

	return GameResult(S_OK);
}

GameResult VkLowRenderer::CreateImageViews() {
	mSwapChainImageViews.resize(mSwapChainImages.size());

	for (size_t i = 0, end = mSwapChainImages.size(); i < end; ++i) {
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = mSwapChainImages[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = mSwapChainImageFormat;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(mDevice, &createInfo, nullptr, &mSwapChainImageViews[i]) != VK_SUCCESS)
			ReturnGameResult(S_FALSE, L"Failed to create image view");
	}

	return GameResult(S_OK);
}

GameResult VkLowRenderer::CreateShaderModule(const std::vector<char>& inCode, VkShaderModule& outModule) {
	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = inCode.size();
	createInfo.pCode = reinterpret_cast<const std::uint32_t*>(inCode.data());

	if (vkCreateShaderModule(mDevice, &createInfo, nullptr, &outModule) != VK_SUCCESS)
		ReturnGameResult(S_FALSE, L"Failed to create shader module");

	return GameResult(S_OK);
}

GameResult VkLowRenderer::CreateRenderPass() {
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = mSwapChainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	if (vkCreateRenderPass(mDevice, &renderPassInfo, nullptr, &mRenderPass) != VK_SUCCESS)
		ReturnGameResult(S_FALSE, L"Failed to create render pass");

	return GameResult(S_OK);
}

GameResult VkLowRenderer::CreateGraphicsPipeline() {
	VkShaderModule vertShaderModule;
	VkShaderModule fragShaderModule;

	{
		std::vector<char> vertShaderCode;
		CheckGameResult(ReadFile("./../../../../Assets/Shaders/vert.spv", vertShaderCode));
		std::vector<char> fragShaderCode;
		CheckGameResult(ReadFile("./../../../../Assets/Shaders/frag.spv", fragShaderCode));

		CheckGameResult(CreateShaderModule(vertShaderCode, vertShaderModule));
		CheckGameResult(CreateShaderModule(fragShaderCode, fragShaderModule));
	}

	VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = {
		vertShaderStageInfo, fragShaderStageInfo
	};

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(mSwapChainExtent.width);
	viewport.height = static_cast<float>(mSwapChainExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = mSwapChainExtent;

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer = {};	
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f;
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 0;
	pipelineLayoutInfo.pSetLayouts = nullptr;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(mDevice, &pipelineLayoutInfo, nullptr, &mPipelineLayout) != VK_SUCCESS)
		ReturnGameResult(S_FALSE, L"Failed to create pipeline layout");

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = nullptr;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = nullptr;
	pipelineInfo.layout = mPipelineLayout;
	pipelineInfo.renderPass = mRenderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mGraphicsPipeline) != VK_SUCCESS)
		ReturnGameResult(S_FALSE, L"Failed to create graphics pipeline");

	vkDestroyShaderModule(mDevice, fragShaderModule, nullptr);
	vkDestroyShaderModule(mDevice, vertShaderModule, nullptr);

	return GameResult(S_OK);
}

GameResult VkLowRenderer::CreateFramebuffers() {
	mSwapChainFramebuffers.resize(mSwapChainImageViews.size());

	for (size_t i = 0, end = mSwapChainImageViews.size(); i < end; ++i) {
		VkImageView attachments[] = {
			mSwapChainImageViews[i]
		};

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = mRenderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = mSwapChainExtent.width;
		framebufferInfo.height = mSwapChainExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(mDevice, &framebufferInfo, nullptr, &mSwapChainFramebuffers[i]) != VK_SUCCESS)
			ReturnGameResult(S_FALSE, L"Failed to create framebuffer");
	}

	return GameResult(S_OK);
}