#include "renderers.h"

#include "d3drmrenderer_sdl3gpu.h"
#include "deviceid.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// The real GUID, so that changing it fails here rather than silently stopping every stored id
// from resolving. The second is only a stand-in for some other device: the GL backend headers
// cannot be included on a host that has no GLES headers.
static const GUID& kGpu = SDL3_GPU_GUID;
static const GUID kOther = {0x682656F3, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04}};

// LegoDeviceEnumerate::ParseDeviceName, which is what the game will run against whatever this
// formats. Copied rather than linked so the test does not drag the engine in, and kept
// deliberately literal: the point is to catch a formatted id the game cannot read back.
static bool ParseDeviceName(const std::string& p_id, int& p_driver, GUID& p_guid)
{
	int driver = -1;
	unsigned int words[4];
	if (std::sscanf(p_id.c_str(), "%d 0x%x 0x%x 0x%x 0x%x", &driver, &words[0], &words[1], &words[2], &words[3]) != 5) {
		return false;
	}
	if (driver < 0) {
		return false;
	}
	p_driver = driver;
	std::memcpy(&p_guid, words, sizeof(GUID));
	return true;
}

static std::string FormatId(const GUID& p_guid)
{
	char id[128];
	Miniwin_FormatDeviceId(p_guid, id, sizeof(id));
	return id;
}

static MiniwinDeviceCandidate Candidate(const char* p_name, const GUID& p_guid)
{
	MiniwinDeviceCandidate candidate = {p_name, p_guid, {}};
	Miniwin_FormatDeviceId(p_guid, candidate.m_id, sizeof(candidate.m_id));
	return candidate;
}

static void RoundTrips(const GUID& p_guid)
{
	int driver = -1;
	GUID parsed = {};
	assert(ParseDeviceName(FormatId(p_guid), driver, parsed));
	// Miniwin reports one DirectDraw driver, and ProcessDeviceBytes only matches a device whose
	// driver ordinal is the one it was given, so anything but the first driver never matches.
	assert(driver == 0);
	assert(std::memcmp(&parsed, &p_guid, sizeof(GUID)) == 0);

	// The same id, read by the parser that decides what kind of window the game gets. The two
	// have to agree, or the window is created for a device the game does not then select.
	assert(Miniwin_DeviceIdNamesGuid(FormatId(p_guid).c_str(), p_guid));
}

static std::vector<std::string> Merge(
	std::vector<std::string> p_renderers,
	std::vector<MiniwinDeviceCandidate> p_candidates
)
{
	Android_AddMissingRenderers(p_renderers, p_candidates.data(), (int) p_candidates.size());
	return p_renderers;
}

int main()
{
	RoundTrips(kGpu);
	RoundTrips(kOther);

	// Distinct devices must not collapse onto one id, or picking one would select the other.
	assert(FormatId(kGpu) != FormatId(kOther));

	const std::string gpuId = FormatId(kGpu);
	const std::string otherId = FormatId(kOther);

	// Nothing enumerated: every candidate is offered, in the order it was reported.
	std::vector<std::string> fromNothing = Merge({}, {Candidate("SDL3 GPU HAL", kGpu), Candidate("Another HAL", kOther)});
	assert((fromNothing == std::vector<std::string>{"SDL3 GPU HAL", gpuId, "Another HAL", otherId}));

	// A GL window enumerates the GL device and hides the GPU one, which is the case the list
	// has to repair, and it must not repeat what the enumeration already reported.
	std::vector<std::string> fromGl =
		Merge({"Another HAL", otherId}, {Candidate("SDL3 GPU HAL", kGpu), Candidate("Another HAL", kOther)});
	assert((fromGl == std::vector<std::string>{"Another HAL", otherId, "SDL3 GPU HAL", gpuId}));

	// A GPU window is the mirror image: the GL devices cannot initialize, so they are the ones
	// missing, and a player has to be able to choose one to get back.
	std::vector<std::string> fromGpu =
		Merge({"SDL3 GPU HAL", gpuId}, {Candidate("SDL3 GPU HAL", kGpu), Candidate("Another HAL", kOther)});
	assert((fromGpu == std::vector<std::string>{"SDL3 GPU HAL", gpuId, "Another HAL", otherId}));

	// The enumeration labels a device from its own description, which may differ from the
	// candidate's name. Matching on the id keeps that from producing a second row.
	std::vector<std::string> relabelled = Merge({"Something else", gpuId}, {Candidate("SDL3 GPU HAL", kGpu)});
	assert((relabelled == std::vector<std::string>{"Something else", gpuId}));

	// A candidate reported twice is still one row.
	std::vector<std::string> repeated = Merge({}, {Candidate("SDL3 GPU HAL", kGpu), Candidate("SDL3 GPU HAL", kGpu)});
	assert((repeated == std::vector<std::string>{"SDL3 GPU HAL", gpuId}));

	// No candidates at all leaves the enumeration alone.
	std::vector<std::string> untouched = Merge({"Another HAL", otherId}, {});
	assert((untouched == std::vector<std::string>{"Another HAL", otherId}));

	// A nameless candidate still has to be selectable, so it falls back to its id.
	std::vector<std::string> unnamed = Merge({}, {Candidate(nullptr, kGpu)});
	assert((unnamed == std::vector<std::string>{gpuId, gpuId}));

	// An id names one device and not another, which is the whole basis for deciding the window
	// from the configured renderer.
	assert(Miniwin_DeviceIdNamesGuid(gpuId.c_str(), kGpu));
	assert(!Miniwin_DeviceIdNamesGuid(otherId.c_str(), kGpu));

	// Nothing has validated the configured value, so anything unreadable means no preference.
	assert(!Miniwin_DeviceIdNamesGuid(nullptr, kGpu));
	assert(!Miniwin_DeviceIdNamesGuid("", kGpu));
	assert(!Miniwin_DeviceIdNamesGuid(gpuId.substr(0, gpuId.find_last_of(' ')).c_str(), kGpu));
	assert(!Miniwin_DeviceIdNamesGuid("not an id", kGpu));

	// ParseDeviceName rejects a negative ordinal outright, and resolves no device for any
	// ordinal but the first, so neither may build a window for this device.
	assert(!Miniwin_DeviceIdNamesGuid(("-1" + gpuId.substr(1)).c_str(), kGpu));
	assert(!Miniwin_DeviceIdNamesGuid(("1" + gpuId.substr(1)).c_str(), kGpu));

	// ParseDeviceName's sscanf stops at the fifth conversion and ignores the rest, so this one
	// does too: an id the game accepts must not be one that changes the window it gets.
	assert(Miniwin_DeviceIdNamesGuid((gpuId + " and then some").c_str(), kGpu));
}
