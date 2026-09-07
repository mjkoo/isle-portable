#include "activity.h"

#include <SDL3/SDL.h>

// A prompt sits over the surface for as long as the user takes, and Android keeps feeding
// touches through the whole time. Pump so the drivers make progress, then drop the input that
// was just queued: the game is not reading it, and it would otherwise receive the entire burst
// at once the moment the prompt closes.
//
// Only what a person can press or drag is flushed, and deliberately not by whole subsystem
// range: SDL groups the hotplug events inside those ranges, and an ADDED event is the only
// notification the game ever gets, since the event handler in isleapp.cpp is the sole caller
// of LegoInputManager::AddJoystick and AddMouse. SDL_Init queues one for every device already
// connected at launch, so flushing SDL_EVENT_GAMEPAD_FIRST through _LAST would leave a
// controller paired before the import dead for the rest of the session.
//
// Lifecycle events are unaffected either way, since SDL hands those straight to the event
// watchers rather than queueing them (see the comment on SDL_AddEventWatch in isleapp.cpp),
// and leaving the 0x100 and 0x200 ranges alone keeps the SDL_EVENT_QUIT that Android_OnDestroy
// queues on its way to SDL_AppEvent.
static const struct {
	SDL_EventType m_first;
	SDL_EventType m_last;
} g_inputRanges[] = {
	{SDL_EVENT_KEY_DOWN, SDL_EVENT_TEXT_INPUT},
	{SDL_EVENT_MOUSE_MOTION, SDL_EVENT_MOUSE_WHEEL},
	{SDL_EVENT_JOYSTICK_AXIS_MOTION, SDL_EVENT_JOYSTICK_BUTTON_UP},
	{SDL_EVENT_GAMEPAD_AXIS_MOTION, SDL_EVENT_GAMEPAD_BUTTON_UP},
	{SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN, SDL_EVENT_GAMEPAD_CAPSENSE_RELEASE},
	{SDL_EVENT_FINGER_FIRST, SDL_EVENT_FINGER_LAST},
	{SDL_EVENT_PINCH_FIRST, SDL_EVENT_PINCH_LAST},
};

bool Android_IsInputEvent(Uint32 p_type)
{
	for (const auto& range : g_inputRanges) {
		if (p_type >= range.m_first && p_type <= range.m_last) {
			return true;
		}
	}
	return false;
}

void Android_DrainInputEvents()
{
	SDL_PumpEvents();

	for (const auto& range : g_inputRanges) {
		SDL_FlushEvents(range.m_first, range.m_last);
	}
}

bool Android_BeginActivityCall(Android_ActivityCall* p_call, const char* p_name, const char* p_signature)
{
	p_call->m_env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
	p_call->m_activity = static_cast<jobject>(SDL_GetAndroidActivity());
	p_call->m_class = NULL;
	p_call->m_method = NULL;

	if (!p_call->m_env || !p_call->m_activity) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "No JNI environment for IsleActivity.%s", p_name);
		return false;
	}

	p_call->m_class = p_call->m_env->GetObjectClass(p_call->m_activity);
	p_call->m_method = p_call->m_env->GetMethodID(p_call->m_class, p_name, p_signature);

	if (!p_call->m_method) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "IsleActivity.%s not found", p_name);
		p_call->m_env->ExceptionClear();
		p_call->m_env->DeleteLocalRef(p_call->m_class);
		p_call->m_env->DeleteLocalRef(p_call->m_activity);
		return false;
	}

	return true;
}

bool Android_EndActivityCall(Android_ActivityCall* p_call)
{
	bool threw = p_call->m_env->ExceptionCheck();
	if (threw) {
		p_call->m_env->ExceptionDescribe();
		p_call->m_env->ExceptionClear();
	}

	p_call->m_env->DeleteLocalRef(p_call->m_class);
	p_call->m_env->DeleteLocalRef(p_call->m_activity);
	return !threw;
}

bool Android_CallActivityBooleanMethod(const char* p_name)
{
	Android_ActivityCall call;
	if (!Android_BeginActivityCall(&call, p_name, "()Z")) {
		return false;
	}

	jboolean result = call.m_env->CallBooleanMethod(call.m_activity, call.m_method);
	return Android_EndActivityCall(&call) && result;
}
