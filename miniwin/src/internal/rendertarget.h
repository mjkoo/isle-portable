#pragma once

#include <algorithm>
#include <cstdint>

struct RenderTargetSize {
	int width;
	int height;
};

inline RenderTargetSize FitRenderTarget(int width, int height, int maxWidth, int maxHeight)
{
	if (width <= 0 || height <= 0 || maxWidth <= 0 || maxHeight <= 0) {
		return {0, 0};
	}
	if (width <= maxWidth && height <= maxHeight) {
		return {width, height};
	}
	// Integer ratios keep the limiting axis exact and avoid rounding above a hardware limit.
	if ((int64_t) maxWidth * height <= (int64_t) maxHeight * width) {
		return {maxWidth, std::max(1, (int) ((int64_t) height * maxWidth / width))};
	}
	return {std::max(1, (int) ((int64_t) width * maxHeight / height)), maxHeight};
}
