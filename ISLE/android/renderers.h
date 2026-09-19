#ifndef ANDROID_RENDERERS_H
#define ANDROID_RENDERERS_H

#include "miniwin/d3drm.h"
#include "miniwin/miniwindevice.h"

#include <string>
#include <vector>

// The device id the game stores in its configuration file, in the form
// LegoDeviceEnumerate::FormatDeviceName writes and ParseDeviceName reads.
std::string Android_FormatDeviceId(const GUID& p_guid);

// Appends the candidates that p_renderers, a flat list of label and id pairs, does not already
// name. Enumerating devices can only report the ones that suit the window the game is already
// using, so without this the renderer a player would have to choose in order to get a different
// window is exactly the one the list cannot contain.
void Android_AddMissingRenderers(
	std::vector<std::string>& p_renderers,
	const MiniwinDeviceCandidate* p_candidates,
	int p_count
);

#endif
