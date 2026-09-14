#include "gamefiles.h"

#include "configstore.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <set>
#include <stdexcept>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;
extern const char* g_files[46];

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
// A complete game tree under p_root/LEGO whose files all hold p_marker.
static void MakeTree(const fs::path& p_root, const std::string& p_marker, bool p_lowercase = false)
{
	for (const char* file : g_files) {
		std::string relative = file + 1;
		if (p_lowercase) {
			std::transform(relative.begin(), relative.end(), relative.begin(), [](unsigned char c) {
				return std::tolower(c);
			});
		}
		fs::path path = p_root / relative;
		fs::create_directories(path.parent_path());
		Write(path, p_marker);
	}
}
static std::string Marker(const fs::path& p_root)
{
	return Read(p_root / (g_files[0] + 1));
}
static std::set<std::string> Entries(const fs::path& p_dir)
{
	std::set<std::string> names;
	for (const auto& entry : fs::directory_iterator(p_dir)) {
		names.insert(entry.path().filename().string());
	}
	return names;
}
static std::string DiskPath(const fs::path& p_config)
{
	std::vector<std::pair<bool, std::string>> values;
	assert(Android_ReadConfig(p_config, {"isle:diskpath"}, values).empty());
	return values[0].first ? values[0].second : "";
}
static bool Throws(const std::function<void()>& p_call)
{
	try {
		p_call();
	}
	catch (const std::runtime_error&) {
		return true;
	}
	return false;
}

int main()
{
	std::string pattern = (fs::temp_directory_path() / "isle-gamefiles-test-XXXXXX").string();
	char* directory = mkdtemp(pattern.data());
	assert(directory);
	fs::path base = directory;
	fs::path files = base / "files", internal = base / "internal", record = internal / "game-files";
	fs::path config = internal / "isle.ini", saves = internal / "saves";

	// The old installation: a live LEGO tree, an unused earlier import that diskpath still names,
	// and a leftover from an import that could not delete a file.
	auto reset = [&] {
		fs::remove_all(files);
		fs::remove_all(internal);
		fs::create_directories(saves);
		MakeTree(files, "old");
		MakeTree(files / "imported-5", "older");
		Write(files / "CREDITS.SI.unreadable.1", "stale");
		Write(config, "[isle]\ndiskpath = " + (files / "imported-5").string() + "\nmusic = false\n");
		Write(saves / "G0.GS", "save");
	};
	auto stage = [&](const std::string& p_marker) {
		std::string id = Android_GameFiles::NewId();
		MakeTree(files / Android_GameFiles::StagingName(id), p_marker);
		return id;
	};
	auto apply = [&](Android_GameFiles& p_store, const std::set<std::string>& p_claimed = {}) {
		std::vector<std::string> garbage;
		std::string message = p_store.Apply(garbage, p_claimed);
		for (const std::string& path : garbage) {
			uint64_t bytes = 0;
			assert(Android_DeleteTree(path, bytes));
		}
		return message;
	};
	auto replaced = [&] {
		assert(Marker(files) == "new");
		assert(Android_FindMissingGameFile(files) == nullptr);
		assert(Entries(files) == std::set<std::string>{"LEGO"});
		assert(DiskPath(config) == files.string());
		assert(Read(config).find("music") != std::string::npos);
		assert(Read(saves / "G0.GS") == "save");
		assert(!Android_GameFiles(record, files).Pending());
	};

	// Lookups ignore case per component, as the game's own do, and require regular files.
	MakeTree(base / "complete", "x");
	assert(Android_FindMissingGameFile(base / "complete") == nullptr);
	MakeTree(base / "lower", "x", true);
	assert(Android_FindMissingGameFile(base / "lower") == nullptr);
	fs::remove(base / "complete" / (g_files[7] + 1));
	assert(Android_FindMissingGameFile(base / "complete") == g_files[7]);
	fs::create_directory(base / "complete" / (g_files[7] + 1));
	assert(Android_FindMissingGameFile(base / "complete") == g_files[7]);
	fs::remove(base / "complete" / (g_files[7] + 1));
	fs::create_symlink(base / "nowhere", base / "complete" / (g_files[7] + 1));
	assert(Android_FindMissingGameFile(base / "complete") == g_files[7]);
	assert(Android_FindMissingGameFile(base / "absent") == g_files[0]);

	// Replacing: scheduling never touches the live files; startup swaps in the staged tree,
	// points diskpath at the root and clears every earlier installation.
	reset();
	{
		Android_GameFiles store(record, files);
		std::string id = stage("new");
		store.Schedule(config, id);
		assert(store.Pending());
		assert(Marker(files) == "old");
		assert(Throws([&] { store.Schedule(config, stage("again")); }));
		assert(apply(store) == "Game files replaced.");
		replaced();
		assert(apply(store).empty());
		replaced();
	}

	// With nothing installed before, and with no configuration yet: a new one already points
	// diskpath at the root, so none is written.
	reset();
	fs::remove_all(files);
	fs::create_directories(files);
	fs::remove(config);
	{
		Android_GameFiles store(record, files);
		store.Schedule(config, stage("new"));
		apply(store);
		assert(Marker(files) == "new");
		assert(!fs::exists(config));
	}

	// Refusals leave nothing scheduled.
	reset();
	{
		Android_GameFiles store(record, files);
		std::string id = stage("new");
		fs::remove(files / Android_GameFiles::StagingName(id) / (g_files[3] + 1));
		assert(Throws([&] { store.Schedule(config, id); }));
		assert(Throws([&] { store.Schedule(config, "12a"); }));
		assert(Throws([&] { store.Schedule(config, "99"); }));
		assert(Throws([&] { store.Schedule("relative.ini", stage("new")); }));
		assert(!store.Pending());
		apply(store);
		assert(Marker(files) == "old");
		assert(!fs::exists(files / Android_GameFiles::StagingName(id)));
	}

	// Removing clears every installation but never saves or unrelated settings.
	reset();
	fs::create_directory(files / ".isle-replaced-3");
	{
		Android_GameFiles store(record, files);
		store.Schedule(config, "");
		assert(Marker(files) == "old");
		assert(apply(store) == "Game files removed.");
		assert(Entries(files).empty());
		assert(DiskPath(config) == files.string());
		assert(Read(saves / "G0.GS") == "save");
		assert(!store.Pending());
	}

	// Interrupt startup after every filesystem step. Each run must leave a game folder in place
	// or the change still waiting, and a clean run afterwards must finish it.
	for (bool remove : {false, true}) {
		int points = 0;
		reset();
		Android_GameFiles(record, files).Schedule(config, remove ? "" : stage("new"));
		{
			Android_GameFiles counting(record, files, [&](const char*) { points++; });
			apply(counting);
		}
		for (int failure = 1; failure <= points; failure++) {
			reset();
			Android_GameFiles(record, files).Schedule(config, remove ? "" : stage("new"));
			int point = 0;
			try {
				Android_GameFiles interrupted(record, files, [&](const char*) {
					if (++point == failure) {
						throw std::runtime_error("interrupted");
					}
				});
				apply(interrupted);
			}
			catch (const std::runtime_error&) {
			}
			Android_GameFiles recovered(record, files);
			assert(remove || fs::exists(files / "LEGO") || recovered.Pending());
			apply(recovered);
			if (remove) {
				assert(Entries(files).empty());
				assert(!recovered.Pending());
			}
			else {
				replaced();
			}
		}
		std::cout << (remove ? "Removal" : "Replacement") << " survived " << points << " interruption points\n";
	}

	// Interrupt scheduling after every step. The live files never change, and whatever was
	// published completes on the next startup.
	{
		int points = 0;
		reset();
		Android_GameFiles(record, files, [&](const char*) { points++; }).Schedule(config, stage("new"));
		for (int failure = 1; failure <= points; failure++) {
			reset();
			std::string id = stage("new");
			int point = 0;
			try {
				Android_GameFiles(record, files, [&](const char*) {
					if (++point == failure) {
						throw std::runtime_error("interrupted");
					}
				}).Schedule(config, id);
			}
			catch (const std::runtime_error&) {
			}
			assert(Marker(files) == "old");
			Android_GameFiles recovered(record, files);
			if (recovered.Pending()) {
				apply(recovered);
				replaced();
			}
			else {
				apply(recovered);
				assert(Marker(files) == "old");
				assert(!fs::exists(files / Android_GameFiles::StagingName(id)));
			}
		}
	}

	// A configuration that cannot be written keeps the change waiting with the new files already
	// playable, and leaves the earlier import alone until diskpath stops naming it.
	reset();
	{
		Android_GameFiles store(record, files);
		store.Schedule(config, stage("new"));
		fs::create_directory(config.string() + ".new");
		assert(apply(store).find("could not be updated") != std::string::npos);
		assert(store.Pending());
		assert(Marker(files) == "new");
		assert(fs::exists(files / "imported-5"));
		fs::remove(config.string() + ".new");
		assert(apply(store) == "Game files replaced.");
		replaced();
	}

	// A staged copy that disappears after the old one was retired puts the old one back.
	reset();
	{
		std::string id = stage("new");
		Android_GameFiles(record, files).Schedule(config, id);
		std::vector<std::string> unused;
		try {
			Android_GameFiles(record, files, [&](const char* p_point) {
				if (std::string(p_point) == "retire") {
					fs::remove_all(files / Android_GameFiles::StagingName(id));
					throw std::runtime_error("interrupted");
				}
			}).Apply(unused);
		}
		catch (const std::runtime_error&) {
		}
		Android_GameFiles store(record, files);
		assert(apply(store).find("were missing") != std::string::npos);
		assert(Marker(files) == "old");
		assert(!store.Pending());
	}

	// A staged copy that loses a file after scheduling never displaces the live files.
	reset();
	{
		std::string id = stage("new");
		Android_GameFiles store(record, files);
		store.Schedule(config, id);
		fs::remove(files / Android_GameFiles::StagingName(id) / (g_files[5] + 1));
		assert(apply(store).find("incomplete") != std::string::npos);
		assert(Marker(files) == "old");
		assert(!store.Pending());
	}

	// Renames the file system refuses keep the previous files, before or after they were retired.
	for (const char* step : {"retire?", "install?"}) {
		reset();
		Android_GameFiles(record, files).Schedule(config, stage("new"));
		Android_GameFiles failing(record, files, [&](const char* p_point) {
			if (std::string(p_point) == step) {
				throw std::system_error(EACCES, std::generic_category());
			}
		});
		std::string message = apply(failing);
		assert(message.find(strerror(EACCES)) != std::string::npos && message.find("kept") != std::string::npos);
		assert(Marker(files) == "old");
		assert(!failing.Pending());
		assert(Entries(files) == (std::set<std::string>{"LEGO", "imported-5", "CREDITS.SI.unreadable.1"}));
	}

	// Replacing when the only earlier installation is an import beside undeletable data, which is
	// what the startup import leaves when it could not clear the root.
	reset();
	fs::remove_all(files / "LEGO");
	{
		Android_GameFiles store(record, files);
		store.Schedule(config, stage("new"));
		assert(apply(store) == "Game files replaced.");
		replaced();
	}

	// A retired tree that could not be deleted is renamed first, so it is never brought back later.
	reset();
	{
		Android_GameFiles store(record, files);
		store.Schedule(config, stage("new"));
		std::vector<std::string> undeleted;
		store.Apply(undeleted);
		for (const std::string& path : undeleted) {
			assert(fs::path(path).filename().string().rfind(".isle-replaced-", 0) != 0);
		}
		fs::remove_all(files / "LEGO");
		assert(apply(store).empty());
		assert(!fs::exists(files / "LEGO"));
	}

	// A game folder that reappears after the old one was retired is kept as it is.
	reset();
	{
		Android_GameFiles(record, files).Schedule(config, stage("new"));
		std::vector<std::string> unused;
		try {
			Android_GameFiles(record, files, [&](const char* p_point) {
				if (std::string(p_point) == "retire") {
					MakeTree(files, "other");
					throw std::runtime_error("interrupted");
				}
			}).Apply(unused);
		}
		catch (const std::runtime_error&) {
		}
		Android_GameFiles store(record, files);
		assert(apply(store).find("skipped") != std::string::npos);
		assert(Marker(files) == "other");
		assert(!store.Pending());
		assert(Entries(files) == (std::set<std::string>{"LEGO", "imported-5", "CREDITS.SI.unreadable.1"}));
	}

	// One that reappears incomplete, such as a wrong folder imported meanwhile, gives way to the
	// retired files.
	reset();
	{
		Android_GameFiles(record, files).Schedule(config, stage("new"));
		std::vector<std::string> unused;
		try {
			Android_GameFiles(record, files, [&](const char* p_point) {
				if (std::string(p_point) == "retire") {
					fs::create_directories(files / "LEGO" / "Scripts");
					throw std::runtime_error("interrupted");
				}
			}).Apply(unused);
		}
		catch (const std::runtime_error&) {
		}
		Android_GameFiles store(record, files);
		assert(apply(store).find("skipped") != std::string::npos);
		assert(Marker(files) == "old");
		assert(!store.Pending());
		assert(Entries(files) == (std::set<std::string>{"LEGO", "imported-5", "CREDITS.SI.unreadable.1"}));
	}

	// Startup collects work directories nobody needs, but not one a running Settings still owns.
	// A retired tree with nothing in its place comes back, newest first.
	reset();
	fs::create_directory(files / ".isle-staging-7");
	fs::create_directory(files / ".isle-staging-8");
	fs::create_directory(files / ".isle-removed-9-0");
	{
		Android_GameFiles store(record, files);
		std::vector<std::string> garbage;
		assert(store.Apply(garbage, {"7"}).empty());
		std::sort(garbage.begin(), garbage.end());
		assert(
			garbage ==
			(std::vector<std::string>{(files / ".isle-removed-9-0").string(), (files / ".isle-staging-8").string()})
		);
		fs::remove_all(files / "LEGO");
		MakeTree(base / "five", "five");
		MakeTree(base / "twelve", "twelve");
		fs::rename(base / "five" / "LEGO", files / ".isle-replaced-5");
		fs::rename(base / "twelve" / "LEGO", files / ".isle-replaced-12");
		assert(apply(store, {"7"}) == "The previous game files were recovered.");
		assert(Marker(files) == "twelve");
		assert(!fs::exists(files / ".isle-replaced-5"));
		assert(fs::exists(files / ".isle-staging-7"));
	}

	// A damaged record is set aside and skipped; one for another storage root is dropped.
	reset();
	fs::create_directories(record);
	Write(record / "state", "damaged");
	{
		Android_GameFiles store(record, files);
		assert(apply(store).find("could not be read") != std::string::npos);
		assert(Read(record / "state.damaged") == "damaged");
		assert(!store.Pending());
		assert(Marker(files) == "old");
	}
	reset();
	fs::create_directories(base / "elsewhere");
	{
		Android_GameFiles(record, files).Schedule(config, stage("new"));
		Android_GameFiles moved(record, base / "elsewhere");
		assert(apply(moved).find("moved") != std::string::npos);
		assert(!moved.Pending());
		assert(Marker(files) == "old");
	}

	// Deleting never follows a symlink out of the tree, and reports what it freed.
	fs::create_directories(base / "outside");
	Write(base / "outside" / "keep", "keep");
	fs::create_directories(base / "doomed" / "nested");
	Write(base / "doomed" / "nested" / "file", "0123456789");
	fs::create_directory_symlink(base / "outside", base / "doomed" / "link");
	{
		uint64_t bytes = 0;
		int pumps = 0;
		assert(Android_DeleteTree((base / "doomed").string(), bytes, [&] { pumps++; }));
		assert(bytes == 10);
		assert(pumps == 4);
		assert(!fs::exists(base / "doomed"));
		assert(Read(base / "outside" / "keep") == "keep");
	}

	fs::remove_all(base);
	std::cout << "Game file tests passed\n";
}
