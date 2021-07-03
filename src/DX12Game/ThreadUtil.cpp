#include "DX12Game/ThreadUtil.h"

HANDLE ThreadUtil::mhLogFile = CreateFile(L"./tlog.txt", 
	GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

std::mutex ThreadUtil::mLogFileMutex;

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

void CVBarrier::Wait() {
	std::unique_lock<std::mutex> ulock(mMutex);
	std::uint64_t currGen = mGeneration;

	if (--mCurrCount == 0) {
		++mGeneration;
		mCurrCount = mInitCount;
		mConditionalVar.notify_all();
		return;
	}

	while (!bWakeUp && currGen == mGeneration)
		mConditionalVar.wait(ulock);
}

void CVBarrier::WakeUp() {
	++mGeneration;
	bWakeUp = true;
	mConditionalVar.notify_all();
}

SpinlockBarrier::SpinlockBarrier(UINT inCount) 
	: mInitCount(inCount), mCurrCount(inCount), mGeneration(0) {}

void SpinlockBarrier::Wait() {
	UINT currGen = mGeneration.load();

	if (--mCurrCount == 0) {
		if (mGeneration.compare_exchange_weak(currGen, currGen + 1))
			mCurrCount = mInitCount;

		return;
	}

	while (!bWakeUp && (currGen == mGeneration) && (mCurrCount != 0))
		std::this_thread::yield();
}

void SpinlockBarrier::WakeUp() {
	++mGeneration;
	bWakeUp = true;
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