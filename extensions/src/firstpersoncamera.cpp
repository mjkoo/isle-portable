#include "extensions/firstpersoncamera.h"

#include "extensions/firstpersoncamera/controller.h"
#include "extensions/thirdpersoncamera.h"
#include "mxcore.h"
#include "mxmisc.h"
#include "mxticklemanager.h"

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_stdinc.h>

using namespace Extensions;

std::map<std::string, std::string> FirstPersonCameraExt::options;
bool FirstPersonCameraExt::enabled = false;
FirstPersonCamera::Controller* FirstPersonCameraExt::s_controller = nullptr;
bool FirstPersonCameraExt::s_registered = false;

namespace
{

class TickleAdapter : public MxCore {
public:
	TickleAdapter(FirstPersonCamera::Controller* p_controller) : m_controller(p_controller) {}

	MxResult Tickle() override
	{
		m_controller->Tick();
		return SUCCESS;
	}

	const char* ClassName() const override { return "FirstPersonCamera::TickleAdapter"; }

private:
	FirstPersonCamera::Controller* m_controller;
};

} // namespace

void FirstPersonCameraExt::Initialize()
{
	if (!s_controller) {
		s_controller = new FirstPersonCamera::Controller();
	}

	auto invert = options.find("first person camera:invert look");
	// Read as iniparser reads a boolean.
	s_controller->SetInverted(
		invert != options.end() && !invert->second.empty() && SDL_strchr("yYtT1", invert->second.c_str()[0])
	);
}

void FirstPersonCameraExt::HandleCreate()
{
	// Every extension has been read by now, including multiplayer, which turns third person on.
	if (ThirdPersonCameraExt::enabled) {
		SDL_Log("First person camera is off: third person has the camera");
		enabled = false;
		return;
	}

	if (!s_registered && s_controller) {
		TickleManager()->RegisterClient(new TickleAdapter(s_controller), 10);
		s_registered = true;
	}
}

MxBool FirstPersonCameraExt::HandleNavOverride(
	LegoNavController* p_nav,
	const Vector3& p_curPos,
	const Vector3& p_curDir,
	Vector3& p_newPos,
	Vector3& p_newDir,
	float p_deltaTime
)
{
	if (s_controller) {
		s_controller->TurnActor(p_nav);
	}

	// The game still moves the actor itself.
	return FALSE;
}

MxBool FirstPersonCameraExt::HandleTouchInput(SDL_Event* p_event, MxBool p_stickScheme)
{
	return s_controller && s_controller->HandleTouch(p_event, p_stickScheme);
}

void FirstPersonCameraExt::CancelPointerInput()
{
	if (s_controller) {
		s_controller->CancelInput();
	}
}

void FirstPersonCameraExt::OnSDLEvent(SDL_Event* p_event)
{
	if (s_controller) {
		s_controller->HandleSDLEvent(p_event);
	}
}

void FirstPersonCameraExt::HandleSDLEvent(SDL_Event* p_event)
{
	Extension<FirstPersonCameraExt>::Call(FP::HandleSDLEvent, p_event);
}

bool FirstPersonCameraExt::IsLooking()
{
	return enabled && s_controller && s_controller->IsLooking();
}

void FirstPersonCameraExt::SetFreeCursor(bool p_held)
{
	if (enabled && s_controller) {
		s_controller->SetFreeCursor(p_held);
	}
}

void FirstPersonCameraExt::Recenter()
{
	if (enabled && s_controller) {
		s_controller->Recenter();
	}
}
