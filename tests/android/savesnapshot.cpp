#include "savesnapshot.h"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <new>
#include <sys/stat.h>
#include <unistd.h>

static bool g_failAllocation;

void* operator new(size_t p_size)
{
	if (g_failAllocation) {
		g_failAllocation = false;
		throw std::bad_alloc();
	}
	if (void* memory = malloc(p_size ? p_size : 1)) {
		return memory;
	}
	throw std::bad_alloc();
}

void operator delete(void* p_memory) noexcept
{
	free(p_memory);
}

int main()
{
	char directory[] = "/tmp/isle-snapshot-XXXXXX";
	assert(mkdtemp(directory));
	std::filesystem::path root(directory);
	assert(Android_ReadSaveSnapshot("").m_error.size());
	assert(Android_ReadSaveSnapshot((root / "missing").string()).m_files.empty());
	assert(Android_ReadSaveSnapshot(directory).m_files.empty());
	std::ofstream(root / "g0.gs", std::ios::binary) << "progress";
	std::ofstream(root / "isle.ini") << "private";
	auto snapshot = Android_ReadSaveSnapshot(directory);
	assert(snapshot.m_error.empty() && snapshot.m_incomplete && snapshot.m_files.size() == 1);
	assert(snapshot.m_files[0].m_name == "G0.GS");
	// Case-sensitive volumes can contain both names; case-insensitive hosts cannot.
	if (!std::filesystem::exists(root / "G0.GS")) {
		std::ofstream(root / "G0.GS") << "duplicate";
		assert(!Android_ReadSaveSnapshot(directory).m_error.empty());
		std::filesystem::remove(root / "G0.GS");
	}
	std::ofstream(root / "g0.gs") << "changed";
	assert(std::string(snapshot.m_files[0].m_bytes.begin(), snapshot.m_files[0].m_bytes.end()) == "progress");
	std::ofstream(root / "Players.gsi") << "players";
	std::ofstream(root / "History.gsi") << "history";
	for (int i = 1; i < 9; i++) {
		std::ofstream(root / ("G" + std::to_string(i) + ".GS")) << i;
	}
	snapshot = Android_ReadSaveSnapshot(directory);
	assert(snapshot.m_error.empty() && !snapshot.m_incomplete && snapshot.m_files.size() == 11);
	std::string directoryPath = directory;
	g_failAllocation = true;
	snapshot = Android_ReadSaveSnapshot(directoryPath);
	assert(!snapshot.m_error.empty() && snapshot.m_files.empty() && !g_failAllocation);
	std::filesystem::remove(root / "g0.gs");
	std::filesystem::create_symlink(root / "isle.ini", root / "G0.GS");
	snapshot = Android_ReadSaveSnapshot(directory);
	assert(!snapshot.m_error.empty() && snapshot.m_files.empty());
	std::filesystem::remove(root / "G0.GS");
	assert(mkfifo((root / "G0.GS").c_str(), 0600) == 0);
	assert(!Android_ReadSaveSnapshot(directory).m_error.empty());
	std::filesystem::remove(root / "G0.GS");
	std::ofstream(root / "G0.GS");
	std::filesystem::resize_file(root / "G0.GS", 16 * 1024 * 1024);
	assert(!Android_ReadSaveSnapshot(directory).m_error.empty());
	std::filesystem::resize_file(root / "G0.GS", 0);
	std::filesystem::permissions(root / "G0.GS", std::filesystem::perms::none);
	if (geteuid() != 0) {
		assert(!Android_ReadSaveSnapshot(directory).m_error.empty());
	}
	std::filesystem::remove_all(root);
}
