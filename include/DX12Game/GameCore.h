#pragma once

// Link necessary d3d12 libraries
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

const int gNumFrameResources = 3;

#include <atomic>
#include <cmath>
#include <cstdint>
#include <ctgmath>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <float.h>
#include <functional>
#include <future>
#include <initializer_list>
#include <iomanip>
#include <mutex>
#include <queue>
#include <deque>
#include <string>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>
#include <Windows.h>
#include <windowsx.h>

#include "common/MathHelper.h"
#include "common/d3dUtil.h"
#include "DX12Game/GameTimer.h"
#include "DX12Game/StringUtil.h"
#include "DX12Game/ThreadUtil.h"