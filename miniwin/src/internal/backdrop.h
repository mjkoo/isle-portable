#pragma once

#include <SDL3/SDL_rect.h>
#include <array>
#include <cmath>

// Whether a 2D blit is a backdrop: an opaque image over the whole game area, as the 4:3 scenes
// such as the Information Center draw every frame. Behind a backdrop the 3D scene can only show in
// the letterbox bars, which should stay black. Without one the 3D scene is the picture, drawn
// across the whole window, and its background belongs across the whole window too.
inline bool IsBackdrop(const SDL_Rect& p_dst, int p_virtualWidth, int p_virtualHeight, bool p_colorKeyed)
{
	return !p_colorKeyed && p_dst.x <= 0 && p_dst.y <= 0 && p_dst.x + p_dst.w >= p_virtualWidth &&
		   p_dst.y + p_dst.h >= p_virtualHeight;
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
