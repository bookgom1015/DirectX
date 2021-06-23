#pragma once

#ifndef Log
	#define Log(x, ...)																	\
	{																					\
		std::vector<std::string> texts = { x, __VA_ARGS__ };							\
		std::stringstream _sstream;														\
																						\
		for (const auto& text : texts)													\
			_sstream << text << ' ';													\
																						\
		StringUtil::LogFunc(_sstream.str());											\
	}
#endif

#ifndef Logln
	#define Logln(x, ...)																\
	{																					\
		std::vector<std::string> texts = { x, __VA_ARGS__ };							\
		std::stringstream _sstream;														\
																						\
		for (const auto& text : texts)													\
			_sstream << text << ' ';													\
		_sstream << '\n';																\
																						\
		StringUtil::LogFunc(_sstream.str());											\
	}
#endif

#ifndef WLog
	#define WLog(x, ...)																\
	{																					\
		std::vector<std::wstring> texts = { x, __VA_ARGS__ };							\
		std::wstringstream _wsstream;													\
																						\
		for (const auto& text : texts)													\
			_wsstream << text << L' ';													\
																						\
		StringUtil::LogFunc(_wsstream.str());											\
	}
#endif

#ifndef WLogln
	#define WLogln(x, ...)																\
	{																					\
		std::vector<std::wstring> texts = { x, __VA_ARGS__ };							\
		std::wstringstream _wsstream;													\
																						\
		for (const auto& text : texts)													\
			_wsstream << text << L' ';													\
		_wsstream << L'\n';																\
																						\
		StringUtil::LogFunc(_wsstream.str());											\
	}
#endif

#ifndef Err
	#define Err(x, ...)																		\
	{																						\
		std::vector<std::string> texts = { x, __VA_ARGS__ };								\
		std::stringstream _sstream;															\
																							\
		_sstream << "[Error] " << __FILE__ << "; line: " << __LINE__ << "; ";				\
		for (const auto& text : texts)														\
			_sstream << text << ' ';														\
																							\
		StringUtil::LogFunc(_sstream.str());												\
	}
#endif

#ifndef WErr
	#define WErr(x, ...)																	\
	{																						\
		std::vector<std::wstring> texts = { x, __VA_ARGS__ };								\
		std::wstringstream _wsstream;														\
																							\
		_wsstream << L"[Error] " << __FILE__ << L"; line: " << __LINE__ << L"; ";			\
		for (const auto& text : texts)														\
			_wsstream << text << L' ';														\
																							\
		StringUtil::LogFunc(_wsstream.str());												\
	}
#endif

#ifndef Errln
	#define Errln(x, ...)																	\
	{																						\
		std::vector<std::string> texts = { x, __VA_ARGS__ };								\
		std::stringstream _sstream;															\
																							\
		_sstream << "[Error] " << __FILE__ << "; line: " << __LINE__ << "; ";				\
		for (const auto& text : texts)														\
			_sstream << text << ' ';														\
																							\
		StringUtil::LogFunc(_sstream.str());												\
	}
#endif

#ifndef WErrln
	#define WErrln(x, ...)																	\
	{																						\
		std::vector<std::wstring> texts = { x, __VA_ARGS__ };								\
		std::wstringstream _wsstream;														\
																							\
		_wsstream << L"[Error] " << __FILE__ << L"; line: " << __LINE__ << L"; ";			\
		for (const auto& text : texts)														\
			_wsstream << text << L' ';														\
																							\
		StringUtil::LogFunc(_wsstream.str());												\
	}
#endif

namespace StringUtil {
	class StringUtilHelper {
	public:
		static HANDLE ghLogFile;

		static std::mutex gLogFileMutex;
	};

	inline void LogFunc(const std::string& text);
	inline void LogFunc(const std::wstring& text);

	inline void SetTextToWnd(HWND hWnd, LPCWSTR newText);
	inline void AppendTextToWnd(HWND hWnd, LPCWSTR newText);
};

#include "StringUtil.inl"