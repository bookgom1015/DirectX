#pragma once

#include "DX12Game/StringUtil.h"

#include <atomic>
#include <functional>
#include <queue>
#include <string>

#ifndef TLog
	#define TLog(x, ...)												\
	{																	\
		std::vector<std::string> texts = { x, __VA_ARGS__ };			\
		std::stringstream _sstream;										\
																		\
		_sstream << "[tid: " << std::this_thread::get_id() << "]";		\
																		\
		for (const auto& text : texts)									\
			_sstream << text << ' ';									\
																		\
		ThreadUtil::TLogFunc(_sstream.str());							\
	}
#endif

#ifndef TLogln
	#define TLogln(x, ...)												\
	{																	\
		std::vector<std::string> texts = { x, __VA_ARGS__ };			\
		std::stringstream _sstream;										\
																		\
		_sstream << "[tid: " << std::this_thread::get_id() << "]";		\
																		\
		for (const auto& text : texts)									\
			_sstream << text << ' ';									\
		_sstream << '\n';												\
																		\
		ThreadUtil::TLogFunc(_sstream.str());							\
	}
#endif

#ifndef TWLog
	#define TWLog(x, ...)												\
	{																	\
		std::vector<std::wstring> texts = { x, __VA_ARGS__ };			\
		std::wstringstream _wsstream;									\
																		\
		_wsstream << L"[tid: " << std::this_thread::get_id() << L"]";	\
																		\
		for (const auto& text : texts)									\
			_wsstream << text << L' ';									\
																		\
		ThreadUtil::TLogFunc(_wsstream.str());							\
	}
#endif

#ifndef TWLogln
	#define TWLogln(x, ...)												\
	{																	\
		std::vector<std::wstring> texts = { x, __VA_ARGS__ };			\
		std::wstringstream _wsstream;									\
																		\
		_wsstream << L"[tid: " << std::this_thread::get_id() << L"]";	\
																		\
		for (const auto& text : texts)									\
			_wsstream << text << L' ';									\
		_wsstream << L'\n';												\
																		\
		ThreadUtil::TLogFunc(_wsstream.str());							\
	}
#endif

class ThreadUtil {
private:
	typedef BOOL(WINAPI *LPFN_GLPI)(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, PDWORD);

public:
	// PROCESSOR_ARCHITECTURE_AMD64		- x64(AMD or Intel)
	// PROCESSOR_ARCHITECTURE_ARM		- ARM
	// PROCESSOR_ARCHITECTURE_ARM64		- ARM64
	// PROCESSOR_ARCHITECTURE_IA64		- Intel Itanium - based
	// PROCESSOR_ARCHITECTURE_INTEL		- x86
	// PROCESSOR_ARCHITECTURE_UNKNOWN	-Unknown architecture.
	static inline WORD GetProcessArchitecture() {
		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);

		return sysInfo.wProcessorArchitecture;
	}

	static inline DWORD GetPageSize() {
		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);

		return sysInfo.dwPageSize;
	}

	static inline DWORD GetNumberOfProcessors() {
		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);

		return sysInfo.dwNumberOfProcessors;
	}

	static bool GetProcessorCount(UINT& outCount, bool inLogic = false);

	static void TLogFunc(const std::string& text);
	static void TLogFunc(const std::wstring& text);

private:
	// Helper function to count set bits in the processor mask.
	static DWORD CountSetBits(ULONG_PTR bitMask);

	static bool GetProcessorInformation(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION& outBuffer, DWORD& outReturnLength);

private:
	static HANDLE mhLogFile;

	static std::mutex mLogFileMutex;
};

class TaskTimer {
public:
	TaskTimer();
	virtual ~TaskTimer() = default;

	void SetBeginTime();
	void SetEndTime();

	float GetElapsedTime() const;

private:
	double mSecondsPerCount;

	__int64 mBeginTime = 0;
	__int64 mEndTime = 0;
};

class ThreadBarrier {
public:
	virtual void Wait() = 0;
	virtual void WakeUp() = 0;

protected:
	bool bWakeUp = false;
};

class CVBarrier : public ThreadBarrier{
public:
	CVBarrier(UINT inCount);
	virtual ~CVBarrier();

private:
	CVBarrier(const CVBarrier& ref) = delete;
	CVBarrier(CVBarrier&& ref) = delete;

	void operator=(const CVBarrier& rhs) = delete;
	void operator=(CVBarrier&& rhs) = delete;

public:
	void Wait() override final;
	void WakeUp() override final;

private:
	std::mutex mMutex;
	std::condition_variable mConditionalVar;
	UINT mInitCount;
	UINT mCurrCount;
	std::uint64_t mGeneration;
};

class SpinlockBarrier : public ThreadBarrier {
public:
	SpinlockBarrier(UINT inCount);
	virtual ~SpinlockBarrier() = default;

private:
	SpinlockBarrier(const SpinlockBarrier& ref) = delete;
	SpinlockBarrier(SpinlockBarrier&& ref) = delete;

	void operator=(const SpinlockBarrier& rhs) = delete;
	void operator=(SpinlockBarrier&& rhs) = delete;

public:
	void Wait() override final;
	void WakeUp() override final;

private:
	UINT mInitCount;
	std::atomic<UINT> mCurrCount;
	std::atomic<UINT> mGeneration;
};

class ThreadPool {
public:
	ThreadPool(size_t inNumThreads);
	virtual ~ThreadPool();

	void Enqueue(UINT inThreadId, std::function<void(UINT)> inJob);

private:
	void DoWork(UINT tid);

private:
	std::vector<std::thread> mThreads;
	std::unordered_map<UINT, std::queue<std::function<void(UINT)>>> mJobs;

	std::mutex mMutex;
	std::unordered_map<UINT, std::condition_variable> mConditionVars;

	bool bStopAll = false;
};