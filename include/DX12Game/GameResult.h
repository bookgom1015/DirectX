#pragma once

#include "DX12Game/StringUtil.h"

#include <comdef.h>
#include <sstream>
#include <string>
#include <windows.h>

struct GameResult {
public:
	HRESULT hr;

public:
	GameResult(HRESULT _hr) {
		hr = _hr;
	}

	GameResult(const GameResult& inRef) {
		hr = inRef.hr;
	}

	GameResult(GameResult&& inRVal) {
		if (this == &inRVal)
			return;

		hr = inRVal.hr;
	}

public:
	GameResult& operator=(const GameResult& inRef) {
		hr = inRef.hr;

		return *this;
	}
};

static GameResult __STATIC_GAMERESULT_OK(S_OK);
static GameResult __STATIC_GAMERESULT_FALSE(S_FALSE);

#ifndef GameResultOk
	#define GameResultOk __STATIC_GAMERESULT_OK
#endif

#ifndef GameResultFalse
	#define GameResultFalse __STATIC_GAMERESULT_FALSE
#endif

#ifndef ReturnGameResult
	#define ReturnGameResult(__status, __message)										\
		{																				\
			std::wstringstream __wsstream_RGR;											\
			__wsstream_RGR << L"[HRESULT: 0x" << std::hex << __status << L"] " <<		\
				std::dec << __FILE__ << L"; line: " << __LINE__ << L"; " << __message;	\
			WLogln(__wsstream_RGR.str());												\
			return GameResult(__status);												\
		}
#endif

#ifndef CheckGameResult
	#define CheckGameResult(__statement)										\
		{																		\
			GameResult __result = (__statement);								\
			if (FAILED(__result.hr)) {											\
				std::wstringstream __wsstream_CGR;								\
				__wsstream_CGR << __FILE__ << L"; line: " << __LINE__ << L';';	\
				WLogln(__wsstream_CGR.str());									\
				return __result;												\
			}																	\
		}
#endif

#ifndef ReturnIfFailed
	#define ReturnIfFailed(__statement)													\
		{																				\
			HRESULT __return_hr = (__statement);										\
			if (FAILED(__return_hr)) {													\
				std::wstringstream __wsstream_RIF;										\
				_com_error __err(__return_hr);											\
				__wsstream_RIF <<  __FILE__ << L"; line: " << __LINE__ << L"; " <<		\
					L#__statement << L" failed; message: " << __err.ErrorMessage();		\
				WLogln(__wsstream_RIF.str());											\
				return GameResult(__return_hr);											\
			}																			\
		}
#endif

#ifndef BreakIfFailed
	#define BreakIfFailed(__statement)				\
		{											\
			GameResult __result = (__statement);	\
			if (FAILED(__result.hr)) {				\
				break;								\
			}										\
		}
#endif

#ifndef SyncHost
	#define SyncHost(__pBarrier)										\
	{																	\
		if (__pBarrier != nullptr) {									\
			bool terminated = __pBarrier->Wait();						\
			if (terminated) {											\
				return GameResultFalse;									\
			}															\
		}																\
		else {															\
			ReturnGameResult(S_FALSE, L#__pBarrier L" is nullptr");		\
		}																\
	}
#endif