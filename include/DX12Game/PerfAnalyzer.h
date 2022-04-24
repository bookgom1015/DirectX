#pragma once

#include "DX12Game/ThreadUtil.h"

#include <vector>
#include <minwindef.h>

class PerfAnalyzer {
public:
	PerfAnalyzer() = default;
	virtual ~PerfAnalyzer() = default;

public:
	void Initialize(class Renderer* inRenderer, UINT inNumThreads);

	void UpdateBeginTime(UINT tid);
	void UpdateEndTime(UINT tid);

	void DrawBeginTime(UINT tid);
	void DrawEndTime(UINT tid);

	void WholeLoopBeginTime(UINT tid);
	void WholeLoopEndTime(UINT tid);

private:
	class Renderer* mRenderer;

	UINT mNumThreads;

	TaskTimer mTimeStep;

	std::vector<int> mSwtichers;
	std::vector<TaskTimer> mUpdateTimers1;
	std::vector<TaskTimer> mUpdateTimers2;
	std::vector<TaskTimer> mDrawTimers1;
	std::vector<TaskTimer> mDrawTimers2;
	std::vector<TaskTimer> mWholeLoopTimers1;
	std::vector<TaskTimer> mWholeLoopTimers2;
};