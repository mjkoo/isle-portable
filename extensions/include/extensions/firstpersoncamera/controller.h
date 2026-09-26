#pragma once

#include "extensions/firstpersoncamera/look.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_stdinc.h>

class LegoNavController;

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

	void HandleSDLEvent(SDL_Event* p_event);

	// Returns whether the finger is the camera's, so the game's own touch handling skips it.
	bool HandleTouch(SDL_Event* p_event, bool p_stickScheme);

	// Drop every held input without acting on it, as for the pause menu.
	void CancelInput();

	bool IsLooking() const { return m_active && !m_freeCursor; }
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
	const void* m_activeActor = nullptr;
	const void* m_activeWorld = nullptr;
	Uint64 m_lastTick = 0;

	Sint16 m_stickX = 0;
	Sint16 m_stickY = 0;

	bool m_rightButtonHeld = false;
	SDL_WindowID m_relativeWindow = 0;
	float m_savedMouseX = 0.0f;
	float m_savedMouseY = 0.0f;

	bool m_fingerHeld = false;
	SDL_FingerID m_finger = 0;
	float m_fingerX = 0.0f;
	float m_fingerY = 0.0f;
};

} // namespace FirstPersonCamera
} // namespace Extensions
