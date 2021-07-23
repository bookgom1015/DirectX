#pragma once

#define MT_World
//#define UsingVulkan

// Link necessary d3d12 libraries.
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "DirectXTK12.lib")
// Link necessary vulkan libraries.
#pragma comment(lib, "glfw3.lib")
#pragma comment(lib, "vulkan-1.lib")

const int gNumFrameResources = 3;

#define NOMINMAX

#include <comdef.h>
#include <float.h>
#include <functional>
#include <future>
#include <initializer_list>
#include <iomanip>
#include <SimpleMath.h>
#include <thread>
#include <utility>
#include <windowsx.h>

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include "common/MathHelper.h"
#include "common/d3dUtil.h"
#include "DX12Game/ContainerUtil.h"
#include "DX12Game/GameTimer.h"
#include "DX12Game/StringUtil.h"
#include "DX12Game/ThreadUtil.h"

struct GameResult {
public:
	HRESULT hr;
	std::wstring msg;

public:
	GameResult(HRESULT _hr, std::wstring _msg = L"") {
		hr = _hr;
		msg = _msg;
	}

	GameResult(const GameResult& src) {
		hr = src.hr;
		msg = src.msg;
	}

	GameResult(GameResult&& src) {
		if (this == &src)
			return;

		hr = src.hr;
		msg = std::move(src.msg);
	}
};

#ifndef ReturnGameResult
	#define ReturnGameResult(__status, __message)													\
	{																								\
		std::wstringstream __wsstream_RGR;															\
		__wsstream_RGR << __FILE__ << L"; line: " << __LINE__ << L"; " << __message << std::endl;	\
		return GameResult(__status, __wsstream_RGR.str());											\
	}
#endif

#ifndef CheckGameResult
#define CheckGameResult(__result)		\
	{									\
		if (__result.hr != S_OK)		\
			return __result;			\
	}
#endif

#ifndef ReturnIfFailed
	#define ReturnIfFailed(__statement)																\
	{																								\
		HRESULT __hr = (__statement);																\
		if (FAILED(__hr)) {																			\
			std::wstringstream __wsstream_RIF;														\
			_com_error err(__hr);																	\
			__wsstream_RIF << L#__statement << L" failed;" << L"; message: " << err.ErrorMessage();	\
			ReturnGameResult(__hr, __wsstream_RIF.str());											\
		}																							\
	}
#endif