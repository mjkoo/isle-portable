#include "extensions/textureloader.h"

#include "extensions/common/pathutils.h"
#include "legovideomanager.h"
#include "misc.h"
#include "misc/legotexture.h"
#include "mxdirectx/mxdirect3d.h"
#include "mxmain.h"
#include "tgl/d3drm/impl.h"

using namespace Extensions;

std::map<std::string, std::string> TextureLoaderExt::options;
std::vector<std::string> TextureLoaderExt::excludedFiles;
bool TextureLoaderExt::enabled = false;

void TextureLoaderExt::Initialize()
{
	for (const auto& option : defaults) {
		if (!options.count(option.first.data())) {
			options[option.first.data()] = option.second;
		}
	}
}

void TextureLoaderExt::AddExcludedFile(const std::string& p_file)
{
	excludedFiles.emplace_back(p_file);
}

bool TextureLoaderExt::PatchTexture(LegoTextureInfo* p_textureInfo, LegoTexture* p_texture)
{
	SDL_Surface* surface = FindTexture(p_textureInfo->m_name);
	if (!surface) {
		return false;
	}

	const SDL_PixelFormatDetails* details = SDL_GetPixelFormatDetails(surface->format);

	DDSURFACEDESC desc;
	memset(&desc, 0, sizeof(desc));
	desc.dwSize = sizeof(desc);
	desc.dwFlags = DDSD_PIXELFORMAT | DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS;
	desc.ddsCaps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_SYSTEMMEMORY;
	desc.ddpfPixelFormat.dwSize = sizeof(desc.ddpfPixelFormat);
	desc.dwWidth = surface->w;
	desc.dwHeight = surface->h;
	desc.ddpfPixelFormat.dwFlags = DDPF_RGB | DDPF_ALPHAPIXELS;
	desc.ddpfPixelFormat.dwRGBBitCount = details->bits_per_pixel;
	desc.ddpfPixelFormat.dwRBitMask = details->Rmask;
	desc.ddpfPixelFormat.dwGBitMask = details->Gmask;
	desc.ddpfPixelFormat.dwBBitMask = details->Bmask;
	desc.ddpfPixelFormat.dwRGBAlphaBitMask = details->Amask;

	LPDIRECTDRAW pDirectDraw = VideoManager()->GetDirect3D()->DirectDraw();
	if (pDirectDraw->CreateSurface(&desc, &p_textureInfo->m_surface, nullptr) != DD_OK) {
		SDL_DestroySurface(surface);
		return false;
	}

	memset(&desc, 0, sizeof(desc));
	desc.dwSize = sizeof(desc);

	// LegoTextureInfo::Create reads a false return as "the extension did nothing" and then builds a
	// surface and a palette of its own straight over these fields, releasing neither, so everything
	// taken here has to be handed back before giving up. Releasing the palette is not enough on its
	// own: SetPalette gives the surface a second reference to it, so the surface has to go too.
	auto fail = [&](bool p_locked) {
		if (p_locked) {
			p_textureInfo->m_surface->Unlock(desc.lpSurface);
		}
		if (p_textureInfo->m_palette) {
			p_textureInfo->m_palette->Release();
			p_textureInfo->m_palette = nullptr;
		}
		if (p_textureInfo->m_surface) {
			p_textureInfo->m_surface->Release();
			p_textureInfo->m_surface = nullptr;
		}
		SDL_DestroySurface(surface);
		return false;
	};

	if (p_textureInfo->m_surface->Lock(nullptr, &desc, DDLOCK_SURFACEMEMORYPTR | DDLOCK_WRITEONLY, nullptr) != DD_OK) {
		return fail(false);
	}

	MxU8* dst = (MxU8*) desc.lpSurface;
	Uint8* srcPixels = (Uint8*) surface->pixels;

	// LegoTextureInfo::Create returns as soon as this succeeds, so everything it would otherwise
	// guarantee has to be set up here. That includes m_palette: LegoTextureContainer::GetCached
	// copies a head texture for phoneme animation and references the palette without checking it,
	// so a texture without one crashes the game as soon as that character speaks. An 8-bit
	// replacement brings its own colours; anything else borrows the palette of the texture it
	// replaces, which is what the cached 8-bit copy is built against.
	SDL_Palette* sdlPalette = details->bits_per_pixel == 8 ? SDL_GetSurfacePalette(surface) : nullptr;
	if (!sdlPalette && p_texture && p_texture->GetImage()) {
		sdlPalette = p_texture->GetImage()->GetPalette();
	}
	if (!sdlPalette) {
		return fail(true);
	}

	PALETTEENTRY entries[256];
	memset(entries, 0, sizeof(entries));
	for (int i = 0; i < 256; ++i) {
		if (i < sdlPalette->ncolors) {
			entries[i].peRed = sdlPalette->colors[i].r;
			entries[i].peGreen = sdlPalette->colors[i].g;
			entries[i].peBlue = sdlPalette->colors[i].b;
			entries[i].peFlags = PC_NONE;
		}
		else {
			entries[i].peFlags = D3DPAL_RESERVED;
		}
	}

	LPDIRECTDRAWPALETTE ddPalette = nullptr;
	if (pDirectDraw->CreatePalette(DDPCAPS_8BIT | DDPCAPS_ALLOW256, entries, &ddPalette, nullptr) != DD_OK) {
		return fail(true);
	}

	if (details->bits_per_pixel == 8) {
		p_textureInfo->m_surface->SetPalette(ddPalette);
	}
	p_textureInfo->m_palette = ddPalette;

	memcpy(dst, srcPixels, surface->pitch * surface->h);
	p_textureInfo->m_surface->Unlock(desc.lpSurface);

	if (((TglImpl::RendererImpl*) VideoManager()->GetRenderer())
			->CreateTextureFromSurface(p_textureInfo->m_surface, &p_textureInfo->m_texture) != D3DRM_OK) {
		return fail(false);
	}

	p_textureInfo->m_texture->SetAppData((LPD3DRM_APPDATA) p_textureInfo);
	SDL_DestroySurface(surface);
	return true;
}

SDL_Surface* TextureLoaderExt::FindTexture(const char* p_name)
{
	if (std::find(excludedFiles.begin(), excludedFiles.end(), p_name) != excludedFiles.end()) {
		return nullptr;
	}

	const char* texturePath = options["texture loader:texture path"].c_str();
	MxString relativePath = MxString(texturePath) + "/" + p_name + ".bmp";

	MxString path;
	if (!Common::ResolveGamePath(relativePath.GetData(), path)) {
		return nullptr;
	}

	return SDL_LoadBMP(path.GetData());
}
