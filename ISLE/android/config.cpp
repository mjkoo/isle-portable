#include "config.h"

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_stdinc.h>
#include <iniparser.h>

void Android_SetupDefaultConfigOverrides(dictionary* p_dictionary, const char* p_dataPath)
{
	SDL_Log("Overriding default config for Android");

	iniparser_set(p_dictionary, "isle:diskpath", p_dataPath);
	iniparser_set(p_dictionary, "isle:cdpath", p_dataPath);
	iniparser_set(p_dictionary, "isle:mediapath", p_dataPath);
	// Resolve the default save directory on each launch, including after a backup is restored
	// on a device whose internal storage path differs. Explicit savepath overrides still work.
	iniparser_unset(p_dictionary, "isle:savepath");

	// Default to Virtual Mouse
	char buf[16];
	iniparser_set(p_dictionary, "isle:Touch Scheme", SDL_itoa(0, buf, 10));
}
