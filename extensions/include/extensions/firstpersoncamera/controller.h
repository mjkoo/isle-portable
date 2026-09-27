#pragma once

#include "extensions/firstpersoncamera/input.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_stdinc.h>

class LegoNavController;
class LegoPathActor;
class LegoWorld;

namespace Extensions
{
namespace FirstPersonCamera
{

class Controller {
public:
	void SetInverted(bool p_inverted) { m_look.SetInverted(p_inverted); }

	// Once per tick: take stick input, follow the game into and out of situations where free look
	// applies, and put the level eye on the camera.
	void Tick();

	// From the navigation step, before it integrates velocity: hand over the turn looking has
	// collected, through the same one-shot rotation a key nudge uses.
	void TurnActor(LegoNavController* p_nav);

	bool HandleSDLEvent(SDL_Event* p_event);

	// Arbitrate taps and look drags before the app queues pointer notifications.
	TouchResult HandleTouch(SDL_Event* p_event, bool p_stickScheme);

	// Drop every held input without acting on it, as for the pause menu.
	void CancelInput();

	bool IsLookActive() const { return m_active && CanLookNow() && SameSituation(); }
	bool IsLooking() const { return IsLookActive() && !m_freeCursor; }
	void SetFreeCursor(bool p_held);
	void Recenter() { m_look.Recenter(); }

private:
	bool CanLookNow() const;
	void ApplyEye();
	void RestoreVanillaEye();
	void ReleaseMouse();
	bool SameSituation() const;

	Look m_look;
	bool m_active = false;
	bool m_freeCursor = false;
	LegoPathActor* m_activeActor = nullptr;
	LegoWorld* m_activeWorld = nullptr;
	Uint64 m_lastTick = 0;

	LookSticks m_sticks;
	TouchLook m_touch;
	bool m_focused = true;
	MouseLookGesture m_mouse;

	SDL_WindowID m_relativeWindow = 0;
	float m_savedMouseX = 0.0f;
	float m_savedMouseY = 0.0f;
};

} // namespace FirstPersonCamera
} // namespace Extensions
