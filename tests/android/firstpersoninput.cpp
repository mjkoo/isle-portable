#include "extensions/firstpersoncamera/input.h"

#include <cassert>
#include <cmath>

using namespace Extensions::FirstPersonCamera;

static SDL_TouchFingerEvent Finger(
	SDL_EventType p_type,
	float p_x,
	float p_y,
	SDL_TouchID p_touch = 1,
	SDL_FingerID p_finger = 1
)
{
	SDL_TouchFingerEvent event{};
	event.type = p_type;
	event.touchID = p_touch;
	event.fingerID = p_finger;
	event.x = p_x;
	event.y = p_y;
	return event;
}

int main()
{
	TouchLook touch;
	assert(touch.Handle(Finger(SDL_EVENT_FINGER_DOWN, 0.2f, 0.5f), true).disposition == TouchDisposition::Pass);
	assert(touch.Handle(Finger(SDL_EVENT_FINGER_DOWN, 0.7f, 0.5f), false).disposition == TouchDisposition::Pass);
	assert(touch.Handle(Finger(SDL_EVENT_FINGER_DOWN, 0.7f, 0.5f), true).disposition == TouchDisposition::Consume);
	assert(touch.Handle(Finger(SDL_EVENT_FINGER_MOTION, 0.704f, 0.5f), true).dx == 0.0f);
	auto tap = touch.Handle(Finger(SDL_EVENT_FINGER_UP, 0.704f, 0.5f), true);
	assert(tap.disposition == TouchDisposition::Tap && tap.x == 0.7f && tap.y == 0.5f);

	// The look finger does not consume a second device's identically numbered finger or the walking finger.
	touch.Handle(Finger(SDL_EVENT_FINGER_DOWN, 0.7f, 0.5f), true);
	assert(touch.Handle(Finger(SDL_EVENT_FINGER_MOTION, 0.8f, 0.5f, 2), true).disposition == TouchDisposition::Pass);
	assert(touch.Handle(Finger(SDL_EVENT_FINGER_DOWN, 0.2f, 0.5f, 1, 2), true).disposition == TouchDisposition::Pass);
	auto drag = touch.Handle(Finger(SDL_EVENT_FINGER_MOTION, 0.72f, 0.5f), true);
	assert(drag.disposition == TouchDisposition::Consume && std::fabs(drag.dx - 0.02f) < 0.00001f);
	assert(touch.Handle(Finger(SDL_EVENT_FINGER_UP, 0.7f, 0.5f), true).disposition == TouchDisposition::Consume);

	// Even a release with no preceding motion is not a tap if it moved beyond the threshold.
	touch.Handle(Finger(SDL_EVENT_FINGER_DOWN, 0.7f, 0.5f), true);
	assert(touch.Handle(Finger(SDL_EVENT_FINGER_UP, 0.8f, 0.5f), true).disposition == TouchDisposition::Consume);

	// Losing eligibility or canceling never creates a click, even if eligibility returns before release.
	touch.Handle(Finger(SDL_EVENT_FINGER_DOWN, 0.7f, 0.5f), true);
	touch.Handle(Finger(SDL_EVENT_FINGER_MOTION, 0.7f, 0.5f), false);
	assert(touch.Handle(Finger(SDL_EVENT_FINGER_UP, 0.7f, 0.5f), true).disposition == TouchDisposition::Consume);
	touch.Handle(Finger(SDL_EVENT_FINGER_DOWN, 0.7f, 0.5f), true);
	touch.Cancel();
	assert(touch.Handle(Finger(SDL_EVENT_FINGER_MOTION, 0.8f, 0.5f), true).dx == 0.0f);
	assert(touch.Handle(Finger(SDL_EVENT_FINGER_UP, 0.8f, 0.5f), true).disposition == TouchDisposition::Consume);
	touch.Handle(Finger(SDL_EVENT_FINGER_DOWN, 0.7f, 0.5f), true);
	assert(touch.Handle(Finger(SDL_EVENT_FINGER_CANCELED, 0.7f, 0.5f), true).disposition == TouchDisposition::Consume);

	// A lifecycle filter can discard the canceled contact's release. A new contact still works.
	touch.Handle(Finger(SDL_EVENT_FINGER_DOWN, 0.7f, 0.5f), true);
	touch.Cancel();
	touch.Handle(Finger(SDL_EVENT_FINGER_DOWN, 0.7f, 0.5f, 1, 2), true);
	assert(touch.Handle(Finger(SDL_EVENT_FINGER_UP, 0.7f, 0.5f, 1, 2), true).disposition == TouchDisposition::Tap);
	touch.Handle(Finger(SDL_EVENT_FINGER_DOWN, 0.7f, 0.5f), true);
	assert(touch.Handle(Finger(SDL_EVENT_FINGER_UP, 0.7f, 0.5f), true).disposition == TouchDisposition::Tap);

	// Failed capture and pause/focus cancellation own the release without accepting motion.
	MouseLookGesture mouse;
	assert(!mouse.End());
	mouse.Begin(false);
	assert(mouse.OwnsMotion() && !mouse.CanLook());
	assert(mouse.End());
	assert(!mouse.OwnsMotion());
	mouse.Begin(true);
	assert(mouse.CanLook());
	mouse.Cancel();
	assert(mouse.OwnsMotion() && !mouse.CanLook());
	assert(mouse.End());
	assert(!mouse.CanLook());
	mouse.Begin(true);
	assert(mouse.CanLook());
	assert(mouse.End());

	LookSticks sticks;
	sticks.Axis(1, SDL_GAMEPAD_AXIS_RIGHTX, 32767);
	sticks.Axis(2, SDL_GAMEPAD_AXIS_RIGHTY, -32768);
	assert(sticks.X() == 0.0f && sticks.Y() == -1.0f);
	sticks.Axis(1, SDL_GAMEPAD_AXIS_RIGHTY, 100);
	assert(sticks.X() == 0.0f && sticks.Y() == -1.0f);
	sticks.Remove(1);
	assert(sticks.Y() == -1.0f);
	sticks.Axis(2, SDL_GAMEPAD_AXIS_RIGHTY, 0);
	assert(sticks.X() == 0.0f && sticks.Y() == 0.0f);
	sticks.Axis(2, SDL_GAMEPAD_AXIS_RIGHTX, 32767);
	sticks.Remove(2);
	assert(sticks.X() == 0.0f && sticks.Y() == 0.0f);
	sticks.Axis(1, SDL_GAMEPAD_AXIS_RIGHTX, 32767);
	sticks.Cancel();
	assert(sticks.X() == 0.0f && sticks.Y() == 0.0f);
	return 0;
}
