#ifndef ANDROID_SAVERESTORE_H
#define ANDROID_SAVERESTORE_H

#include "savesnapshot.h"

#include <functional>
#include <string>

// All methods may throw std::exception. Serialize calls externally. Recover must run
// before constructing the game; Schedule only records intent and never changes saves.
class Android_SaveRestore {
public:
	using Checkpoint = std::function<void(const char*)>;
	explicit Android_SaveRestore(std::string p_root, Checkpoint p_checkpoint = {});
	void Schedule(const std::string& p_destination, const std::vector<Android_SaveFile>& p_files, bool p_previous);
	std::string Recover(const std::string& p_destination);
	std::string Previous(const std::string& p_destination);
	bool Pending();
	bool CanCancel();
	void Cancel();

private:
	std::string m_root;
	Checkpoint m_checkpoint;
};

#endif
