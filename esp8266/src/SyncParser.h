// Streaming reader for the response of the `door_sync` RPC:
//   {"now": 1767000000, "total": 2, "blocked": [["<key id>", <expires epoch>], ...]}
// It never buffers the body, so a full list costs no RAM beyond the BlacklistSet it fills.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "BlacklistSet.h"

class SyncParser {
 public:
	explicit SyncParser(BlacklistSet& out) : out_(out) {}
	void feed(const char* data, size_t len);
	// The whole object arrived: server time, the list, and the closing brace. Anything less is a
	// truncated download and must not replace the stored blacklist.
	bool complete() const { return sawNow_ && sawBlocked_ && opened_ && depth_ == 0 && !inString_ && !inNumber_; }
	uint32_t now() const { return now_; }
	// Rows the server holds beyond what it returned.
	bool truncated() const { return total_ > count_; }

 private:
	enum class Field : uint8_t { None, Now, Total, Blocked };
	void endString();
	void endNumber();
	BlacklistSet& out_;
	Field field_ = Field::None;
	char text_[48];
	size_t len_ = 0;
	bool inString_ = false, escape_ = false, inNumber_ = false;
	bool sawNow_ = false, sawBlocked_ = false, havePending_ = false, opened_ = false;
	int depth_ = 0;
	char pending_[48];
	uint64_t number_ = 0;
	uint32_t now_ = 0, total_ = 0, count_ = 0;
};
