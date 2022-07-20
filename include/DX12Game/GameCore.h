#pragma once

//#define UsingVulkan

// Link necessary d3d12 libraries.
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
//#pragma comment(lib, "DirectXTK12.lib")
#ifdef UsingVulkan
	// Link necessary vulkan libraries.
	#pragma comment(lib, "glfw3.lib")
	#pragma comment(lib, "vulkan-1.lib")
#endif

const int gNumFrameResources = 3;

#include <algorithm>
#include <array>
#include <cassert>
#include <cfloat>
#include <cstdint>
#include <fstream>
#include <functional>
#include <future>
#include <initializer_list>
#include <iomanip>
#include <memory>
#include <optional>
#include <thread>
#include <utility>
#include <vector>
#define NOMINMAX
#include <Windows.h>
#include <windowsx.h>

#include "DX12Game/D3D12Util.h"

#include "DX12Game/GameTimer.h"
#include "DX12Game/ThreadUtil.h"

#ifdef UsingVulkan
	#define VK_USE_PLATFORM_WIN32_KHR
	#define GLFW_INCLUDE_VULKAN
	#include <GLFW/glfw3.h>
	
	#define GLFW_EXPOSE_NATIVE_WIN32
	#include <GLFW/glfw3native.h>
#endif 

const std::wstring TextureFilePathW = L"./../../../../Assets/Textures/";
const std::string TextureFilePath = "./../../../../Assets/Textures/";
const std::wstring ShaderFilePathW = L".\\..\\..\\..\\..\\Assets\\Shaders\\";
const std::string ShaderFilePath = ".\\..\\..\\..\\..\\Assets\\Shaders\\";
const std::wstring FontFilePathW = L"./../../../../Assets/Fonts/";
const std::string FontFilePath = "./../../../../Assets/Fonts/";
const std::wstring ModelFilePathW = L"./../../../../Assets/Models/";
const std::string ModelFilePath = "./../../../../Assets/Models/";