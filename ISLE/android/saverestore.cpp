#include "saverestore.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <memory>
#include <set>
#include <stdexcept>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
constexpr size_t kLimit = 16 * 1024 * 1024;
constexpr size_t kJournalLimit = 3 * kLimit + 65536;
const char* const kNames[] =
	{"G0.GS", "G1.GS", "G2.GS", "G3.GS", "G4.GS", "G5.GS", "G6.GS", "G7.GS", "G8.GS", "Players.gsi", "History.gsi"};
using Files = std::vector<Android_SaveFile>;
using Hook = Android_SaveRestore::Checkpoint;

void Require(bool p_ok, const char* p_message)
{
	if (!p_ok) {
		throw std::runtime_error(p_message);
	}
}
void Point(const Hook& p_hook, const char* p_name)
{
	if (p_hook) {
		p_hook(p_name);
	}
}
struct FD {
	int fd;
	explicit FD(int p_fd) : fd(p_fd) { Require(fd >= 0, "Could not open restore storage."); }
	~FD()
	{
		if (fd >= 0) {
			close(fd);
		}
	}
	FD(const FD&) = delete;
	FD& operator=(const FD&) = delete;
	void Close()
	{
		int old = fd;
		fd = -1;
		Require(close(old) == 0, "Could not close restore storage.");
	}
};
int NameIndex(const std::string& p_name)
{
	for (size_t i = 0; i < std::size(kNames); i++) {
		if (strcasecmp(p_name.c_str(), kNames[i]) == 0) {
			return static_cast<int>(i);
		}
	}
	return -1;
}
void CheckFiles(const Files& p_files)
{
	std::set<int> seen;
	size_t total = 0;
	Require(p_files.size() <= 11, "Invalid restore file count.");
	for (const auto& file : p_files) {
		int index = NameIndex(file.m_name);
		Require(
			index >= 0 && file.m_name.find('\0') == std::string::npos && seen.insert(index).second,
			"Invalid or ambiguous save filename."
		);
		Require(file.m_bytes.size() <= kLimit - total, "The save files exceed the 16 MiB restore limit.");
		total += file.m_bytes.size();
	}
}
std::string Canonical(std::string p_path)
{
	Require(
		!p_path.empty() && p_path[0] == '/' && p_path.find('\0') == std::string::npos,
		"Restore needs an absolute save directory."
	);
	while (p_path.size() > 1 && p_path.back() == '/') {
		p_path.pop_back();
	}
	struct stat info;
	Require(
		lstat(p_path.c_str(), &info) == 0 && S_ISDIR(info.st_mode),
		"The save directory is missing, inaccessible or a symlink."
	);
	std::unique_ptr<char, decltype(&free)> path(realpath(p_path.c_str(), nullptr), free);
	Require(path != nullptr, "Could not resolve the save directory.");
	return path.get();
}
std::string Identity(int p_fd)
{
	struct stat info;
	Require(fstat(p_fd, &info) == 0 && S_ISDIR(info.st_mode), "Invalid save directory.");
	return std::to_string(info.st_dev) + ":" + std::to_string(info.st_ino);
}
std::vector<uint8_t> ReadFile(int p_dir, const std::string& p_name, size_t p_limit)
{
	FD file(openat(p_dir, p_name.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
	struct stat info;
	Require(
		fstat(file.fd, &info) == 0 && S_ISREG(info.st_mode) && info.st_size >= 0 &&
			static_cast<uint64_t>(info.st_size) <= p_limit,
		"Unreadable, nonregular or oversized restore file."
	);
	std::vector<uint8_t> bytes;
	uint8_t buffer[16384];
	for (;;) {
		ssize_t count = read(file.fd, buffer, sizeof(buffer));
		if (count < 0 && errno == EINTR) {
			continue;
		}
		Require(count >= 0, "Could not read restore file.");
		if (!count) {
			break;
		}
		Require(static_cast<size_t>(count) <= p_limit - bytes.size(), "Restore file exceeds the size limit.");
		bytes.insert(bytes.end(), buffer, buffer + count);
	}
	Require(bytes.size() == static_cast<uint64_t>(info.st_size), "Restore file changed while reading.");
	file.Close();
	return bytes;
}
void Sync(int p_fd, const Hook& p_hook)
{
	Require(fsync(p_fd) == 0, "Could not synchronize restore storage.");
	Point(p_hook, "sync");
}
void WriteFile(int p_dir, const std::string& p_name, const std::vector<uint8_t>& p_bytes, const Hook& p_hook)
{
	FD file(openat(p_dir, p_name.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600));
	Point(p_hook, "create");
	size_t offset = 0;
	while (offset < p_bytes.size()) {
		ssize_t count = write(file.fd, p_bytes.data() + offset, p_bytes.size() - offset);
		if (count < 0 && errno == EINTR) {
			continue;
		}
		Require(count > 0, "Could not write restore file. Check available space.");
		offset += count;
		Point(p_hook, "write");
	}
	Sync(file.fd, p_hook);
	file.Close();
	Point(p_hook, "close");
}
void Remove(int p_dir, const std::string& p_name, const Hook& p_hook)
{
	Require(unlinkat(p_dir, p_name.c_str(), 0) == 0 || errno == ENOENT, "Could not remove a restore file.");
	Point(p_hook, "remove");
}
Files Capture(int p_dir, bool p_read = true)
{
	FD duplicate(openat(p_dir, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
	std::unique_ptr<DIR, decltype(&closedir)> directory(fdopendir(duplicate.fd), closedir);
	Require(directory != nullptr, "Could not enumerate save files.");
	duplicate.fd = -1;
	Files files;
	size_t total = 0;
	for (;;) {
		errno = 0;
		dirent* entry = readdir(directory.get());
		if (!entry) {
			Require(errno == 0, "Could not enumerate save files.");
			break;
		}
		if (NameIndex(entry->d_name) < 0) {
			continue;
		}
		struct stat info;
		Require(
			fstatat(p_dir, entry->d_name, &info, AT_SYMLINK_NOFOLLOW) == 0 && S_ISREG(info.st_mode),
			"A recognized save entry is not a regular file."
		);
		auto bytes = p_read ? ReadFile(p_dir, entry->d_name, kLimit - total) : std::vector<uint8_t>{};
		total += bytes.size();
		files.push_back({entry->d_name, std::move(bytes)});
	}
	CheckFiles(files);
	std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) { return a.m_name < b.m_name; });
	return files;
}

// A checksummed, bounded journal contains the bytes it describes. This avoids a
// metadata-to-file publication race and keeps interrupted cleanup out of recovery.
struct State {
	uint32_t phase = 0; // Idle, confirmed, installing. Idle publication commits installation.
	bool previous = false;
	std::string id, path, identity, previousPath, previousIdentity;
	uint64_t time = 0, previousTime = 0;
	Files incoming, original, backup;
};
uint32_t Crc(const std::vector<uint8_t>& p_bytes)
{
	uint32_t crc = ~0U;
	for (uint8_t byte : p_bytes) {
		crc ^= byte;
		for (int bit = 0; bit < 8; bit++) {
			crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1)));
		}
	}
	return ~crc;
}
void Number(std::vector<uint8_t>& p_out, uint64_t p_n)
{
	for (int i = 0; i < 8; i++) {
		p_out.push_back(p_n & 255);
		p_n >>= 8;
	}
}
void String(std::vector<uint8_t>& p_out, const std::string& p_text)
{
	Number(p_out, p_text.size());
	p_out.insert(p_out.end(), p_text.begin(), p_text.end());
}
void Pack(std::vector<uint8_t>& p_out, const Files& p_files)
{
	CheckFiles(p_files);
	Number(p_out, p_files.size());
	for (const auto& file : p_files) {
		String(p_out, file.m_name);
		Number(p_out, file.m_bytes.size());
		p_out.insert(p_out.end(), file.m_bytes.begin(), file.m_bytes.end());
	}
}
struct Reader {
	const std::vector<uint8_t>& bytes;
	size_t pos = 0;
	uint64_t Num()
	{
		Require(bytes.size() - pos >= 8, "Restore journal is truncated. Recovery data was preserved.");
		uint64_t n = 0;
		for (int i = 0; i < 8; i++) {
			n |= static_cast<uint64_t>(bytes[pos++]) << (8 * i);
		}
		return n;
	}
	std::string Text()
	{
		uint64_t n = Num();
		Require(n <= 4096 && n <= bytes.size() - pos, "Invalid restore journal string.");
		std::string s(bytes.begin() + pos, bytes.begin() + pos + n);
		pos += n;
		Require(s.find('\0') == std::string::npos, "Invalid restore journal string.");
		return s;
	}
	Files Unpack()
	{
		uint64_t count = Num();
		Require(count <= 11, "Invalid restore journal file count.");
		Files files;
		size_t total = 0;
		for (size_t i = 0; i < count; i++) {
			std::string name = Text();
			uint64_t n = Num();
			Require(n <= kLimit - total && n <= bytes.size() - pos, "Invalid restore journal file size.");
			files.push_back({name, {bytes.begin() + pos, bytes.begin() + pos + n}});
			pos += n;
			total += n;
		}
		CheckFiles(files);
		return files;
	}
};
State Load(int p_root)
{
	struct stat info;
	if (fstatat(p_root, "state", &info, AT_SYMLINK_NOFOLLOW) != 0) {
		Require(errno == ENOENT, "Could not inspect restore journal.");
		return {};
	}
	auto bytes = ReadFile(p_root, "state", kJournalLimit);
	Require(bytes.size() >= 8, "Restore journal is damaged. Recovery data was preserved.");
	Reader checksum{bytes, bytes.size() - 8};
	uint64_t expected = checksum.Num();
	bytes.resize(bytes.size() - 8);
	Require(Crc(bytes) == expected, "Restore journal checksum failed. Recovery data was preserved.");
	Reader reader{bytes};
	Require(reader.Num() == 0x49534c4552535431ULL, "Unsupported restore journal version.");
	State state;
	uint64_t phase = reader.Num(), previous = reader.Num();
	Require(phase <= 2 && previous <= 1, "Invalid restore transaction state.");
	state.phase = phase;
	state.previous = previous;
	state.id = reader.Text();
	state.path = reader.Text();
	state.identity = reader.Text();
	state.previousPath = reader.Text();
	state.previousIdentity = reader.Text();
	state.time = reader.Num();
	state.previousTime = reader.Num();
	state.incoming = reader.Unpack();
	state.original = reader.Unpack();
	state.backup = reader.Unpack();
	Require(reader.pos == bytes.size(), "Unexpected restore journal data.");
	if (state.phase) {
		Require(
			!state.id.empty() && state.id.find_first_not_of("0123456789") == std::string::npos,
			"Invalid restore operation ID."
		);
	}
	return state;
}
void Store(int p_root, const State& p_state, const Hook& p_hook)
{
	std::vector<uint8_t> bytes;
	Number(bytes, 0x49534c4552535431ULL);
	Number(bytes, p_state.phase);
	Number(bytes, p_state.previous);
	String(bytes, p_state.id);
	String(bytes, p_state.path);
	String(bytes, p_state.identity);
	String(bytes, p_state.previousPath);
	String(bytes, p_state.previousIdentity);
	Number(bytes, p_state.time);
	Number(bytes, p_state.previousTime);
	Pack(bytes, p_state.incoming);
	Pack(bytes, p_state.original);
	Pack(bytes, p_state.backup);
	Number(bytes, Crc(bytes));
	Remove(p_root, "state.tmp", p_hook);
	WriteFile(p_root, "state.tmp", bytes, p_hook);
	Require(renameat(p_root, "state.tmp", p_root, "state") == 0, "Could not publish restore journal.");
	Point(p_hook, "journal");
	Sync(p_root, p_hook);
}
void Replace(int p_dir, const Files& p_files, const std::string& p_id, const Hook& p_hook)
{
	// Always prepare the complete set before removing any live file. Both install and
	// rollback can repeat after any mutation; private originals remain authoritative.
	for (const auto& file : p_files) {
		std::string temp = ".isle-restore-" + p_id + "-" + kNames[NameIndex(file.m_name)];
		Remove(p_dir, temp, p_hook);
		WriteFile(p_dir, temp, file.m_bytes, p_hook);
	}
	Sync(p_dir, p_hook);
	Files live = Capture(p_dir, false);
	for (const auto& file : live) {
		Remove(p_dir, file.m_name, p_hook);
	}
	for (const auto& file : p_files) {
		std::string temp = ".isle-restore-" + p_id + "-" + kNames[NameIndex(file.m_name)];
		Require(renameat(p_dir, temp.c_str(), p_dir, file.m_name.c_str()) == 0, "Could not replace save file.");
		Point(p_hook, "replace");
	}
	Sync(p_dir, p_hook);
	Files actual = Capture(p_dir), expected = p_files;
	std::sort(expected.begin(), expected.end(), [](const auto& a, const auto& b) { return a.m_name < b.m_name; });
	Require(actual.size() == expected.size(), "Restored save set failed verification.");
	for (size_t i = 0; i < actual.size(); i++) {
		Require(
			actual[i].m_name == expected[i].m_name && actual[i].m_bytes == expected[i].m_bytes,
			"Restored save bytes failed verification."
		);
	}
	// Remove only this transaction's reserved temporary names, including files that
	// were absent from the rollback set after an interrupted install.
	for (const char* name : kNames) {
		Remove(p_dir, ".isle-restore-" + p_id + "-" + name, p_hook);
	}
	Sync(p_dir, p_hook);
}
State Idle(const State& p_state)
{
	State idle;
	idle.backup = p_state.backup;
	idle.previousPath = p_state.previousPath;
	idle.previousIdentity = p_state.previousIdentity;
	idle.previousTime = p_state.previousTime;
	return idle;
}
} // namespace

Android_SaveRestore::Android_SaveRestore(std::string p_root, Checkpoint p_checkpoint)
	: m_root(std::move(p_root)), m_checkpoint(std::move(p_checkpoint))
{
	Require(mkdir(m_root.c_str(), 0700) == 0 || errno == EEXIST, "Could not create private restore storage.");
	FD root(open(m_root.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
	// A previous attempt may have published a commit but failed its directory sync.
	// Make that observed journal durable before allowing any subsequent game launch.
	Sync(root.fd, m_checkpoint);
	FD parent(open((m_root + "/..").c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
	Sync(parent.fd, m_checkpoint);
}

void Android_SaveRestore::Schedule(const std::string& p_destination, const Files& p_files, bool p_previous)
{
	FD root(open(m_root.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
	State state = Load(root.fd);
	Require(!state.phase, "A restore is already scheduled. Close and reopen the game.");
	std::string path = Canonical(p_destination);
	FD directory(open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
	std::string identity = Identity(directory.fd);
	if (p_previous) {
		Require(
			state.previousTime && state.previousPath == path && state.previousIdentity == identity,
			"No previous saves are available for this directory."
		);
		state.incoming = state.backup;
	}
	else {
		CheckFiles(p_files);
		Require(!p_files.empty(), "No save files selected.");
		state.incoming = p_files;
	}
	state.phase = 1;
	state.previous = p_previous;
	state.path = path;
	state.identity = identity;
	state.id = std::to_string(
		std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch())
			.count()
	);
	Store(root.fd, state, m_checkpoint);
}

std::string Android_SaveRestore::Recover(const std::string& p_destination)
{
	FD root(open(m_root.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
	State state = Load(root.fd);
	if (!state.phase) {
		return {};
	}
	std::string path = Canonical(p_destination);
	Require(path == state.path, "The configured save directory changed. Restore was stopped.");
	FD directory(open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
	Require(Identity(directory.fd) == state.identity, "The save directory was replaced. Restore was stopped.");
	if (state.phase == 2) {
		Replace(directory.fd, state.original, state.id, m_checkpoint);
		Store(root.fd, Idle(state), m_checkpoint);
		return "Restore was interrupted. The original save files were recovered.";
	}
	state.original = Capture(directory.fd);
	state.time =
		std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
			.count();
	state.phase = 2;
	Store(root.fd, state, m_checkpoint);
	Replace(directory.fd, state.incoming, state.id, m_checkpoint);
	State done;
	if (!state.previous) {
		done.backup = std::move(state.original);
		done.previousPath = state.path;
		done.previousIdentity = state.identity;
		done.previousTime = state.time;
	}
	Store(root.fd, done, m_checkpoint);
	return state.previous ? "Previous save files restored." : "Save files restored.";
}

std::string Android_SaveRestore::Previous(const std::string& p_destination)
{
	FD root(open(m_root.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
	State state = Load(root.fd);
	if (state.phase || !state.previousTime) {
		return {};
	}
	// A missing destination has no usable backup identity. Keep this advisory
	// lookup read-only so a new ZIP can still be selected and scheduled there.
	struct stat info;
	if (!p_destination.empty() && p_destination[0] == '/' && p_destination.find('\0') == std::string::npos &&
		lstat(p_destination.c_str(), &info) != 0 && errno == ENOENT) {
		return {};
	}
	std::string path = Canonical(p_destination);
	FD directory(open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
	if (state.previousPath != path || state.previousIdentity != Identity(directory.fd)) {
		return {};
	}
	return std::to_string(state.previousTime) + (state.backup.empty() ? ":empty" : ":saved");
}

bool Android_SaveRestore::Pending()
{
	FD root(open(m_root.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
	return Load(root.fd).phase != 0;
}

bool Android_SaveRestore::CanCancel()
{
	FD root(open(m_root.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
	return Load(root.fd).phase == 1;
}

void Android_SaveRestore::Cancel()
{
	FD root(open(m_root.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
	State state = Load(root.fd);
	Require(state.phase != 2, "Replacement has started. Recover the original saves before continuing.");
	if (state.phase == 1) {
		Store(root.fd, Idle(state), m_checkpoint);
	}
}
