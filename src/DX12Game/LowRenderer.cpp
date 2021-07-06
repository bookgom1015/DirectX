#include "DX12Game/LowRenderer.h"

float LowRenderer::AspectRatio() const {
	return static_cast<float>(mClientWidth / mClientHeight);
}