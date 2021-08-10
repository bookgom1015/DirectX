#pragma once

#define MT_World
//#define UsingVulkan

#define DeferredRendering

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

#include <algorithm>
#include <array>
#include <cassert>
#include <cfloat>
#include <comdef.h>
#include <cstdint>
#include <DirectXCollision.h>
#include <DirectXColors.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <dxgi1_4.h>
#include <D3Dcompiler.h>
#include <d3d12.h>
#include <initializer_list>
#include <iomanip>
#include <fstream>
#include <functional>
#include <future>
#include <memory>
#include <SimpleMath.h>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <windows.h>
#include <windowsx.h>
#include <wrl.h>

#include "common/d3dx12.h"
#include "common/DDSTextureLoader.h"
#include "common/MathHelper.h"

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

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
	#define ReturnGameResult(__status, __message)										\
	{																					\
		std::wstringstream __wsstream_RGR;												\
		__wsstream_RGR << L"[HRESULT: 0x" << std::hex << __status << L"] " << std::dec	\
			<< __FILE__ << L"; line: " << __LINE__ << L"; " << __message << std::endl;	\
		return GameResult(__status, __wsstream_RGR.str());								\
	}
#endif

#ifndef CheckGameResult
#define CheckGameResult(__statement)			\
	{											\
		GameResult __result = (__statement);	\
		if (FAILED(__result.hr))				\
			return __result;					\
	}
#endif

#ifndef ReturnIfFailed
	#define ReturnIfFailed(__statement)															\
	{																							\
		HRESULT __hr = (__statement);															\
		if (FAILED(__hr)) {																		\
			std::wstringstream __wsstream_RIF;													\
			_com_error __err(__hr);																\
			__wsstream_RIF << L#__statement << L" failed; message: " << __err.ErrorMessage();	\
			ReturnGameResult(__hr, __wsstream_RIF.str());										\
		}																						\
	}
#endif

#include "DX12Game/D3D12Util.h"