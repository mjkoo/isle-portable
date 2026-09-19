#include "renderers.h"

#include <cstdio>
#include <cstring>

std::string Android_FormatDeviceId(const GUID& p_guid)
{
	// The four words are the GUID's bytes as LegoDeviceEnumerate reads them back, so they are
	// copied rather than taken field by field: m_data1, m_data2 and m_data3 would print a different
	// string, and ParseDeviceName would not match it. The leading zero is the DirectDraw driver
	// ordinal, and miniwin reports exactly one driver, so it is always the first. There is no
	// second chance if it is ever wrong - ProcessDeviceBytes only matches a device whose driver
	// ordinal is the one it was given.
	unsigned int words[4];
	static_assert(sizeof(words) == sizeof(GUID), "Equal size");
	std::memcpy(words, &p_guid, sizeof(words));

	char id[128];
	std::snprintf(id, sizeof(id), "0 0x%x 0x%x 0x%x 0x%x", words[0], words[1], words[2], words[3]);
	return id;
}

void Android_AddMissingRenderers(
	std::vector<std::string>& p_renderers,
	const MiniwinDeviceCandidate* p_candidates,
	int p_count
)
{
	for (int i = 0; i < p_count; i++) {
		std::string id = Android_FormatDeviceId(p_candidates[i].m_guid);

		// The list is read as pairs, so the id is at every odd position. Match on the id rather
		// than the label: the label is what a player reads, the id is what the game resolves.
		bool known = false;
		for (size_t j = 1; j < p_renderers.size(); j += 2) {
			if (p_renderers[j] == id) {
				known = true;
				break;
			}
		}
		if (known) {
			continue;
		}

		p_renderers.emplace_back(p_candidates[i].m_name ? p_candidates[i].m_name : id);
		p_renderers.emplace_back(id);
	}
}
