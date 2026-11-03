// An in-memory LittleFS with the calls Persist.cpp uses, so the real Persist code runs on the
// workstation. Files are byte vectors; opening for "w" truncates, "a" appends, "r" reads.
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <map>
#include <string>
#include <vector>

struct FakeFs {
	std::map<std::string, std::vector<uint8_t>> files;
	bool mounted = false;
	int failWritesAfter = -1;  // bytes of writes allowed before every write comes up short; -1 = no limit
};
extern FakeFs g_fs;

class File {
 public:
	File() = default;
	File(const std::string& name, char mode) : name_(name), mode_(mode), valid_(true) {}
	explicit operator bool() const { return valid_; }
	size_t size() { return g_fs.files[name_].size(); }
	size_t read(uint8_t* buf, size_t n) {
		auto& d = g_fs.files[name_];
		if (pos_ >= d.size()) return 0;
		size_t got = n < d.size() - pos_ ? n : d.size() - pos_;
		for (size_t i = 0; i < got; i++) buf[i] = d[pos_ + i];
		pos_ += got;
		return got;
	}
	size_t write(const uint8_t* buf, size_t n) {
		if (mode_ == 'r') return 0;
		if (g_fs.failWritesAfter >= 0) {
			if ((size_t)g_fs.failWritesAfter < n) n = (size_t)g_fs.failWritesAfter;
			g_fs.failWritesAfter -= (int)n;
		}
		auto& d = g_fs.files[name_];
		d.insert(d.end(), buf, buf + n);  // "w" truncated at open, so both modes append
		return n;
	}
	bool seek(uint32_t pos) {
		if (pos > g_fs.files[name_].size()) return false;
		pos_ = pos;
		return true;
	}
	void close() { valid_ = false; }

 private:
	std::string name_;
	char mode_ = 'r';
	size_t pos_ = 0;
	bool valid_ = false;
};

struct FakeLittleFs {
	bool begin(bool) { return g_fs.mounted; }
	bool format() { g_fs.files.clear(); g_fs.mounted = true; return true; }
	bool exists(const char* path) { return g_fs.files.count(path) != 0; }
	bool remove(const char* path) { return g_fs.files.erase(path) != 0; }
	bool rename(const char* from, const char* to) {
		auto it = g_fs.files.find(from);
		if (it == g_fs.files.end()) return false;
		g_fs.files[to] = it->second;  // replaces an existing file, like lfs_rename
		g_fs.files.erase(from);
		return true;
	}
	File open(const char* path, const char* mode) {
		if (mode[0] == 'r') return g_fs.files.count(path) ? File(path, 'r') : File();
		if (mode[0] == 'w') g_fs.files[path].clear();
		else g_fs.files[path];  // "a" creates
		return File(path, mode[0]);
	}
};
extern FakeLittleFs LittleFS;
