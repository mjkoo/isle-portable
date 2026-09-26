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

#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_timer.h>

using namespace Extensions::FirstPersonCamera;

// The eye the game puts on every actor it hands the camera to.
static const float c_eyeHeight = 1.25f;

// A finger that lands on this side of the screen looks; the other side keeps the virtual stick.
static const float c_touchLookZoneX = 0.5f;

bool Controller::CanLookNow() const
{
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
			ReleaseMouse();
			RestoreVanillaEye();
		}
		m_look.Reset();
		m_active = canLook;
		m_activeActor = canLook ? UserActor() : nullptr;
		m_activeWorld = canLook ? CurrentWorld() : nullptr;
	}
	else if (m_active && !SameSituation()) {
		// A new actor or world took over between ticks; each starts looking straight ahead.
		m_look.Reset();
		m_activeActor = UserActor();
		m_activeWorld = CurrentWorld();
	}

	if (!m_active) {
		return;
	}

	if (!m_freeCursor) {
		m_look.Turn(StickRate(m_stickX) * c_stickYawDegreesPerSecond * deltaTime);
		// Stick up is negative.
		m_look.Tilt(-StickRate(m_stickY) * c_stickPitchRadiansPerSecond * deltaTime);
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
	if (!m_active || yaw == 0.0f) {
		return;
	}

	p_nav->m_additionalRotationY += yaw;
	p_nav->m_keyPressed = TRUE;
}

void Controller::HandleSDLEvent(SDL_Event* p_event)
{
	switch (p_event->type) {
	case SDL_EVENT_GAMEPAD_AXIS_MOTION:
		if (p_event->gaxis.axis == SDL_GAMEPAD_AXIS_RIGHTX) {
			m_stickX = p_event->gaxis.value;
		}
		else if (p_event->gaxis.axis == SDL_GAMEPAD_AXIS_RIGHTY) {
			m_stickY = p_event->gaxis.value;
		}
		break;

	case SDL_EVENT_GAMEPAD_REMOVED:
		m_stickX = m_stickY = 0;
		break;

	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP:
		if (p_event->button.which == SDL_TOUCH_MOUSEID || p_event->button.button != SDL_BUTTON_RIGHT) {
			break;
		}
		if (!p_event->button.down) {
			ReleaseMouse();
		}
		else if (m_active) {
			SDL_Window* window = SDL_GetWindowFromID(p_event->button.windowID);
			if (window) {
				SDL_GetMouseState(&m_savedMouseX, &m_savedMouseY);
				SDL_SetWindowRelativeMouseMode(window, true);
				m_relativeWindow = p_event->button.windowID;
			}
			m_rightButtonHeld = true;
		}
		break;

	case SDL_EVENT_MOUSE_MOTION:
		if (m_rightButtonHeld && m_active && p_event->motion.which != SDL_TOUCH_MOUSEID) {
			m_look.Turn(p_event->motion.xrel * c_mouseYawDegreesPerPixel);
			m_look.Tilt(-p_event->motion.yrel * c_mousePitchRadiansPerPixel);
		}
		break;
	}
}

bool Controller::HandleTouch(SDL_Event* p_event, bool p_stickScheme)
{
	const SDL_TouchFingerEvent& finger = p_event->tfinger;

	switch (p_event->type) {
	case SDL_EVENT_FINGER_DOWN:
		if (!m_active || !p_stickScheme || m_fingerHeld || finger.x < c_touchLookZoneX) {
			return false;
		}
		m_fingerHeld = true;
		m_finger = finger.fingerID;
		m_fingerX = finger.x;
		m_fingerY = finger.y;
		return true;

	case SDL_EVENT_FINGER_MOTION:
		if (!m_fingerHeld || finger.fingerID != m_finger) {
			return false;
		}
		if (m_active) {
			m_look.Turn((finger.x - m_fingerX) * c_touchYawDegreesPerUnit);
			m_look.Tilt(-(finger.y - m_fingerY) * c_touchPitchRadiansPerUnit);
		}
		m_fingerX = finger.x;
		m_fingerY = finger.y;
		return true;

	case SDL_EVENT_FINGER_UP:
	case SDL_EVENT_FINGER_CANCELED:
		if (!m_fingerHeld || finger.fingerID != m_finger) {
			return false;
		}
		m_fingerHeld = false;
		return true;
	}

	return false;
}

void Controller::CancelInput()
{
	m_stickX = m_stickY = 0;
	m_fingerHeld = false;
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
	m_rightButtonHeld = false;
	if (!m_relativeWindow) {
		return;
	}

	SDL_Window* window = SDL_GetWindowFromID(m_relativeWindow);
	m_relativeWindow = 0;
	if (window && SDL_GetWindowRelativeMouseMode(window)) {
		SDL_SetWindowRelativeMouseMode(window, false);
		SDL_WarpMouseInWindow(window, m_savedMouseX, m_savedMouseY);
	}
}
