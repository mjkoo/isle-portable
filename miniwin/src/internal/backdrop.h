#pragma once

#include <SDL3/SDL_rect.h>

// Whether a 2D blit is a backdrop: an opaque image over the whole game area, as the 4:3 scenes
// such as the Information Center draw every frame. Behind a backdrop the 3D scene can only show in
// the letterbox bars, which should stay black. Without one the 3D scene is the picture, drawn
// across the whole window, and its background belongs across the whole window too.
inline bool IsBackdrop(const SDL_Rect& p_dst, int p_virtualWidth, int p_virtualHeight, bool p_colorKeyed)
{
	return !p_colorKeyed && p_dst.x <= 0 && p_dst.y <= 0 && p_dst.x + p_dst.w >= p_virtualWidth &&
		   p_dst.y + p_dst.h >= p_virtualHeight;
}
