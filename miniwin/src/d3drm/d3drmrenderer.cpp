#include "d3drmrenderer.h"

#include "deviceid.h"
#include "miniwin.h"
#ifdef USE_OPENGL1
#include "d3drmrenderer_opengl1.h"
#endif
#ifdef USE_OPENGLES2
#include "d3drmrenderer_opengles2.h"
#endif
#ifdef USE_OPENGLES3
#include "d3drmrenderer_opengles3.h"
#endif
#ifdef USE_CITRO3D
#include "d3drmrenderer_citro3d.h"
#endif
#ifdef USE_DIRECTX9
#include "d3drmrenderer_directx9.h"
#endif
#ifdef USE_SDL_GPU
#include "d3drmrenderer_sdl3gpu.h"
#endif
#ifdef USE_SOFTWARE_RENDER
#include "d3drmrenderer_software.h"
#endif
#ifdef USE_PALETTE_SW_RENDER
#include "d3drmrenderer_palettesw.h"
#endif
#ifdef USE_GXM
#include "d3drmrenderer_gxm.h"
#endif
#ifdef USE_GLIDE
#include "d3drmrenderer_glide.h"
#endif

// The names must match the ones the matching *_EnumDevice passes to EnumDevice, so that a
// device offered before the window exists is labelled the way the enumeration labels it.
//
// This is the compile-time list, not a probe: nothing here asks whether a device would start,
// because the enumeration settles that once the window exists, and probing costs every player a
// device creation for a list only a settings screen reads.
int Miniwin_GetDeviceCandidates(MiniwinDeviceCandidate* out, int maxCount)
{
	int count = 0;
	int total = 0;
	auto add = [&](const char* name, const GUID& guid) {
		total++;
		if (count < maxCount) {
			out[count].m_name = name;
			out[count].m_guid = guid;
			count++;
		}
	};
#ifdef USE_SDL_GPU
	add("SDL3 GPU HAL", SDL3_GPU_GUID);
#endif
#ifdef USE_OPENGLES3
	add("OpenGL ES 3.0 HAL", OpenGLES3_GUID);
#endif
#ifdef USE_OPENGLES2
	add("OpenGL ES 2.0 HAL", OpenGLES2_GUID);
#endif
#ifdef USE_OPENGL1
	add("OpenGL 1.1 HAL", OpenGL1_GUID);
#endif
#ifdef USE_CITRO3D
	add("Citro3D", Citro3D_GUID);
#endif
#ifdef USE_DIRECTX9
	add("DirectX 9 HAL", DirectX9_GUID);
#endif
#ifdef USE_PALETTE_SW_RENDER
	add("Miniwin Paletted Software", PALETTE_SW_GUID);
#endif
#ifdef USE_SOFTWARE_RENDER
	add("Miniwin Emulation", SOFTWARE_GUID);
#endif
#ifdef USE_GLIDE
	add("3dfx Glide", GLIDE_GUID);
#endif
#ifdef USE_GXM
	add("GXM HAL", GXM_GUID);
#endif
	if (total > count) {
		SDL_LogWarn(LOG_CATEGORY_MINIWIN, "%d of %d devices did not fit and cannot be offered", total - count, total);
	}
	return count;
}

void Miniwin_SetupWindowCreateProperties(SDL_PropertiesID props, const char* deviceId)
{
#if defined(USE_SDL_GPU) && defined(SDL_PLATFORM_ANDROID)
	// [library:3d]
	// Android binds an EGLSurface to the window's ANativeWindow as soon as it is created for
	// OpenGL, and vkCreateAndroidSurfaceKHR then refuses that window for the rest of its life -
	// see Direct3DRMSDL3GPU_EnumDevice, which declines to advertise a device it could not
	// render into. So a player who has asked for the GPU backend gets a window it can claim,
	// and everyone else keeps the OpenGL window the OpenGL backends need. Asking whether a GPU
	// device exists at all matters here: without one, dropping the flag would leave no
	// hardware device advertised at all.
	if (Miniwin_DeviceIdNamesGuid(deviceId, SDL3_GPU_GUID) && Direct3DRMSDL3GPU_IsAvailable()) {
		return;
	}
#else
	(void) deviceId;
#endif

#if (defined(USE_OPENGL1) || defined(USE_OPENGLES2) || defined(USE_OPENGLES3)) && !defined(__3DS__) &&                 \
	!defined(WINDOWS_STORE) && !defined(__vita__) && !defined(__DJGPP__)
	SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
#endif
}

Direct3DRMRenderer* CreateDirect3DRMRenderer(
	const IDirect3DMiniwin* d3d,
	const DDSURFACEDESC& DDSDesc,
	const GUID* guid
)
{
#ifdef USE_SDL_GPU
	if (SDL_memcmp(guid, &SDL3_GPU_GUID, sizeof(GUID)) == 0) {
		return Direct3DRMSDL3GPURenderer::Create(DDSDesc.dwWidth, DDSDesc.dwHeight);
	}
#endif
#ifdef USE_SOFTWARE_RENDER
	if (SDL_memcmp(guid, &SOFTWARE_GUID, sizeof(GUID)) == 0) {
		return new Direct3DRMSoftwareRenderer(DDSDesc.dwWidth, DDSDesc.dwHeight);
	}
#endif
#ifdef USE_OPENGLES3
	if (SDL_memcmp(guid, &OpenGLES3_GUID, sizeof(GUID)) == 0) {
		return OpenGLES3Renderer::Create(
			DDSDesc.dwWidth,
			DDSDesc.dwHeight,
			d3d->GetMSAASamples(),
			d3d->GetAnisotropic(),
			d3d->GetLightingModel()
		);
	}
#endif
#ifdef USE_OPENGLES2
	if (SDL_memcmp(guid, &OpenGLES2_GUID, sizeof(GUID)) == 0) {
		return OpenGLES2Renderer::Create(
			DDSDesc.dwWidth,
			DDSDesc.dwHeight,
			d3d->GetAnisotropic(),
			d3d->GetLightingModel()
		);
	}
#endif
#ifdef USE_OPENGL1
	if (SDL_memcmp(guid, &OpenGL1_GUID, sizeof(GUID)) == 0) {
		return OpenGL1Renderer::Create(DDSDesc.dwWidth, DDSDesc.dwHeight, d3d->GetMSAASamples());
	}
#endif
#ifdef USE_CITRO3D
	if (SDL_memcmp(guid, &Citro3D_GUID, sizeof(GUID)) == 0) {
		return new Citro3DRenderer(DDSDesc.dwWidth, DDSDesc.dwHeight);
	}
#endif
#ifdef USE_DIRECTX9
	if (SDL_memcmp(guid, &DirectX9_GUID, sizeof(GUID)) == 0) {
		return DirectX9Renderer::Create(DDSDesc.dwWidth, DDSDesc.dwHeight);
	}
#endif
#ifdef USE_GXM
	if (SDL_memcmp(guid, &GXM_GUID, sizeof(GUID)) == 0) {
		return GXMRenderer::Create(DDSDesc.dwWidth, DDSDesc.dwHeight, d3d->GetMSAASamples());
	}
#endif
#ifdef USE_GLIDE
	if (SDL_memcmp(guid, &GLIDE_GUID, sizeof(GUID)) == 0) {
		return new Direct3DRMGlideRenderer(DDSDesc.dwWidth, DDSDesc.dwHeight);
	}
#endif
#ifdef USE_PALETTE_SW_RENDER
	if (SDL_memcmp(guid, &PALETTE_SW_GUID, sizeof(GUID)) == 0) {
		return new Direct3DRMPaletteSWRenderer(DDSDesc.dwWidth, DDSDesc.dwHeight);
	}
#endif
	return nullptr;
}

void Direct3DRMRenderer_EnumDevices(const IDirect3DMiniwin* d3d, LPD3DENUMDEVICESCALLBACK cb, void* ctx)
{
#ifdef USE_SDL_GPU
	Direct3DRMSDL3GPU_EnumDevice(cb, ctx);
#endif
#ifdef USE_OPENGLES3
	OpenGLES3Renderer_EnumDevice(d3d, cb, ctx);
#endif
#ifdef USE_OPENGLES2
	OpenGLES2Renderer_EnumDevice(d3d, cb, ctx);
#endif
#ifdef USE_OPENGL1
	OpenGL1Renderer_EnumDevice(d3d, cb, ctx);
#endif
#ifdef USE_CITRO3D
	Citro3DRenderer_EnumDevice(cb, ctx);
#endif
#ifdef USE_DIRECTX9
	DirectX9Renderer_EnumDevice(cb, ctx);
#endif
#ifdef USE_PALETTE_SW_RENDER
	Direct3DRMPaletteSW_EnumDevice(cb, ctx);
#endif
#ifdef USE_SOFTWARE_RENDER
	Direct3DRMSoftware_EnumDevice(cb, ctx);
#endif
#ifdef USE_GLIDE
	Direct3DRMGlide_EnumDevice(cb, ctx);
#endif
#ifdef USE_GXM
	GXMRenderer_EnumDevice(cb, ctx);
#endif
}
