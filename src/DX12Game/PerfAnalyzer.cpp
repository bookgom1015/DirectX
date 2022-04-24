#include "DX12Game/PerfAnalyzer.h"
#include "DX12Game/Renderer.h"

void PerfAnalyzer::Initialize(class Renderer* inRenderer, UINT inNumThreads) {
	mRenderer = inRenderer;

	mNumThreads = inNumThreads;

	mTimeStep.SetBeginTime();

	mSwtichers.resize(inNumThreads);
	mUpdateTimers1.resize(inNumThreads);
	mUpdateTimers2.resize(inNumThreads);
	mDrawTimers1.resize(inNumThreads);
	mDrawTimers2.resize(inNumThreads);
	mWholeLoopTimers1.resize(inNumThreads);
	mWholeLoopTimers2.resize(inNumThreads);
}

void PerfAnalyzer::UpdateBeginTime(UINT tid) {
	if (mSwtichers[tid] == 0) mUpdateTimers1[tid].SetBeginTime();
	else mUpdateTimers2[tid].SetBeginTime();
}

void PerfAnalyzer::UpdateEndTime(UINT tid) {
	if (mSwtichers[tid] == 0) mUpdateTimers1[tid].SetEndTime();
	else mUpdateTimers2[tid].SetEndTime();
}

void PerfAnalyzer::DrawBeginTime(UINT tid) {
	if (mSwtichers[tid] == 0) mDrawTimers1[tid].SetBeginTime();
	else mDrawTimers2[tid].SetBeginTime();
}

void PerfAnalyzer::DrawEndTime(UINT tid) {
	if (mSwtichers[tid] == 0) mDrawTimers1[tid].SetEndTime();
	else mDrawTimers2[tid].SetEndTime();
}

void PerfAnalyzer::WholeLoopBeginTime(UINT tid) {
	if (mSwtichers[tid] == 0) mWholeLoopTimers1[tid].SetBeginTime();
	else mWholeLoopTimers2[tid].SetBeginTime();
}

void PerfAnalyzer::WholeLoopEndTime(UINT tid) {
	if (mSwtichers[tid] == 0) {
		mSwtichers[tid] = 1;
		mWholeLoopTimers1[tid].SetEndTime();
	}
	else {
		mSwtichers[tid] = 0;
		mWholeLoopTimers2[tid].SetEndTime();
	}

	if (tid == 0) {
		mTimeStep.SetEndTime();

		if (mTimeStep.GetElapsedTime() > 0.5f) {
			mTimeStep.SetBeginTime();

			if (mSwtichers[0] == 0) {
				for (UINT i = 0; i < mNumThreads; ++i) {
					mRenderer->AddOutputText(
						"LOOP_" + std::to_string(i),
						L"loop[" + std::to_wstring(i) + L"]: " + std::to_wstring(mWholeLoopTimers2[i].GetElapsedTime()),
						10.0f,
						static_cast<float>(40 + 30.0f * i),
						16.0f
					);

					mRenderer->AddOutputText(
						"UPDATE_" + std::to_string(i),
						L"update[" + std::to_wstring(i) + L"]: " + std::to_wstring(mUpdateTimers2[i].GetElapsedTime()),
						10.0f,
						static_cast<float>(40 + 30.0f * (mNumThreads + i)),
						16.0f
					);

					mRenderer->AddOutputText(
						"DRAW_" + std::to_string(i),
						L"draw[" + std::to_wstring(i) + L"]: " + std::to_wstring(mDrawTimers2[i].GetElapsedTime()),
						10.0f,
						static_cast<float>(40 + 30.0f * (mNumThreads + mNumThreads + i)),
						16.0f
					);
				}
			}
			else {
				for (UINT i = 0; i < mNumThreads; ++i) {
					mRenderer->AddOutputText(
						"LOOP_" + std::to_string(i),
						L"loop[" + std::to_wstring(i) + L"]: " + std::to_wstring(mWholeLoopTimers1[i].GetElapsedTime()),
						10.0f,
						static_cast<float>(40 + 30.0f * i),
						16.0f
					);

					mRenderer->AddOutputText(
						"UPDATE_" + std::to_string(i),
						L"update[" + std::to_wstring(i) + L"]: " + std::to_wstring(mUpdateTimers1[i].GetElapsedTime()),
						10.0f,
						static_cast<float>(40 + 30.0f * (mNumThreads + i)),
						16.0f
					);

					mRenderer->AddOutputText(
						"DRAW_" + std::to_string(i),
						L"draw[" + std::to_wstring(i) + L"]: " + std::to_wstring(mDrawTimers1[i].GetElapsedTime()),
						10.0f,
						static_cast<float>(40 + 30.0f * (mNumThreads + mNumThreads + i)),
						16.0f
					);
				}
			}
		}
	}	
}