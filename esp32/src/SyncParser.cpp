#include "SyncParser.h"
#include <string.h>

void SyncParser::endString() {
	text_[len_] = 0;
	if (!strcmp(text_, "now")) field_ = Field::Now;
	else if (!strcmp(text_, "total")) field_ = Field::Total;
	else if (!strcmp(text_, "blocked")) { field_ = Field::Blocked; sawBlocked_ = true; havePending_ = false; }
	else if (field_ == Field::Blocked) { strcpy(pending_, text_); havePending_ = true; }
	len_ = 0;
	inString_ = false;
}

void SyncParser::endNumber() {
	uint32_t v = number_ > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)number_;
	if (field_ == Field::Now) { now_ = v; sawNow_ = true; }
	else if (field_ == Field::Total) total_ = v;
	else if (field_ == Field::Blocked && havePending_) {
		out_.upsert(pending_, v);
		count_++;
		havePending_ = false;
	}
	number_ = 0;
	inNumber_ = false;
}

void SyncParser::feed(const char* data, size_t len) {
	for (size_t i = 0; i < len; i++) {
		char c = data[i];
		if (inString_) {
			if (escape_) { escape_ = false; if (len_ + 1 < sizeof(text_)) text_[len_++] = c; }
			else if (c == '\\') escape_ = true;
			else if (c == '"') endString();
			else if (len_ + 1 < sizeof(text_)) text_[len_++] = c;
			continue;
		}
		if (c >= '0' && c <= '9') {
			inNumber_ = true;
			number_ = number_ * 10 + (uint64_t)(c - '0');
			if (number_ > 0xFFFFFFFFFFull) number_ = 0xFFFFFFFFFFull;
			continue;
		}
		if (inNumber_) endNumber();
		if (c == '"') { inString_ = true; len_ = 0; }
		else if (c == '{' || c == '[') { depth_++; opened_ = true; }
		else if (c == '}' || c == ']') depth_--;
	}
}
