#include "quitprompt.h"

#include "activity.h"

#include <SDL3/SDL.h>

// Mirrors QuitPrompt's STATUS_ constants; keep the numbering in step.
enum QuitPromptStatus {
	e_quitPromptPending = -1,
	e_quitPromptResume = 0,
	e_quitPromptQuit = 1,
};

static bool ShowQuitPrompt()
{
	Android_ActivityCall call;
	if (!Android_BeginActivityCall(&call, "showQuitPrompt", "()V")) {
		return false;
	}

	call.m_env->CallVoidMethod(call.m_activity, call.m_method);
	return Android_EndActivityCall(&call);
}

static QuitPromptStatus GetQuitPromptStatus()
{
	Android_ActivityCall call;
	if (!Android_BeginActivityCall(&call, "getQuitPromptStatus", "()I")) {
		return e_quitPromptResume;
	}

	jint status = call.m_env->CallIntMethod(call.m_activity, call.m_method);
	if (!Android_EndActivityCall(&call)) {
		return e_quitPromptResume;
	}

	return static_cast<QuitPromptStatus>(status);
}

bool Android_ConfirmQuit()
{
	if (!ShowQuitPrompt()) {
		// No prompt means no answer, and quitting a game the player did not agree to quit is
		// the worse of the two failures.
		return false;
	}

	for (;;) {
		// Not just SDL_PumpEvents: the dialog only runs while this thread pumps, and the
		// input queued behind it must not arrive in one burst afterwards.
		Android_DrainInputEvents();

		QuitPromptStatus status = GetQuitPromptStatus();
		if (status != e_quitPromptPending) {
			return status == e_quitPromptQuit;
		}

		SDL_Delay(100);
	}
}
