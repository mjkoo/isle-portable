#ifndef ANDROID_GAMEFILES_H
#define ANDROID_GAMEFILES_H

#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <vector>

// Replaces or removes the game data under the app's external files directory across a restart.
// The running game has its files open, so Settings only copies a new tree into a staging
// directory beside the live one and records the change; the next startup applies it before the
// engine reads anything. Every interruption leaves either the previous files or the new ones in
// place, never neither.
//
// All methods may throw std::exception. Serialize calls externally.
class Android_GameFiles {
public:
	// Called after each filesystem mutation, so tests can interrupt at every step.
	using Checkpoint = std::function<void(const char*)>;
	Android_GameFiles(std::string p_recordDir, std::string p_root, Checkpoint p_checkpoint = {});

	// The directory, under the root, that Settings copies the LEGO folder of a new tree into.
	static std::string StagingName(const std::string& p_id);
	static std::string NewId();

	// Records that the next startup installs the tree staged under p_id, or removes the game data
	// when p_id is empty, and points diskpath in p_config at the root. Checks the staged tree is
	// complete first. Never changes the live files.
	void Schedule(const std::string& p_config, const std::string& p_id);
	bool Pending();

	// Applies a recorded change, then collects work directories that earlier attempts left behind,
	// except those whose ids are in p_claimed. Returns a message for the player, or empty.
	// Directories to delete are appended to p_garbage for the caller, since deleting hundreds of
	// megabytes takes a while; the game cannot see them in the meantime.
	std::string Apply(std::vector<std::string>& p_garbage, const std::set<std::string>& p_claimed = {});

private:
	std::string m_recordDir;
	std::string m_root;
	Checkpoint m_checkpoint;
};

// The first required game file missing under p_root, or nullptr when the tree is complete. Path
// components match without regard to case, as the game's own lookup does.
const char* Android_FindMissingGameFile(const std::string& p_root);

// Deletes a directory tree without following symlinks, calling p_pump between entries. Adds the
// size of what was deleted to p_bytes. Returns false if anything could not be deleted.
bool Android_DeleteTree(const std::string& p_path, uint64_t& p_bytes, const std::function<void()>& p_pump = {});

#endif
