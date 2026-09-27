#include "extensions/firstpersoncamera/controller.h"

#include "extensions/common/arearestriction.h"
#include "legocameracontroller.h"
#include "legogamestate.h"
#include "legomain.h"
#include "legonavcontroller.h"
#include "legopathactor.h"
#include "legoworld.h"
#include "misc.h"
#include "mxgeometry/mxgeometry3d.h"
#include "roi/legoroi.h"

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_timer.h>

using namespace Extensions::FirstPersonCamera;

// The eye the game puts on every actor it hands the camera to.
static const float c_eyeHeight = 1.25f;

bool Controller::CanLookNow() const
{
	if (!m_focused || !Lego() || Lego()->IsPaused()) {
		return false;
	}
	LegoPathActor* actor = UserActor();
	LegoWorld* world = CurrentWorld();
	if (!actor || !actor->GetROI() || !world || !world->GetCameraController()) {
		return false;
	}

	LookSituation situation;
	situation.m_cameraOnUser = world->GetCameraController()->GetEntity() == actor && actor->GetCameraFlag();
	situation.m_userDisabled = (actor->GetActorState() & LegoPathActor::c_maxState) == LegoPathActor::c_disabled;
	situation.m_restrictedArea = Common::IsRestrictedArea(GameState()->m_currentArea);
	situation.m_helicopter = actor->IsA("Helicopter");
	// The island in the acts where the player walks and drives it. Buildings, races and the other
	// screens are worlds of their own, and Act 3 is flown in the helicopter.
	situation.m_freeRoamWorld = world->GetWorldId() == LegoOmni::e_act1 || world->GetWorldId() == LegoOmni::e_act2;
	return CanLook(situation);
}

bool Controller::SameSituation() const
{
	return UserActor() == m_activeActor && CurrentWorld() == m_activeWorld;
}

void Controller::Tick()
{
	Uint64 now = SDL_GetTicksNS();
	float deltaTime = m_lastTick ? SDL_min((now - m_lastTick) / (float) SDL_NS_PER_SECOND, 0.1f) : 0.0f;
	m_lastTick = now;

	bool canLook = CanLookNow();
	if (canLook != m_active) {
		if (!canLook) {
			CancelInput();
			RestoreVanillaEye();
		}
		m_look.Reset();
		m_active = canLook;
		m_activeActor = canLook ? UserActor() : nullptr;
		m_activeWorld = canLook ? CurrentWorld() : nullptr;
	}
	else if (m_active && !SameSituation()) {
		// A new actor or world took over between ticks; each starts looking straight ahead.
		CancelInput();
		m_look.Reset();
		m_activeActor = UserActor();
		m_activeWorld = CurrentWorld();
	}

	if (!m_active) {
		return;
	}

	if (!m_freeCursor) {
		m_look.Turn(m_sticks.X() * c_stickYawDegreesPerSecond * deltaTime);
		// Stick up is negative.
		m_look.Tilt(-m_sticks.Y() * c_stickPitchRadiansPerSecond * deltaTime);
	}

	ApplyEye();
}

void Controller::ApplyEye()
{
	LegoPathActor* actor = UserActor();
	const Matrix4& transform = actor->GetROI()->GetLocal2World();
	LookVector right = {transform[0][0], transform[0][1], transform[0][2]};
	LookVector up = {transform[1][0], transform[1][1], transform[1][2]};
	LookVector forward = {transform[2][0], transform[2][1], transform[2][2]};

	LookVector dir, eyeUp;
	if (!LevelEye(right, up, forward, m_look.Pitch(), dir, eyeUp)) {
		return;
	}

	CurrentWorld()->GetCameraController()->SetWorldTransform(
		Mx3DPointFloat(0.0f, c_eyeHeight, 0.0f),
		Mx3DPointFloat(dir.x, dir.y, dir.z),
		Mx3DPointFloat(eyeUp.x, eyeUp.y, eyeUp.z)
	);
	actor->TransformPointOfView();
}

// Hand the camera back as the game set it. Only for the actor and world look was running in: a
// new actor or world has already had its own eye put in place, which must stand.
void Controller::RestoreVanillaEye()
{
	if (!SameSituation() || !m_activeActor) {
		return;
	}

	LegoPathActor* actor = UserActor();
	LegoWorld* world = CurrentWorld();
	if (!world || !world->GetCameraController() || world->GetCameraController()->GetEntity() != actor) {
		return;
	}

	// The view itself is left alone: whatever ended look, such as a scripted animation, may be
	// driving it now, and the actor's next move puts this eye to use.
	world->GetCameraController()->SetWorldTransform(
		Mx3DPointFloat(0.0f, c_eyeHeight, 0.0f),
		Mx3DPointFloat(0.0f, 0.0f, 1.0f),
		Mx3DPointFloat(0.0f, 1.0f, 0.0f)
	);
}

void Controller::TurnActor(LegoNavController* p_nav)
{
	float yaw = m_look.TakeYaw();
	if (!IsLookActive() || yaw == 0.0f) {
		return;
	}

	p_nav->m_additionalRotationY += yaw;
	p_nav->m_keyPressed = TRUE;
}

bool Controller::HandleSDLEvent(SDL_Event* p_event)
{
	if (p_event->type == SDL_EVENT_WINDOW_FOCUS_LOST) {
		m_focused = false;
		CancelInput();
	}
	else if (p_event->type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
		m_focused = true;
	}
	if (!CanLookNow() || !SameSituation()) {
		CancelInput();
	}

	switch (p_event->type) {
	case SDL_EVENT_GAMEPAD_AXIS_MOTION:
		if (CanLookNow()) {
			m_sticks
				.Axis(p_event->gaxis.which, static_cast<SDL_GamepadAxis>(p_event->gaxis.axis), p_event->gaxis.value);
		}
		break;
	case SDL_EVENT_GAMEPAD_REMOVED:
		m_sticks.Remove(p_event->gdevice.which);
		break;
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP:
		if (p_event->button.which == SDL_TOUCH_MOUSEID || p_event->button.button != SDL_BUTTON_RIGHT) {
			break;
		}
		if (!p_event->button.down) {
			bool consumed = m_mouse.End();
			ReleaseMouse();
			return consumed;
		}
		if (IsLookActive()) {
			m_mouse.Begin(false);
			SDL_Window* window = SDL_GetWindowFromID(p_event->button.windowID);
			if (window) {
				SDL_GetMouseState(&m_savedMouseX, &m_savedMouseY);
				if (SDL_SetWindowRelativeMouseMode(window, true)) {
					m_relativeWindow = p_event->button.windowID;
					m_mouse.Begin(true);
				}
				else {
					SDL_Log("Could not capture first-person mouse look: %s", SDL_GetError());
				}
			}
			return true;
		}
		break;
	case SDL_EVENT_MOUSE_MOTION:
		if (m_mouse.OwnsMotion() && p_event->motion.which != SDL_TOUCH_MOUSEID) {
			if (m_mouse.CanLook() && IsLookActive()) {
				m_look.Turn(p_event->motion.xrel * c_mouseYawDegreesPerPixel);
				m_look.Tilt(-p_event->motion.yrel * c_mousePitchRadiansPerPixel);
			}
			return true;
		}
		break;
	}
	return false;
}

TouchResult Controller::HandleTouch(SDL_Event* p_event, bool p_stickScheme)
{
	if (!IsLookActive()) {
		m_touch.Cancel();
	}
	auto result = m_touch.Handle(p_event->tfinger, IsLookActive() && p_stickScheme);
	m_look.Turn(result.dx * c_touchYawDegreesPerUnit);
	m_look.Tilt(-result.dy * c_touchPitchRadiansPerUnit);
	return result;
}

void Controller::CancelInput()
{
	m_sticks.Cancel();
	m_touch.Cancel();
	m_lastTick = 0;
	m_freeCursor = false;
	ReleaseMouse();
	m_look.TakeYaw();
}

void Controller::SetFreeCursor(bool p_held)
{
	m_freeCursor = p_held;
}

void Controller::ReleaseMouse()
{
	m_mouse.Cancel();
	if (!m_relativeWindow) {
		return;
	}

	SDL_Window* window = SDL_GetWindowFromID(m_relativeWindow);
	m_relativeWindow = 0;
	if (window && SDL_GetWindowRelativeMouseMode(window)) {
		SDL_WarpMouseInWindow(window, m_savedMouseX, m_savedMouseY);
		SDL_SetWindowRelativeMouseMode(window, false);
	}
}
