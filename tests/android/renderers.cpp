#include "renderers.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static const GUID kGpu = {0x682656F3, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}};
static const GUID kGles3 = {0x682656F3, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04}};

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

static void RoundTrips(const GUID& p_guid)
{
	int driver = -1;
	GUID parsed = {};
	assert(ParseDeviceName(Android_FormatDeviceId(p_guid), driver, parsed));
	// Miniwin reports one DirectDraw driver, and ProcessDeviceBytes only matches a device whose
	// driver ordinal is the one it was given, so anything but the first driver never matches.
	assert(driver == 0);
	assert(std::memcmp(&parsed, &p_guid, sizeof(GUID)) == 0);
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
	RoundTrips(kGles3);

	// Distinct devices must not collapse onto one id, or picking one would select the other.
	assert(Android_FormatDeviceId(kGpu) != Android_FormatDeviceId(kGles3));

	const std::string gpuId = Android_FormatDeviceId(kGpu);
	const std::string gles3Id = Android_FormatDeviceId(kGles3);

	// Nothing enumerated: every candidate is offered, in the order it was reported.
	std::vector<std::string> fromNothing = Merge({}, {{"SDL3 GPU HAL", kGpu}, {"OpenGL ES 3.0 HAL", kGles3}});
	assert((fromNothing == std::vector<std::string>{"SDL3 GPU HAL", gpuId, "OpenGL ES 3.0 HAL", gles3Id}));

	// A GL window enumerates the GL device and hides the GPU one, which is the case the list
	// has to repair, and it must not repeat what the enumeration already reported.
	std::vector<std::string> fromGl =
		Merge({"OpenGL ES 3.0 HAL", gles3Id}, {{"SDL3 GPU HAL", kGpu}, {"OpenGL ES 3.0 HAL", kGles3}});
	assert((fromGl == std::vector<std::string>{"OpenGL ES 3.0 HAL", gles3Id, "SDL3 GPU HAL", gpuId}));

	// A GPU window is the mirror image: the GL devices cannot initialize, so they are the ones
	// missing, and a player has to be able to choose one to get back.
	std::vector<std::string> fromGpu =
		Merge({"SDL3 GPU HAL", gpuId}, {{"SDL3 GPU HAL", kGpu}, {"OpenGL ES 3.0 HAL", kGles3}});
	assert((fromGpu == std::vector<std::string>{"SDL3 GPU HAL", gpuId, "OpenGL ES 3.0 HAL", gles3Id}));

	// The enumeration labels a device from its own description, which may differ from the
	// candidate's name. Matching on the id keeps that from producing a second row.
	std::vector<std::string> relabelled = Merge({"Something else", gpuId}, {{"SDL3 GPU HAL", kGpu}});
	assert((relabelled == std::vector<std::string>{"Something else", gpuId}));

	// A candidate reported twice is still one row.
	std::vector<std::string> repeated = Merge({}, {{"SDL3 GPU HAL", kGpu}, {"SDL3 GPU HAL", kGpu}});
	assert((repeated == std::vector<std::string>{"SDL3 GPU HAL", gpuId}));

	// No candidates at all leaves the enumeration alone.
	std::vector<std::string> untouched = Merge({"OpenGL ES 3.0 HAL", gles3Id}, {});
	assert((untouched == std::vector<std::string>{"OpenGL ES 3.0 HAL", gles3Id}));

	// A nameless candidate still has to be selectable, so it falls back to its id.
	std::vector<std::string> unnamed = Merge({}, {{nullptr, kGpu}});
	assert((unnamed == std::vector<std::string>{gpuId, gpuId}));

	std::printf("renderers: ok\n");
	return 0;
}
