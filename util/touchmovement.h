#ifndef TOUCHMOVEMENT_H
#define TOUCHMOVEMENT_H

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_rect.h>
#include <cmath>
#include <map>

// Platform touch movement uses normalized game-viewport coordinates, which can be
// outside [0, 1] when a finger is in a letterbox bar.
namespace TouchMovement
{
enum Direction {
	Left = 1,
	Right = 2,
	Up = 4,
	Down = 8
};

struct State {
	bool stickActive = false;
	SDL_FPoint origin = {0, 0};
	SDL_FPoint axes = {0, 0};
	Uint32 directions = 0;
};

inline bool Inside(float p_x, float p_y)
{
	return p_x >= 0 && p_x <= 1 && p_y >= 0 && p_y <= 1;
}

inline Uint32 Region(float p_x, float p_y)
{
	if (!Inside(p_x, p_y)) {
		return 0;
	}
	if (p_y <= 3.0 / 4.0) {
		return Up;
	}
	return p_x < 1.0 / 3.0 ? Left : (p_x > 2.0 / 3.0 ? Right : Down);
}

inline void UpdateArrows(const SDL_TouchFingerEvent& p_event, std::map<SDL_FingerID, Uint32>& p_fingers)
{
	if (p_event.type == SDL_EVENT_FINGER_UP || p_event.type == SDL_EVENT_FINGER_CANCELED) {
		p_fingers.erase(p_event.fingerID);
	}
	else if (p_event.type == SDL_EVENT_FINGER_DOWN) {
		if (Inside(p_event.x, p_event.y)) {
			p_fingers[p_event.fingerID] = Region(p_event.x, p_event.y);
		}
	}
	else if (p_event.type == SDL_EVENT_FINGER_MOTION) {
		auto finger = p_fingers.find(p_event.fingerID);
		if (finger != p_fingers.end()) {
			finger->second = Region(p_event.x, p_event.y);
		}
	}
}

inline void UpdateStick(
	const SDL_TouchFingerEvent& p_event,
	SDL_FingerID& p_finger,
	SDL_FPoint& p_origin,
	SDL_Point& p_axes
)
{
	if (p_event.type == SDL_EVENT_FINGER_DOWN) {
		if (!p_finger && Inside(p_event.x, p_event.y)) {
			p_finger = p_event.fingerID;
			p_origin = {p_event.x, p_event.y};
			p_axes = {0, 0};
		}
	}
	else if (p_finger && p_event.fingerID == p_finger) {
		if (p_event.type == SDL_EVENT_FINGER_UP || p_event.type == SDL_EVENT_FINGER_CANCELED) {
			p_finger = 0;
			p_origin = {0, 0};
			p_axes = {0, 0};
		}
		else if (p_event.type == SDL_EVENT_FINGER_MOTION && std::isfinite(p_event.x) && std::isfinite(p_event.y)) {
			constexpr float radius = 0.25f;
			p_axes = {
				static_cast<int>(SDL_clamp(p_event.x - p_origin.x, -radius, radius) / radius * 32767.0f),
				static_cast<int>(SDL_clamp(p_event.y - p_origin.y, -radius, radius) / radius * 32767.0f),
			};
		}
	}
}
} // namespace TouchMovement

#endif
