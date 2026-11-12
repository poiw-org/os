// The messages a phone and the door exchange over BLE (docs/ble-protocol.md, section 3), free of
// UART and BLE types so every byte is unit tested on a workstation.
#pragma once
#include <stddef.h>
#include <stdint.h>

constexpr uint8_t kBleChallenge = 0x01;
constexpr uint8_t kBleRequest = 0x02;
constexpr uint8_t kBleResult = 0x03;
constexpr uint8_t kBleVersion = 0x01;
constexpr size_t kBleNonceLen = 16;
constexpr size_t kBleMaxDoorId = 31;
constexpr size_t kBleSigLen = 64;
constexpr size_t kBleMaxBody = 1024;  // the bridge refuses larger reassembled requests

// What the door tells the phone (the table in section 3).
enum class BleResult : uint8_t {
	Opened = 0,
	Malformed = 1,
	BadCredential = 2,
	ProofFailed = 3,
	Disabled = 4,
	Expired = 5,
	NotYetValid = 6,
	NoDoorKey = 7,
	Busy = 8
};

// 0x01 | version | nonce[16] | door_id_len | door_id. Returns the length, or 0 when the door id is
// empty or longer than 31 bytes or `cap` is too small.
size_t bleEncodeChallenge(const uint8_t nonce[kBleNonceLen], const char* doorId, uint8_t* out, size_t cap);

// 0x03 | code.
inline size_t bleEncodeResult(BleResult code, uint8_t out[2]) {
	out[0] = kBleResult;
	out[1] = (uint8_t)code;
	return 2;
}

// A request body: 0x02 | version | signature[64] | jwt. Both pointers point into `body`.
struct BleRequest {
	const uint8_t* sig;
	const char* jwt;
	size_t jwtLen;
};
bool bleParseRequest(const uint8_t* body, size_t len, BleRequest& out);

// SHA-256 of "doora-ble-1" | door_id_len | door_id | nonce, the digest the phone signs. The door
// always passes its own id and the live nonce.
void bleProofHash(const char* doorId, const uint8_t nonce[kBleNonceLen], uint8_t hash[32]);
