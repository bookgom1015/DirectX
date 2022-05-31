#include "DX12Game/ThreadUtil.h"

HANDLE ThreadUtil::mhLogFile = CreateFile(L"./tlog.txt", 
	GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

std::mutex ThreadUtil::mLogFileMutex;

UINT ThreadUtil::mPhysicalProcessorCount = 0;
UINT ThreadUtil::mLogicalProcessorCount = 0;
UINT ThreadUtil::mProcessorL1CacheCount = 0;
UINT ThreadUtil::mProcessorL2CacheCount = 0;
UINT ThreadUtil::mProcessorL3CacheCount = 0;

bool ThreadUtil::Initialize() {
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = nullptr;
	DWORD returnLength = 0;

	if (!GetProcessorInformation(buffer, returnLength))
		return false;

	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = buffer;
	DWORD byteOffset = 0;
	PCACHE_DESCRIPTOR cache = {};
	mPhysicalProcessorCount = 0;
	mLogicalProcessorCount = 0;
	mProcessorL1CacheCount = 0;
	mProcessorL2CacheCount = 0;
	mProcessorL3CacheCount = 0;

	while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= returnLength) {
		switch (ptr->Relationship) {
		case RelationCache:
			// Cache data is in ptr->Cache, one CACHE_DESCRIPTOR structure for each cache. 
			cache = &ptr->Cache;
			if (cache->Level == 1)
				++mProcessorL1CacheCount;
			else if (cache->Level == 2)
				++mProcessorL2CacheCount;
			else if (cache->Level == 3)
				++mProcessorL3CacheCount;
			break;
		case RelationProcessorCore:
			++mPhysicalProcessorCount;

			// A hyperthreaded core supplies more than one logical processor.
			mLogicalProcessorCount += CountSetBits(ptr->ProcessorMask);
		}

		byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
		++ptr;
	}

	std::free(buffer);

	return true;
}

UINT ThreadUtil::GetProcessorCount(bool inLogic) {
	return inLogic ? mLogicalProcessorCount : mPhysicalProcessorCount;
};

void ThreadUtil::GetProcessorCaches(UINT& outL1Cache, UINT& outL2Cache, UINT& outL3Cache) {
	outL1Cache = mProcessorL1CacheCount;
	outL1Cache = mProcessorL1CacheCount;
	outL1Cache = mProcessorL1CacheCount;
}

void ThreadUtil::TLogFunc(const std::string& text) {
	std::wstring wstr;
	wstr.assign(text.begin(), text.end());

	DWORD writtenBytes = 0;

	mLogFileMutex.lock();

	WriteFile(
		mhLogFile,
		wstr.c_str(),
		static_cast<DWORD>(wstr.length() * sizeof(wchar_t)),
		&writtenBytes, NULL
	);

	mLogFileMutex.unlock();
}

void ThreadUtil::TLogFunc(const std::wstring& text) {
	DWORD writtenBytes = 0;

	mLogFileMutex.lock();

	WriteFile(
		mhLogFile, 
		text.c_str(),
		static_cast<DWORD>(text.length() * sizeof(wchar_t)),
		&writtenBytes, 
		NULL
	);

	mLogFileMutex.unlock();
}

DWORD ThreadUtil::CountSetBits(ULONG_PTR bitMask) {
	DWORD LSHIFT = sizeof(ULONG_PTR) * 8 - 1;
	DWORD bitSetCount = 0;
	ULONG_PTR bitTest = (ULONG_PTR)1 << LSHIFT;
	DWORD i;

	for (i = 0; i <= LSHIFT; ++i) {
		bitSetCount += ((bitMask & bitTest) ? 1 : 0);
		bitTest /= 2;
	}

	return bitSetCount;
}

bool ThreadUtil::GetProcessorInformation(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION& outBuffer, DWORD& outReturnLength) {
	LPFN_GLPI glpi = (LPFN_GLPI)GetProcAddress(
		GetModuleHandle(TEXT("kernel32")),
		"GetLogicalProcessorInformation"
	);

	if (glpi == NULL) {
		WErrln(L"GetLogicalProcessorInformation is not supported");
		return false;
	}

	BOOL bDone = false;

	do {
		DWORD rc = glpi(outBuffer, &outReturnLength);

		if (FALSE == rc) {
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
				if (outBuffer != nullptr)
					std::free(outBuffer);

				outBuffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)std::malloc(outReturnLength);

				if (outBuffer == nullptr) {
					WErrln(L"Allocation failure");
					return false;
				}
			}
			else {
				std::wstringstream wsstream;
				wsstream << GetLastError();
				WErrln(wsstream.str());
				return false;
			}
		}
		else {
			bDone = TRUE;
		}
	} while (!bDone);

	return true;
}

TaskTimer::TaskTimer() {
	__int64 countsPerSec;
	mSecondsPerCount = QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&countsPerSec));
	mSecondsPerCount = 1.0 / static_cast<double>(countsPerSec);
}

void TaskTimer::SetBeginTime() {
	__int64 currTime;
	QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currTime));

	mBeginTime = currTime;
}

void TaskTimer::SetEndTime() {
	__int64 currTime;
	QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currTime));

	mEndTime = currTime;
}

float TaskTimer::GetElapsedTime() const {
	__int64 diff = mEndTime - mBeginTime;
	if (diff < 0)
		return 0;

	return static_cast<float>(diff * mSecondsPerCount);
}

CVBarrier::CVBarrier(UINT inCount)
	: mInitCount(inCount), mCurrCount(inCount), mGeneration(0) {}

CVBarrier::~CVBarrier() {
	WakeUp();
}

bool CVBarrier::Wait() {
	std::unique_lock<std::mutex> ulock(mMutex);
	std::uint64_t currGen = mGeneration;

	if (--mCurrCount == 0) {
		++mGeneration;
		mCurrCount = mInitCount;
		mConditionalVar.notify_all();
		return bTerminated;
	}

	while (!bTerminated && currGen == mGeneration)
		mConditionalVar.wait(ulock);

	return bTerminated;
}

void CVBarrier::WakeUp() {
	++mGeneration;
	mConditionalVar.notify_all();
}

void CVBarrier::Terminate() {
	bTerminated = true;
	mConditionalVar.notify_all();
}

SpinlockBarrier::SpinlockBarrier(UINT inCount) 
	: mInitCount(inCount), mCurrCount(inCount), mGeneration(0) {}

bool SpinlockBarrier::Wait() {	
	std::unique_lock<std::mutex> ulock(mMutex);
	std::uint64_t currGen = mGeneration;

	if (--mCurrCount == 0) {
		++mGeneration;
		mCurrCount = mInitCount;
		return bTerminated;
	}

	ulock.unlock();

	while (!bTerminated && currGen == mGeneration)
		std::this_thread::yield();

	return bTerminated;
}

void SpinlockBarrier::WakeUp() {
	++mGeneration;
}

void SpinlockBarrier::Terminate() {
	bTerminated = true;	
}

ThreadPool::ThreadPool(size_t inNumThreads) {
	mThreads.resize(inNumThreads);
	for (size_t i = 0; i < inNumThreads; ++i) {
		mThreads[i] = std::thread([this](UINT tid) -> void {
			this->DoWork(tid);
		}, static_cast<UINT>(i));
	}
}

ThreadPool::~ThreadPool() {
	bStopAll = true;
	
	for (auto& cv : mConditionVars)
		cv.second.notify_one();

	for (auto& thread : mThreads)
		thread.join();
}

void ThreadPool::Enqueue(UINT inThreadId, std::function<void(UINT)> inJob) {
	if (bStopAll)
		return;

	{
		std::lock_guard<std::mutex> lock(mMutex);
		mJobs[inThreadId].push(std::move(inJob));
	}

	mConditionVars[inThreadId].notify_all();
}

void ThreadPool::DoWork(UINT tid) {
	while (true) {
		std::unique_lock<std::mutex> ulock(mMutex);
		mConditionVars[tid].wait(ulock, [this, tid]() -> bool {
			return !mJobs[tid].empty() || bStopAll;
		});
		if (bStopAll && mJobs[tid].empty())
			return;

		std::function<void(UINT)> job = std::move(mJobs[tid].front());
		mJobs[tid].pop();

		ulock.unlock();

		job(tid);
	}
}