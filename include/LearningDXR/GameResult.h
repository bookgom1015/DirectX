#pragma once

#include "DX12Game/StringUtil.h"

#include <comdef.h>
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
static GameResult __STATIC_GAMERESULT_FAIL(E_FAIL);

#ifndef GameResultOk
	#define GameResultOk __STATIC_GAMERESULT_OK
#endif

#ifndef GameResultFail
	#define GameResultFail __STATIC_GAMERESULT_FAIL
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
	#define CheckGameResult(__statement)											\
		{																			\
			try {																	\
				GameResult __result = (__statement);								\
				if (FAILED(__result.hr)) {											\
					std::wstringstream __wsstream_CGR;								\
					__wsstream_CGR << __FILE__ << L"; line: " << __LINE__ << L';';	\
					WLogln(__wsstream_CGR.str());									\
					return __result;												\
				}																	\
			}																		\
			catch (const std::exception& e) {										\
				ReturnGameResult(E_UNEXPECTED, e.what());							\
			}																		\
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

#ifndef ReturnIfFailedDxDebug
	#define ReturnIfFailedDxDebug(__info_q, __statement)									\
		{																					\
			if (__info_q == nullptr) return GameResult(E_POINTER);							\
			HRESULT __return_hr = (__statement);											\
			if (FAILED(__return_hr)) {														\
				SIZE_T __messageLength = 0;													\
				ReturnIfFailed(__info_q->GetMessageW(0, NULL, &__messageLength));			\
				D3D12_MESSAGE* __message = (D3D12_MESSAGE*)std::malloc(__messageLength);	\
				ReturnIfFailed(__info_q->GetMessage(0, __message, &__messageLength));		\
				std::wstringstream __wsstream_RIFDXDBG;										\
				__wsstream_RIFDXDBG <<  __FILE__ << L"; line: " << __LINE__ << L"; " <<		\
					L#__statement << L" failed; message: " << __message->pDescription;		\
				WLogln(__wsstream_RIFDXDBG.str());											\
				return GameResult(__return_hr);												\
			}																				\
		}
#endif

#ifndef ReturnIfFalse
	#define ReturnIfFalse(__statement, __msg)											\
		{																				\
			bool __result = (__statement);												\
			if (!__result) {															\
				std::wstringstream __wsstream_RIFM;										\
				__wsstream_RIFM <<  __FILE__ << L"; line: " << __LINE__ << L"; " <<		\
					L#__statement << L" is invalid; message: " << __msg;				\
				WLogln(__wsstream_RIFM.str());											\
				return GameResult(E_FAIL);												\
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
				return GameResultFail;									\
			}															\
		}																\
		else {															\
			ReturnGameResult(E_POINTER, L#__pBarrier L" is nullptr");	\
		}																\
	}
#endif