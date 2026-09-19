#include "renderers.h"

void Android_AddMissingRenderers(
	std::vector<std::string>& p_renderers,
	const MiniwinDeviceCandidate* p_candidates,
	int p_count
)
{
	for (int i = 0; i < p_count; i++) {
		const char* id = p_candidates[i].m_id;

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
