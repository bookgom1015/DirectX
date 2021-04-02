#pragma once

class ThreadUtil {
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

	static void Log(const std::string& text);
	static void Logln(const std::string& text);
	static void Lognid(const std::string& text);
	static void Lognidln(const std::string& text);
	static void Log(std::initializer_list<std::string> texts);
	static void Logln(std::initializer_list<std::string> texts);
	static void Lognid(std::initializer_list<std::string> texts);
	static void Lognidln(std::initializer_list<std::string> texts);

	static void Log(const std::wstring& text);
	static void Logln(const std::wstring& text);
	static void Lognid(const std::wstring& text);
	static void Lognidln(const std::wstring& text);
	static void Log(std::initializer_list<std::wstring> texts);	
	static void Logln(std::initializer_list<std::wstring> texts);	
	static void Lognid(std::initializer_list<std::wstring> texts);
	static void Lognidln(std::initializer_list<std::wstring> texts);

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
};

class CVBarrier : public ThreadBarrier{
public:
	CVBarrier(UINT inCount);
	virtual ~CVBarrier() = default;

private:
	CVBarrier(const CVBarrier& ref) = delete;
	CVBarrier(CVBarrier&& ref) = delete;

	void operator=(const CVBarrier& rhs) = delete;
	void operator=(CVBarrier&& rhs) = delete;

public:
	void Wait() override final;
	void WakeUp();

private:
	std::mutex mMutex;
	std::condition_variable mConditionalVar;
	UINT mInitCount;
	UINT mCurrCount;
	std::uint64_t mGeneration;

	bool bWakeUp = false;
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