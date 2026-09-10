#include "touchinput.h"

#include <cassert>

int main()
{
	Android_TouchInput input;
	SDL_TouchFingerEvent event{};
	event.touchID = 1;
	event.fingerID = 7;
	event.x = event.y = 0.5f;
	event.type = SDL_EVENT_FINGER_DOWN;
	assert(input.Accept(event));
	assert(!input.Accept(event));
	event.type = SDL_EVENT_FINGER_MOTION;
	assert(input.Accept(event));
	input.Cancel();
	assert(!input.Accept(event));
	event.type = SDL_EVENT_FINGER_UP;
	assert(!input.Accept(event));
	event.type = SDL_EVENT_FINGER_DOWN;
	assert(input.Accept(event));
	// Finger identifiers from different touch devices have independent ownership.
	event.touchID = 2;
	assert(input.Accept(event));
	event.type = SDL_EVENT_FINGER_CANCELED;
	assert(input.Accept(event));
	assert(!input.Accept(event));
	event.touchID = 1;
	event.type = SDL_EVENT_FINGER_UP;
	assert(input.Accept(event));
	assert(!input.Accept(event));

	event.timestamp = SDL_MS_TO_NS(1000);
	assert(!input.DoubleTap(event));
	event.timestamp = SDL_MS_TO_NS(1200);
	assert(input.DoubleTap(event));
	event.timestamp = SDL_MS_TO_NS(1300);
	assert(!input.DoubleTap(event));
	input.Cancel();
	event.timestamp = SDL_MS_TO_NS(1400);
	assert(!input.DoubleTap(event));
	event.type = SDL_EVENT_FINGER_CANCELED;
	event.timestamp = SDL_MS_TO_NS(1500);
	assert(!input.DoubleTap(event));
	event.type = SDL_EVENT_FINGER_UP;
	event.timestamp = SDL_MS_TO_NS(1600);
	assert(!input.DoubleTap(event));
	event.timestamp = SDL_MS_TO_NS(2100);
	assert(!input.DoubleTap(event));
	event.timestamp = SDL_MS_TO_NS(2200);
	event.x = 0.8f;
	assert(!input.DoubleTap(event));
	event.timestamp = SDL_MS_TO_NS(2300);
	assert(input.DoubleTap(event));
}
