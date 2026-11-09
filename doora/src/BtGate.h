// Who may use the Bluetooth console: a connection stays closed until an admin approves it within
// kApprovalMs, and an approved device may come back without asking for kGrantMs after the approval.
// Grants live in RAM, so a restart asks again. Portable, so the rules are unit-tested on the workstation.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

class BtGate {
 public:
	static constexpr uint32_t kApprovalMs = 20000;
	static constexpr uint32_t kGrantMs = 24UL * 3600 * 1000;
	enum class Link : uint8_t { None, Pending, Open };

	void connected(const uint8_t address[6], uint32_t ms) {
		memcpy(address_, address, 6);
		const Grant* g = find(address, ms);
		link_ = g ? Link::Open : Link::Pending;
		deadline_ = g ? g->until : ms + kApprovalMs;
	}

	// True when this approval opened the link; a late or repeated one changes nothing.
	bool approve(uint32_t ms) {
		if (link_ != Link::Pending || expired(ms)) return false;
		link_ = Link::Open;
		deadline_ = ms + kGrantMs;
		Grant& g = slot(ms);
		memcpy(g.address, address_, 6);
		g.until = deadline_;
		g.used = true;
		return true;
	}

	void closed() { link_ = Link::None; }

	// A new pairing means a new link key, so whoever holds it is not the device that was approved.
	void forget(const uint8_t address[6]) {
		for (Grant& g : grants_)
			if (g.used && !memcmp(g.address, address, 6)) g.used = false;
	}

	// True when the link's time is up: the approval window or the grant has run out.
	bool expired(uint32_t ms) const { return link_ != Link::None && (int32_t)(ms - deadline_) >= 0; }
	Link link() const { return link_; }
	uint32_t deadline() const { return deadline_; }
	bool granted(const uint8_t address[6], uint32_t ms) const { return find(address, ms) != nullptr; }

 private:
	struct Grant {
		uint8_t address[6];
		uint32_t until;
		bool used;
	};
	static constexpr size_t kGrants = 4;

	const Grant* find(const uint8_t address[6], uint32_t ms) const {
		for (const Grant& g : grants_)
			if (g.used && !memcmp(g.address, address, 6) && (int32_t)(ms - g.until) < 0) return &g;
		return nullptr;
	}
	// A free or lapsed slot, else the grant that ends first.
	Grant& slot(uint32_t ms) {
		Grant* soonest = &grants_[0];
		for (Grant& g : grants_) {
			if (!g.used || (int32_t)(ms - g.until) >= 0) return g;
			if ((int32_t)(g.until - soonest->until) < 0) soonest = &g;
		}
		return *soonest;
	}

	Grant grants_[kGrants] = {};
	uint8_t address_[6] = {0};
	uint32_t deadline_ = 0;
	Link link_ = Link::None;
};
