#include "audiofocus.h"

#include "legomain.h"
#include "legosoundmanager.h"
#include "misc.h"

#include <SDL3/SDL_log.h>
#include <jni.h>

static AudioFocus g_audioFocus;

void Android_ReportAudioFocus(int p_androidChange)
{
	if (!g_audioFocus.Report(p_androidChange)) {
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Ignoring audio focus change %d", p_androidChange);
	}
}

void Android_ApplyAudioGain()
{
	// The sound manager is checked before the gain is taken, so a focus change that arrives
	// before the game has one stays pending instead of being consumed and dropped.
	if (!Lego() || !Lego()->GetSoundManager()) {
		return;
	}

	float gain;
	if (g_audioFocus.Take(&gain)) {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Audio focus changed, playing at gain %.2f", gain);
		Lego()->GetSoundManager()->SetOutputGain(gain);
	}
}

extern "C" JNIEXPORT void JNICALL
Java_org_legoisland_isle_AudioFocus_reportNativeChange(JNIEnv*, jclass, jint p_androidChange)
{
	Android_ReportAudioFocus(p_androidChange);
}
