#include "rendertarget.h"

#include <cassert>
#include <cmath>
#include <limits>

static void CheckFit(int width, int height, int maxWidth, int maxHeight)
{
	RenderTargetSize result = FitRenderTarget(width, height, maxWidth, maxHeight);
	assert(result.width > 0 && result.width <= maxWidth && result.width <= width);
	assert(result.height > 0 && result.height <= maxHeight && result.height <= height);
	// Rounding may lose less than one pixel on each axis, but must not stretch the image.
	double scale = std::min({1.0, (double) maxWidth / width, (double) maxHeight / height});
	assert(std::abs(result.width - width * scale) < 1.0);
	assert(std::abs(result.height - height * scale) < 1.0);
}

int main()
{
	// A portrait target includes letterboxing beyond its 1280 x 960 content.
	RenderTargetSize portrait = FitRenderTarget(1280, 2276, 2048, 2048);
	assert(portrait.width == 1151 && portrait.height == 2048);
	RenderTargetSize landscape = FitRenderTarget(2276, 1280, 2048, 2048);
	assert(landscape.width == 2048 && landscape.height == 1151);
	RenderTargetSize unchanged = FitRenderTarget(1707, 960, 2048, 2048);
	assert(unchanged.width == 1707 && unchanged.height == 960);
	for (int limit : {1024, 2048, 4096}) {
		CheckFit(1280, 2276, limit, limit);
		CheckFit(2276, 1280, limit, limit);
		CheckFit(1067, 480, limit, limit);
	}
	CheckFit(1280, 2276, 1024, 2048);
	CheckFit(2276, 1280, 2048, 1024);
	CheckFit(std::numeric_limits<int>::max(), std::numeric_limits<int>::max(), 2048, 2048);
	assert(FitRenderTarget(0, 480, 2048, 2048).width == 0);
	assert(FitRenderTarget(640, 480, 0, 2048).height == 0);
	assert(FitRenderTarget(640, 480, 2048, -1).height == 0);
}
