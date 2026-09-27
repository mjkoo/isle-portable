#pragma once

#include "extensions/extensions.h"
#include "extensions/firstpersoncamera/input.h"
#include "mxtypes.h"

#include <SDL3/SDL_events.h>
#include <map>
#include <string>

class LegoNavController;

namespace Extensions
{

namespace FirstPersonCamera
{
class Controller;
}

// The game's own eye camera, kept level with the world and free to look around: the right stick,
// a right-button mouse drag or a drag on the right half of the screen turns the player and tilts
// the view. On a gamepad, clicks land on a crosshair at the center of the view.
//
// Third person replaces the eye camera altogether, so the two never run together; multiplayer,
// which forces third person, rules this out too.
class FirstPersonCameraExt {
public:
	static void Initialize();

	static void HandleCreate();
	static void HandleNavTurn(LegoNavController* p_nav);
	LEGO1_EXPORT static FirstPersonCamera::TouchResult HandleTouchInput(SDL_Event* p_event, bool p_stickScheme);
	static void CancelPointerInput();

	// For the app's event loop and gamepad handling, which live outside LEGO1. Each does nothing
	// while the extension is off.
	LEGO1_EXPORT static bool HandleSDLEvent(SDL_Event* p_event);
	// Whether look runs and the free cursor is not held: the right stick is the view's, not the
	// cursor's.
	LEGO1_EXPORT static bool IsLooking();
	// Held to let the right stick move the cursor again, for dashboards and other 2D controls.
	LEGO1_EXPORT static void SetFreeCursor(bool p_held);
	LEGO1_EXPORT static void Recenter();

	static std::map<std::string, std::string> options;
	static bool enabled;

private:
	static FirstPersonCamera::Controller* s_controller;
	static bool s_registered;
};

namespace FP
{
#ifdef EXTENSIONS
constexpr auto HandleCreate = &FirstPersonCameraExt::HandleCreate;
constexpr auto HandleNavTurn = &FirstPersonCameraExt::HandleNavTurn;
constexpr auto CancelPointerInput = &FirstPersonCameraExt::CancelPointerInput;
#else
constexpr decltype(&FirstPersonCameraExt::HandleCreate) HandleCreate = nullptr;
constexpr decltype(&FirstPersonCameraExt::HandleNavTurn) HandleNavTurn = nullptr;
constexpr decltype(&FirstPersonCameraExt::CancelPointerInput) CancelPointerInput = nullptr;
#endif
} // namespace FP

}; // namespace Extensions
