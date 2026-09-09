#include "touchmovement.h"

#include <cassert>
#include <limits>

static SDL_TouchFingerEvent Finger(SDL_EventType p_type, SDL_FingerID p_id, float p_x, float p_y)
{
	SDL_TouchFingerEvent event{};
	event.type = p_type;
	event.fingerID = p_id;
	event.x = p_x;
	event.y = p_y;
	return event;
}

int main()
{
	using namespace TouchMovement;
	assert(Region(0, 0) == Up);
	assert(Region(1, .75f) == Up);
	assert(Region(.1f, std::nextafter(.75f, 1.0f)) == Left);
	assert(Region(.5f, 1) == Down);
	assert(Region(1, 1) == Right);
	assert(Region(std::nextafter(1.0f / 3, 0.0f), 1) == Left);
	assert(Region(1.0f / 3, 1) == Down);
	assert(Region(std::nextafter(2.0f / 3, 0.0f), 1) == Down);
	assert(Region(2.0f / 3, 1) == Right);
	assert(Region(-.01f, .5f) == 0);
	assert(Region(.5f, 1.01f) == 0);
	assert(Region(std::numeric_limits<float>::quiet_NaN(), .5f) == 0);

	std::map<SDL_FingerID, Uint32> arrows;
	UpdateArrows(Finger(SDL_EVENT_FINGER_DOWN, 1, -.1f, .5f), arrows);
	UpdateArrows(Finger(SDL_EVENT_FINGER_MOTION, 1, .5f, .5f), arrows);
	assert(arrows.empty()); // Entering from a bar never acquires movement.
	UpdateArrows(Finger(SDL_EVENT_FINGER_DOWN, 2, .1f, .9f), arrows);
	UpdateArrows(Finger(SDL_EVENT_FINGER_DOWN, 3, .5f, .5f), arrows);
	assert((arrows.at(2) | arrows.at(3)) == (Left | Up));
	UpdateArrows(Finger(SDL_EVENT_FINGER_MOTION, 2, -.1f, .9f), arrows);
	assert(arrows.at(2) == 0 && arrows.at(3) == Up);
	UpdateArrows(Finger(SDL_EVENT_FINGER_MOTION, 2, .9f, .9f), arrows);
	assert(arrows.at(2) == Right);
	UpdateArrows(Finger(SDL_EVENT_FINGER_UP, 2, 2, 2), arrows);
	UpdateArrows(Finger(SDL_EVENT_FINGER_CANCELED, 3, -1, -1), arrows);
	assert(arrows.empty());
	UpdateArrows(Finger(SDL_EVENT_FINGER_MOTION, 3, .5f, .5f), arrows);
	assert(arrows.empty());

	SDL_FingerID owner = 0;
	SDL_FPoint origin{};
	SDL_Point axes{};
	auto stick = [&](SDL_EventType type, SDL_FingerID id, float x, float y) {
		UpdateStick(Finger(type, id, x, y), owner, origin, axes);
	};
	stick(SDL_EVENT_FINGER_DOWN, 1, -.1f, .5f);
	stick(SDL_EVENT_FINGER_MOTION, 1, .5f, .5f);
	assert(owner == 0 && axes.x == 0 && axes.y == 0);
	stick(SDL_EVENT_FINGER_DOWN, 2, .5f, .5f);
	stick(SDL_EVENT_FINGER_DOWN, 3, .1f, .1f);
	stick(SDL_EVENT_FINGER_MOTION, 3, 0, 0);
	assert(owner == 2 && origin.x == .5f && origin.y == .5f && axes.x == 0);
	stick(SDL_EVENT_FINGER_MOTION, 2, .625f, .375f);
	assert(axes.x == 16383 && axes.y == -16383);
	stick(SDL_EVENT_FINGER_MOTION, 2, 2, -2);
	assert(owner == 2 && axes.x == 32767 && axes.y == -32767);
	stick(SDL_EVENT_FINGER_UP, 3, 0, 0);
	assert(owner == 2 && axes.x == 32767);
	stick(SDL_EVENT_FINGER_CANCELED, 2, 2, -2);
	assert(owner == 0 && axes.x == 0 && axes.y == 0 && origin.x == 0 && origin.y == 0);
	stick(SDL_EVENT_FINGER_MOTION, 2, .8f, .8f);
	assert(owner == 0 && axes.x == 0);
	stick(SDL_EVENT_FINGER_DOWN, 4, 0, 1);
	stick(SDL_EVENT_FINGER_UP, 4, -.1f, 1.1f);
	assert(owner == 0 && axes.x == 0 && origin.y == 0);
}
