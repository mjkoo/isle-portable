#pragma once

#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_surface.h>
#include <array>
#include <cmath>
#include <cstring>

// Whether a 2D blit is a backdrop: an opaque image over the whole game area, as the 4:3 scenes
// such as the Information Center draw every frame. Behind a backdrop the 3D scene can only show in
// the letterbox bars, which should stay black. Without one the 3D scene is the picture, drawn
// across the whole window, and its background belongs across the whole window too.
inline bool CoversGameArea(const SDL_Rect& p_dst, int p_virtualWidth, int p_virtualHeight)
{
	return p_dst.x <= 0 && p_dst.y <= 0 && p_dst.x + p_dst.w >= p_virtualWidth && p_dst.y + p_dst.h >= p_virtualHeight;
}

// The blit flag alone does not describe transparency: the game sets it for opaque bitmaps too.
// Conversely, a transfer-buffer upload can contain alpha even without a color-key flag.
inline bool IsOpaqueBackdropSurface(SDL_Surface* p_surface, const SDL_Rect& p_src)
{
	if (!p_surface || SDL_SurfaceHasColorKey(p_surface) || p_src.x < 0 || p_src.y < 0 || p_src.w <= 0 || p_src.h <= 0 ||
		p_src.x + p_src.w > p_surface->w || p_src.y + p_src.h > p_surface->h) {
		return false;
	}
	const SDL_PixelFormatDetails* format = SDL_GetPixelFormatDetails(p_surface->format);
	SDL_Palette* palette = SDL_GetSurfacePalette(p_surface);
	if (palette) {
		for (int i = 0; i < palette->ncolors; i++) {
			if (palette->colors[i].a != 255) {
				return false;
			}
		}
		return true;
	}
	if (!format->Amask) {
		return true;
	}
	// miniwin's transfer buffers are RGBA32. Reject other alpha formats conservatively.
	if (format->bytes_per_pixel != 4 || !SDL_LockSurface(p_surface)) {
		return false;
	}
	bool opaque = true;
	for (int y = p_src.y; opaque && y < p_src.y + p_src.h; y++) {
		const auto* row = static_cast<const unsigned char*>(p_surface->pixels) + y * p_surface->pitch;
		for (int x = p_src.x; x < p_src.x + p_src.w; x++) {
			Uint32 pixel;
			std::memcpy(&pixel, row + x * 4, sizeof(pixel));
			if ((pixel & format->Amask) != format->Amask) {
				opaque = false;
				break;
			}
		}
	}
	SDL_UnlockSurface(p_surface);
	return opaque;
}

// Rectangles outside the logical game area, extended to the target edges. Rounding outward
// covers fractional letterboxes without painting over a game pixel.
inline std::array<SDL_Rect, 4> BackdropBars(
	int p_width,
	int p_height,
	int p_virtualWidth,
	int p_virtualHeight,
	float p_scale,
	float p_offsetX,
	float p_offsetY
)
{
	if (p_scale <= 0.0f) {
		return {};
	}
	int left = static_cast<int>(std::ceil(p_offsetX / p_scale));
	int top = static_cast<int>(std::ceil(p_offsetY / p_scale));
	int right = static_cast<int>(std::ceil((p_width - p_offsetX) / p_scale)) - p_virtualWidth;
	int bottom = static_cast<int>(std::ceil((p_height - p_offsetY) / p_scale)) - p_virtualHeight;
	return {
		{{-left, -top, left, p_virtualHeight + top + bottom},
		 {p_virtualWidth, -top, right, p_virtualHeight + top + bottom},
		 {0, -top, p_virtualWidth, top},
		 {0, p_virtualHeight, p_virtualWidth, bottom}}
	};
}

class BackdropFrame {
public:
	void Note() { m_present = true; }
	bool Finish()
	{
		bool present = m_present;
		m_present = false;
		return present;
	}

private:
	bool m_present = false;
};
