#include "saverestore.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

namespace fs = std::filesystem;
static void Write(const fs::path& path, const std::string& bytes)
{
	std::ofstream file(path, std::ios::binary);
	file << bytes;
	assert(file.good());
}
static std::string Read(const fs::path& path)
{
	std::ifstream file(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(file), {}};
}
static const std::vector<Android_SaveFile> incoming = {{"G0.GS", {1, 2, 3}}, {"Players.gsi", {4, 5}}};
int main()
{
	char pattern[] = "/tmp/isle-restore-test-XXXXXX";
	fs::path base = mkdtemp(pattern);
	fs::path saves = base / "saves", journal = base / "journal";
	auto reset = [&] {
		fs::remove_all(saves);
		fs::remove_all(journal);
		fs::create_directory(saves);
		fs::create_directory(journal);
		Write(saves / "g0.gs", "original");
		Write(saves / "G8.GS", "old slot");
		Write(saves / "unrelated", "keep");
	};
	auto old = [&] {
		assert(Read(saves / "g0.gs") == "original");
		assert(Read(saves / "G8.GS") == "old slot");
		assert(!fs::exists(saves / "Players.gsi"));
		assert(Read(saves / "unrelated") == "keep");
	};
	auto fresh = [&] {
		assert(Read(saves / "G0.GS") == std::string("\1\2\3", 3));
		assert(!fs::exists(saves / "G8.GS"));
		assert(Read(saves / "Players.gsi") == std::string("\4\5", 2));
		assert(Read(saves / "unrelated") == "keep");
	};
	reset();
	Android_SaveRestore store(journal);
	store.Schedule(saves, incoming, false);
	old(); // Scheduling must not touch live saves.
	Write(saves / "g0.gs", "shutdown save");
	assert(!store.Recover(saves).empty());
	fresh();
	assert(!store.Previous(saves).empty());
	store.Schedule(saves, {}, true);
	store.Recover(saves);
	assert(Read(saves / "g0.gs") == "shutdown save");
	assert(store.Previous(saves).empty());
	assert(store.Recover(saves).empty());

	// Interrupt after every filesystem checkpoint. Reopening must settle on one
	// complete generation, including when the commit rename happened before sync.
	int checkpoints = 0;
	reset();
	Android_SaveRestore(journal).Schedule(saves, incoming, false);
	Android_SaveRestore(journal, [&](const char*) { checkpoints++; }).Recover(saves);
	for (int failure = 1; failure <= checkpoints; failure++) {
		reset();
		Android_SaveRestore(journal).Schedule(saves, incoming, false);
		int point = 0;
		try {
			Android_SaveRestore interrupted(journal, [&](const char*) {
				if (++point == failure) {
					throw std::runtime_error("interrupted");
				}
			});
			interrupted.Recover(saves);
		}
		catch (const std::runtime_error&) {
		}
		Android_SaveRestore recovered(journal);
		recovered.Recover(saves);
		if (fs::exists(saves / "Players.gsi")) {
			fresh();
		}
		else {
			old();
		}
		std::string bytes = Read(saves / "G0.GS");
		recovered.Recover(saves);
		assert(bytes == Read(saves / "G0.GS"));
	}

	// Fail rollback itself, then retry. The earlier recovery copy must survive.
	reset();
	store.Schedule(saves, incoming, false);
	store.Recover(saves);
	std::string previous = store.Previous(saves);
	store.Schedule(saves, {}, true);
	try {
		Android_SaveRestore(journal, [](const char* p) {
			if (std::string(p) == "replace") {
				throw std::runtime_error("interrupted");
			}
		}).Recover(saves);
	}
	catch (const std::runtime_error&) {
	}
	try {
		Android_SaveRestore(journal, [](const char* p) {
			if (std::string(p) == "replace") {
				throw std::runtime_error("rollback interrupted");
			}
		}).Recover(saves);
	}
	catch (const std::runtime_error&) {
	}
	store.Recover(saves);
	fresh();
	assert(store.Previous(saves) == previous);

	reset();
	store.Schedule(saves, incoming, false);
	fs::rename(saves, base / "moved");
	fs::create_directory(saves);
	try {
		store.Recover(saves);
		assert(false);
	}
	catch (const std::runtime_error&) {
	}
	assert(fs::is_empty(saves));
	fs::remove(saves);
	fs::rename(base / "moved", saves);
	store.Recover(saves);
	fresh();

	reset();
	Write(saves / "Players.gsi", "x");
	fs::remove(saves / "g0.gs");
	fs::create_symlink(saves / "Players.gsi", saves / "g0.gs");
	store.Schedule(saves, incoming, false);
	try {
		store.Recover(saves);
		assert(false);
	}
	catch (const std::runtime_error&) {
	}
	assert(fs::is_symlink(saves / "g0.gs"));
	assert(Read(saves / "Players.gsi") == "x");

	reset();
	store.Schedule(saves, incoming, false);
	Write(journal / "state", "damaged");
	try {
		store.Recover(saves);
		assert(false);
	}
	catch (const std::runtime_error&) {
	}
	old();
	assert(Read(journal / "state") == "damaged");

	reset();
	fs::remove(saves / "g0.gs");
	fs::remove(saves / "G8.GS");
	store.Schedule(saves, incoming, false);
	store.Recover(saves);
	assert(store.Previous(saves).find(":empty") != std::string::npos);
	store.Schedule(saves, {}, true);
	store.Recover(saves);
	assert(!fs::exists(saves / "G0.GS"));
	assert(Read(saves / "unrelated") == "keep");
	fs::remove_all(base);
	std::cout << "Restore tests passed, including " << checkpoints << " interruption points\n";
}
