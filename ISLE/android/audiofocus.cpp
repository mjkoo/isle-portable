#include "audiofocus.h"

#include <SDL3/SDL_log.h>
#include <jni.h>

static AudioFocus g_audioFocus;

void Android_ReportAudioFocus(int p_androidChange)
{
	if (!g_audioFocus.Report(p_androidChange)) {
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Ignoring audio focus change %d", p_androidChange);
	}
}

bool Android_TakeAudioFocusGain(float* p_gain)
{
	return g_audioFocus.Take(p_gain);
}

extern "C" JNIEXPORT void JNICALL
Java_org_legoisland_isle_AudioFocus_reportNativeChange(JNIEnv*, jclass, jint p_androidChange)
{
	Android_ReportAudioFocus(p_androidChange);
}
