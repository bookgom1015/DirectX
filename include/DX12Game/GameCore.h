#pragma once

#define MT_World

// Link necessary d3d12 libraries
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "DirectXTK12.lib")

const int gNumFrameResources = 3;

#define NOMINMAX

#include <atomic>
#include <cmath>
#include <comdef.h>
#include <cstdint>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <float.h>
#include <functional>
#include <future>
#include <initializer_list>
#include <iomanip>
#include <mutex>
#include <string>
#include <sstream>
#include <thread>
#include <Windows.h>
#include <windowsx.h>

#include "common/MathHelper.h"
#include "common/d3dUtil.h"
#include "DX12Game/ContainerUtil.h"
#include "DX12Game/GameTimer.h"
#include "DX12Game/StringUtil.h"
#include "DX12Game/ThreadUtil.h"

struct DxResult {
public:
	HRESULT hr;
	std::wstring msg;

public:
	DxResult(HRESULT _hr, std::wstring _msg = L"") {
		hr = _hr;
		msg = _msg;
	}

	DxResult(const DxResult& src) {
		hr = src.hr;
		msg = src.msg;
	}

	DxResult(DxResult&& src) {
		if (this == &src)
			return;

		hr = src.hr;
		msg = std::move(src.msg);
	}
};

#ifndef ReturnDxResult
	#define ReturnDxResult(__status, __message)															\
	{																									\
		std::wstringstream __wsstream;																	\
		__wsstream << __FILE__ << L"; line: " << __LINE__ << L"; " << __message << std::endl;			\
		return DxResult(__status, __wsstream.str());													\
	}
#endif

#ifndef ReturnIfFailed
	#define ReturnIfFailed(__statement)																\
	{																								\
		HRESULT __hr = (__statement);																\
		if (FAILED(__hr)) {																			\
			std::wstringstream __wsstream;															\
			_com_error err(__hr);																	\
			__wsstream << L#__statement << L" failed;" << L"; message: " << err.ErrorMessage();		\
			ReturnDxResult(__hr, __wsstream.str());													\
		}																							\
	}
#endif

#ifndef CheckDxResult
	#define CheckDxResult(__dxresult)	\
	{									\
		if (__dxresult.hr != S_OK)		\
			return __dxresult;			\
	}
#endif