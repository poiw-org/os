#include "BlacklistSet.h"
#include <string.h>
#include "Crc.h"
#include "Sha256.h"

static const uint32_t kMagic = 0x314C4244;  // "DBL1"

void BlacklistSet::hashId(const char* id, uint8_t out[16]) {
	Sha256 s;
	s.update((const uint8_t*)id, strlen(id));
	uint8_t full[32];
	s.final(full);
	memcpy(out, full, 16);
}

void BlacklistSet::replaceWith(const BlacklistSet& other) {
	if (this == &other) return;
	std::scoped_lock hold(lock_, other.lock_);
	memcpy(items_, other.items_, other.count_ * sizeof(BlockedKey));
	count_ = other.count_;
	overflowed_ = other.overflowed_;
}

bool BlacklistSet::replaceWithIfUnedited(const BlacklistSet& other, uint32_t seen) {
	if (this == &other) return true;
	std::scoped_lock hold(lock_, other.lock_);
	if (edits_ != seen) return false;
	memcpy(items_, other.items_, other.count_ * sizeof(BlockedKey));
	count_ = other.count_;
	overflowed_ = other.overflowed_;
	return true;
}

// Index of the entry, or where it would be inserted.
int BlacklistSet::find(const uint8_t* hash, bool* found) const {
	int lo = 0, hi = (int)count_ - 1;
	while (lo <= hi) {
		int mid = (lo + hi) / 2;
		int c = memcmp(items_[mid].hash, hash, 16);
		if (c == 0) { *found = true; return mid; }
		if (c < 0) lo = mid + 1; else hi = mid - 1;
	}
	*found = false;
	return lo;
}

bool BlacklistSet::contains(const char* id) const {
	uint8_t h[16];
	hashId(id, h);
	std::lock_guard<std::mutex> hold(lock_);
	bool found;
	find(h, &found);
	return found;
}

void BlacklistSet::upsert(const char* id, uint32_t exp) {
	BlockedKey k;
	hashId(id, k.hash);
	k.exp = exp;
	std::lock_guard<std::mutex> hold(lock_);
	edits_++;
	bool found;
	int at = find(k.hash, &found);
	if (found) { items_[at].exp = exp; return; }
	if (count_ == kCapacity) {
		// The newest revocation matters most (an admin acting now): evict the oldest-expiring entry.
		overflowed_ = true;
		size_t victim = count_;
		for (size_t i = 0; i < count_; i++) {
			if (!memcmp(items_[i].hash, lastAdded_, 16)) continue;
			if (victim == count_ || items_[i].exp < items_[victim].exp) victim = i;
		}
		memmove(&items_[victim], &items_[victim + 1], (count_ - victim - 1) * sizeof(BlockedKey));
		count_--;
		at = find(k.hash, &found);
	}
	memmove(&items_[at + 1], &items_[at], (count_ - (size_t)at) * sizeof(BlockedKey));
	items_[at] = k;
	count_++;
	memcpy(lastAdded_, k.hash, 16);
}

void BlacklistSet::remove(const char* id) {
	uint8_t h[16];
	hashId(id, h);
	std::lock_guard<std::mutex> hold(lock_);
	edits_++;
	bool found;
	int at = find(h, &found);
	if (!found) return;
	memmove(&items_[at], &items_[at + 1], (count_ - (size_t)at - 1) * sizeof(BlockedKey));
	count_--;
}

size_t BlacklistSet::prune(uint32_t now) {
	std::lock_guard<std::mutex> hold(lock_);
	size_t kept = 0;
	for (size_t i = 0; i < count_; i++)
		if (items_[i].exp > now) items_[kept++] = items_[i];
	size_t dropped = count_ - kept;
	count_ = kept;
	return dropped;
}

// magic(4) count(4) | entries | crc32(4) over everything before it.
size_t BlacklistSet::serialize(uint8_t* out, size_t cap) const {
	std::lock_guard<std::mutex> hold(lock_);
	size_t need = 8 + count_ * sizeof(BlockedKey) + 4;
	if (cap < need) return 0;
	memcpy(out, &kMagic, 4);
	uint32_t n = (uint32_t)count_;
	memcpy(out + 4, &n, 4);
	for (size_t i = 0; i < count_; i++) {
		memcpy(out + 8 + i * 20, items_[i].hash, 16);
		memcpy(out + 8 + i * 20 + 16, &items_[i].exp, 4);
	}
	uint32_t crc = crc32(out, need - 4);
	memcpy(out + need - 4, &crc, 4);
	return need;
}

bool BlacklistSet::deserialize(const uint8_t* data, size_t len) {
	if (len < 12) return false;
	std::lock_guard<std::mutex> hold(lock_);
	uint32_t magic, n, crc;
	memcpy(&magic, data, 4);
	memcpy(&n, data + 4, 4);
	if (magic != kMagic || n > kCapacity || len != 12 + (size_t)n * 20) return false;
	memcpy(&crc, data + len - 4, 4);
	if (crc != crc32(data, len - 4)) return false;
	count_ = n;
	for (size_t i = 0; i < count_; i++) {
		memcpy(items_[i].hash, data + 8 + i * 20, 16);
		memcpy(&items_[i].exp, data + 8 + i * 20 + 16, 4);
	}
	overflowed_ = false;
	// Sorted order is an invariant of find(); do not trust the file for it.
	for (size_t i = 1; i < count_; i++)
		if (memcmp(items_[i - 1].hash, items_[i].hash, 16) >= 0) { count_ = 0; return false; }
	return true;
}
