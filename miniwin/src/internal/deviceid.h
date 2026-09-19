#pragma once

#include "miniwin/windows.h"

#include <cstddef>
#include <cstdio>
#include <cstring>

// The device id the game keeps in its configuration file: "<driver> 0x<w> 0x<x> 0x<y> 0x<z>",
// the four words being the device GUID, in the form LegoDeviceEnumerate::FormatDeviceName
// writes it and ParseDeviceName reads it back.
//
// Both halves live here because they have to agree with each other and with the game. The
// window is created for whatever the parse says, and the game then resolves the same string on
// its own: an id accepted here and rejected there gives a window built for a device the game
// does not select.
//
// Standard library rather than SDL's wrappers, so that the header stands alone and a host test
// can include it without linking SDL.

// Writes the id that names a device. The four words are the GUID's bytes, the way GUID4 reads
// them back. Copied rather than taken field by field: m_data1, m_data2 and m_data3 would print
// a different string, and ParseDeviceName would not match it.
//
// The leading zero is the DirectDraw driver ordinal, and miniwin reports exactly one driver, so
// it is always the first. There is no second chance if it is ever wrong - ProcessDeviceBytes
// only matches a device whose driver ordinal is the one it was given.
inline void Miniwin_FormatDeviceId(const GUID& guid, char* out, size_t size)
{
	unsigned int words[4];
	static_assert(sizeof(words) == sizeof(GUID), "Equal size");
	std::memcpy(words, &guid, sizeof(words));

	std::snprintf(out, size, "0 0x%x 0x%x 0x%x 0x%x", words[0], words[1], words[2], words[3]);
}

// Whether a configured device id names a particular device. It comes straight from the
// configuration file and nothing has validated it, so it rejects what ParseDeviceName rejects,
// and accepts the trailing rubbish ParseDeviceName's sscanf accepts.
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
