#pragma once

#define MT_World

// Link necessary d3d12 libraries
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "DirectXTK12.lib")

const int gNumFrameResources = 3;

#include <atomic>
#include <cmath>
#include <comdef.h>
#include <cstdint>
#include <ctgmath>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <float.h>
#include <functional>
#include <future>
#include <initializer_list>
#include <iomanip>
#include <mutex>
#include <queue>
#include <deque>
#include <string>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>
#include <Windows.h>
#include <windowsx.h>

#include "common/MathHelper.h"
#include "common/d3dUtil.h"
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

#ifndef ReturnIfFailed
#define ReturnIfFailed(x)																			\
	{																								\
		HRESULT __hr = (x);																			\
		if (FAILED(__hr)) {																			\
			std::wstringstream __wsstream;															\
			_com_error err(__hr);																	\
			__wsstream << L#x << L" failed in" << __FILE__ <<										\
				L"; line: " << __LINE__ << L"; message: " << err.ErrorMessage();					\
			StringUtil::Logln(__wsstream.str());													\
			return DxResult(__hr, __wsstream.str());												\
		}																							\
	}
#endif

#ifndef CheckDxResult
#define CheckDxResult(x)			\
	{								\
		if (x.hr != S_OK)			\
			return x;				\
	}
#endif