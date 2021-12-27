#pragma once

#include <comdef.h>
#include <sstream>
#include <string>
#include <windows.h>

struct GameResult {
public:
	HRESULT hr;
	std::wstring msg;

public:
	GameResult(HRESULT _hr, std::wstring _msg = L"") {
		hr = _hr;
		msg = _msg;
	}

	GameResult(const GameResult& inRef) {
		hr = inRef.hr;
		msg = inRef.msg;
	}

	GameResult(GameResult&& inRVal) {
		if (this == &inRVal)
			return;

		hr = inRVal.hr;
		msg = std::move(inRVal.msg);
	}

public:
	GameResult& operator=(const GameResult& inRef) {
		hr = inRef.hr;
		msg = inRef.msg;

		return *this;
	}
};

#ifndef ReturnGameResult
	#define ReturnGameResult(__status, __message)											\
		{																					\
			std::wstringstream __wsstream_RGR;												\
			__wsstream_RGR << L"[HRESULT: 0x" << std::hex << __status << L"] " << std::dec	\
				<< __FILE__ << L"; line: " << __LINE__ << L"; " << __message << std::endl;	\
			return GameResult(__status, __wsstream_RGR.str());								\
		}
#endif

#ifndef CheckGameResult
	#define CheckGameResult(__statement)													\
		{																					\
			GameResult __result = (__statement);											\
			if (FAILED(__result.hr)) {														\
				std::wstringstream __wsstream_CGR;											\
				__wsstream_CGR << __FILE__ << L"; line: " << __LINE__ << L';' << std::endl;	\
				__result.msg.append(__wsstream_CGR.str());									\
				return __result;															\
			}																				\
		}
#endif

#ifndef ReturnIfFailed
	#define ReturnIfFailed(__statement)																\
		{																							\
			HRESULT __return_hr = (__statement);													\
			if (FAILED(__return_hr)) {																\
				std::wstringstream __wsstream_RIF;													\
				_com_error __err(__return_hr);														\
				__wsstream_RIF << L#__statement << L" failed; message: " << __err.ErrorMessage();	\
				ReturnGameResult(__return_hr, __wsstream_RIF.str());								\
			}																						\
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
	#ifdef MT_World
		#define SyncHost(__pBarrier)										\
				{																	\
					if (__pBarrier != nullptr)										\
						__pBarrier->Wait();											\
					else															\
						ReturnGameResult(S_FALSE, L#__pBarrier L" is nullptr");		\
				}
		#else
		// Do nothing.
	#endif
#endif

static GameResult __STATIC_GAMERESULT_OK(S_OK);
static GameResult __STATIC_GAMERESULT_FALSE(S_FALSE);

#ifndef GameResultOk
	#define GameResultOk __STATIC_GAMERESULT_OK
#endif

#ifndef GameResultFalse
	#define GameResultFalse __STATIC_GAMERESULT_FALSE
#endif