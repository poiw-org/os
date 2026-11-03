// Disabled keys, kept sorted in RAM and mirrored to flash. Entries are the first 16 bytes of
// SHA-256(key id), so any id format fits in 20 bytes. Entries disappear once the key would have
// expired anyway, which keeps the set small.
//
// Every method takes the set's own lock: the main loop checks it on each tap, the realtime task
// edits it and the REST task replaces it, and none of them waits for more than a few microseconds.
#pragma once
#include <mutex>
#include <stddef.h>
#include <stdint.h>

struct BlockedKey {
	uint8_t hash[16];
	uint32_t exp;
};

class BlacklistSet {
 public:
	static constexpr size_t kCapacity = 200;
	static constexpr size_t kMaxSerialized = 16 + kCapacity * sizeof(BlockedKey);

	void clear() {
		std::lock_guard<std::mutex> hold(lock_);
		count_ = 0;
		overflowed_ = false;
	}
	size_t size() const {
		std::lock_guard<std::mutex> hold(lock_);
		return count_;
	}
	// True once an insert had to evict or refuse an entry; the next full sync starts from scratch.
	bool overflowed() const {
		std::lock_guard<std::mutex> hold(lock_);
		return overflowed_;
	}
	// Makes this set an exact copy of `other`.
	void replaceWith(const BlacklistSet& other);

	bool contains(const char* id) const;
	// Adds or refreshes a key. When full, the entry that expires first is dropped.
	void upsert(const char* id, uint32_t exp);
	void remove(const char* id);
	size_t prune(uint32_t now);

	size_t serialize(uint8_t* out, size_t cap) const;
	bool deserialize(const uint8_t* data, size_t len);

	static void hashId(const char* id, uint8_t out[16]);

 private:
	int find(const uint8_t* hash, bool* found) const;  // caller holds the lock
	mutable std::mutex lock_;
	BlockedKey items_[kCapacity];
	size_t count_ = 0;
	bool overflowed_ = false;
};
