#pragma once

#include "miniwin/windows.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_properties.h>

DEFINE_GUID(IID_IDirect3DRMMiniwinDevice, 0x6eb09673, 0x8d30, 0x4d8a, 0x8d, 0x81, 0x34, 0xea, 0x69, 0x30, 0x12, 0x01);

struct IDirect3DRMMiniwinDevice : virtual public IUnknown {
	virtual bool ConvertEventToRenderCoordinates(SDL_Event* event) = 0;
	virtual bool ConvertRenderToWindowCoordinates(Sint32 inX, Sint32 inY, Sint32& outX, Sint32& outY) = 0;
};

// A device this build compiled in. Enumerating devices needs the window that is being chosen
// for, so this answers the question the other way round: what a settings screen may offer before
// any window exists. Whether a given one will start is settled by the enumeration, once there is
// a window. The name matches the one the enumeration reports for the same device.
struct MiniwinDeviceCandidate {
	const char* m_name;
	GUID m_guid;
	// The configured value that selects this device, as LegoDeviceEnumerate::FormatDeviceName
	// would write it. Sized like the buffer that function is called with.
	char m_id[128];
};

// Fills at most maxCount entries and returns how many it wrote, so a caller needs no arithmetic
// of its own. Anything that did not fit is logged, since a device missing from a settings screen
// is a device a player cannot choose.
int Miniwin_GetDeviceCandidates(MiniwinDeviceCandidate* out, int maxCount);

// deviceId is the configured device id, in the form the game writes it, or NULL for no
// preference. The window has to be created for the device that will render into it, because
// on Android a window created for OpenGL cannot be handed to the GPU backend afterwards.
void Miniwin_SetupWindowCreateProperties(SDL_PropertiesID props, const char* deviceId);

// Requested content size; the render target also includes the window's letterboxing.
#define MINIWIN_PROP_RENDER_WIDTH "miniwin.render.width"
#define MINIWIN_PROP_RENDER_HEIGHT "miniwin.render.height"
