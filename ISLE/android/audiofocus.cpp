#include "audiofocus.h"

#include <SDL3/SDL_log.h>
#include <jni.h>

static AudioFocus g_audioFocus;

void Android_ReportAudioFocus(int p_change)
{
	if (!g_audioFocus.Report(p_change)) {
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Ignoring unknown audio focus change %d", p_change);
	}
}

bool Android_TakeAudioGain(float* p_gain)
{
	return g_audioFocus.Take(p_gain);
}

extern "C" JNIEXPORT void JNICALL Java_org_legoisland_isle_AudioFocus_reportNativeChange(JNIEnv*, jclass, jint p_change)
{
	Android_ReportAudioFocus(p_change);
}
