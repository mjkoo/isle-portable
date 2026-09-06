#ifndef ANDROID_ACTIVITY_H
#define ANDROID_ACTIVITY_H

#include <jni.h>

// The JNI glue and the event housekeeping every Android prompt in this port needs. Shared so
// that the list of event ranges below and the exception handling above it cannot fork between
// the callers.

struct Android_ActivityCall {
	JNIEnv* m_env;
	jobject m_activity;
	jclass m_class;
	jmethodID m_method;
};

// Resolves a method on the SDL activity. On success the caller must pass p_call to
// Android_EndActivityCall.
bool Android_BeginActivityCall(Android_ActivityCall* p_call, const char* p_name, const char* p_signature);

// Returns true when the call completed without a pending Java exception.
bool Android_EndActivityCall(Android_ActivityCall* p_call);

// Calls a no-argument boolean method on the activity, false if it threw or could not be found.
bool Android_CallActivityBooleanMethod(const char* p_name);

// Pumps the event queue and drops the input queued behind a prompt. Callers that sit in a
// poll loop must call this rather than SDL_PumpEvents: it is what keeps the Android UI thread
// running, and it is what stops a prompt's worth of touches arriving in one burst afterwards.
void Android_DrainInputEvents();

#endif // ANDROID_ACTIVITY_H
