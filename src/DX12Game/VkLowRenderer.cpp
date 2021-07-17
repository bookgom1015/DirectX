#include "DX12Game/VkLowRenderer.h"

#include <map>
#include <set>

namespace {
	std::vector<const char*> ValidationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

#ifdef _DEBUG
	const bool EnableValidationLayers = true;
#else
	const bool EnableValidationLayers = false;
#endif

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

	for (const char* layerName : ValidationLayers) {
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers) {
			if (std::strcmp(layerName, layerProperties.layerName) == 0) {
				layerFound = true;
				break;
			}
		}

		if (!layerFound) {
			std::wstringstream wsstream;
			wsstream << layerName << L" is not supported" << std::endl;
			ReturnGameResult(S_FALSE, wsstream.str());
		}
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
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
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

	return indices.IsComplete();
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
	createInfo.enabledExtensionCount = 0;

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