#pragma once

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_pixels.h>
#include <algorithm>
#include <cstdint>

struct RenderTargetSize {
	int width;
	int height;
};

// How to read a render target's pixels back on the CPU. A render target carries the swapchain's
// format, which is the driver's choice: Metal and D3D12 hand out BGRA, and Vulkan hands out RGBA
// wherever it cannot get BGRA, which includes Android. Reading every one of them as BGRA would
// swap channels on those drivers.
//
// The SDL_PIXELFORMAT_*32 names are byte-array names, so they read in the same order as the GPU
// format's own name and the two line up directly. The packed names do not: XRGB8888 is BGRX32 on
// a little-endian machine, which is how this mapping was got backwards once already.
inline SDL_PixelFormat PixelFormatForRenderTarget(SDL_GPUTextureFormat format)
{
	switch (format) {
	case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM:
	case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB:
		return SDL_PIXELFORMAT_BGRX32;
	case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM:
	case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB:
		return SDL_PIXELFORMAT_RGBX32;
	default:
		return SDL_PIXELFORMAT_UNKNOWN;
	}
}

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
