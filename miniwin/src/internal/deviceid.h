#pragma once

#include "miniwin/windows.h"

#include <cstdio>
#include <cstring>

// Whether a configured device id names a particular device. The id is
// "<driver> 0x<w> 0x<x> 0x<y> 0x<z>", the four words being the device GUID, in the form
// LegoDeviceEnumerate::FormatDeviceName writes it and ParseDeviceName reads it back. It comes
// straight from the configuration file and nothing has validated it.
//
// This has to agree with LegoDeviceEnumerate about which ids resolve to which device, because
// the window is created for whatever this says and the game then resolves the same string on
// its own: an id accepted here and rejected there gives a window built for a device the game
// does not select. So it rejects what ParseDeviceName rejects, and accepts the trailing rubbish
// ParseDeviceName's sscanf accepts.
//
// Standard library rather than SDL's wrappers, so that the header stands alone and a host test
// can include it without linking SDL.
inline bool Miniwin_DeviceIdNamesGuid(const char* deviceId, const GUID& guid)
{
	if (!deviceId) {
		return false;
	}

	int driver = -1;
	unsigned int words[4];
	if (std::sscanf(deviceId, "%d 0x%x 0x%x 0x%x 0x%x", &driver, &words[0], &words[1], &words[2], &words[3]) != 5) {
		return false;
	}

	// ParseDeviceName rejects a negative ordinal outright, and ProcessDeviceBytes only matches a
	// device whose driver ordinal is the one it was given. Miniwin reports one DirectDraw driver,
	// so every id the game can resolve names the first.
	if (driver != 0) {
		return false;
	}

	GUID parsed;
	static_assert(sizeof(words) == sizeof(GUID), "Equal size");
	std::memcpy(&parsed, words, sizeof(GUID));
	return std::memcmp(&parsed, &guid, sizeof(GUID)) == 0;
}
