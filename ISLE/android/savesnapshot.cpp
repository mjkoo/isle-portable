#include "savesnapshot.h"

#include <array>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <memory>
#include <new>
#include <stdexcept>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
constexpr size_t kMaximumBytes = 16 * 1024 * 1024;
const char* const kNames[] =
	{"G0.GS", "G1.GS", "G2.GS", "G3.GS", "G4.GS", "G5.GS", "G6.GS", "G7.GS", "G8.GS", "Players.gsi", "History.gsi"};

struct Descriptor {
	int m_fd;
	~Descriptor()
	{
		if (m_fd >= 0) {
			close(m_fd);
		}
	}
};

Android_SaveSnapshot Read(const std::string& p_directory)
{
	if (p_directory.empty()) {
		throw std::runtime_error("The save directory must not be empty.");
	}
	std::unique_ptr<DIR, decltype(&closedir)> directory(opendir(p_directory.c_str()), closedir);
	if (!directory) {
		if (errno == ENOENT) {
			return {};
		}
		throw std::runtime_error(std::string("Could not open the save directory: ") + strerror(errno));
	}
	std::array<std::string, 11> names;
	for (;;) {
		errno = 0;
		dirent* entry = readdir(directory.get());
		if (!entry) {
			if (errno) {
				throw std::runtime_error("Could not list the save directory.");
			}
			break;
		}
		for (size_t i = 0; i < names.size(); i++) {
			if (strcasecmp(entry->d_name, kNames[i]) == 0) {
				if (!names[i].empty()) {
					throw std::runtime_error("The save directory has duplicate filenames differing only in case.");
				}
				names[i] = entry->d_name;
			}
		}
	}
	Android_SaveSnapshot result;
	size_t total = 0;
	for (size_t i = 0; i < names.size(); i++) {
		if (names[i].empty()) {
			continue;
		}
		Descriptor file{openat(dirfd(directory.get()), names[i].c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK)
		};
		struct stat info;
		if (file.m_fd < 0 || fstat(file.m_fd, &info) != 0 || !S_ISREG(info.st_mode)) {
			throw std::runtime_error(std::string("Could not read regular save file ") + names[i] + ".");
		}
		if (info.st_size < 0 || static_cast<uint64_t>(info.st_size) > kMaximumBytes - total) {
			throw std::runtime_error("The save files exceed the 16 MiB export limit.");
		}
		Android_SaveFile saved{kNames[i], {}};
		saved.m_bytes.reserve(static_cast<size_t>(info.st_size));
		uint8_t buffer[16384];
		for (;;) {
			ssize_t count = read(file.m_fd, buffer, sizeof(buffer));
			if (count < 0 && errno == EINTR) {
				continue;
			}
			if (count < 0) {
				throw std::runtime_error(std::string("Could not read ") + names[i] + ".");
			}
			if (count == 0) {
				break;
			}
			if (static_cast<size_t>(count) > kMaximumBytes - total) {
				throw std::runtime_error("The save files exceed the 16 MiB export limit.");
			}
			total += count;
			saved.m_bytes.insert(saved.m_bytes.end(), buffer, buffer + count);
		}
		if (static_cast<uint64_t>(info.st_size) != saved.m_bytes.size()) {
			throw std::runtime_error(
				"A save file changed while it was being captured. Reopen the game menu and try again."
			);
		}
		int fd = file.m_fd;
		file.m_fd = -1;
		if (close(fd) != 0) {
			throw std::runtime_error("Could not close a save file after reading.");
		}
		result.m_files.push_back(std::move(saved));
	}
	bool slot = false;
	for (size_t i = 0; i < 9; i++) {
		slot = slot || !names[i].empty();
	}
	result.m_incomplete = !slot || names[9].empty() || names[10].empty();
	return result;
}
} // namespace

Android_SaveSnapshot Android_ReadSaveSnapshot(const std::string& p_directory)
{
	try {
		return Read(p_directory);
	}
	catch (const std::bad_alloc&) {
		return {{}, "Not enough memory to capture save files.", false};
	}
	catch (const std::exception& error) {
		return {{}, error.what(), false};
	}
}
