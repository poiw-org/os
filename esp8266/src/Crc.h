// Checksums used to detect torn or corrupted flash writes.
#pragma once
#include <stddef.h>
#include <stdint.h>

inline uint32_t crc32(const uint8_t* data, size_t len, uint32_t crc = 0) {
	crc = ~crc;
	while (len--) {
		crc ^= *data++;
		for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
	}
	return ~crc;
}

inline uint16_t crc16(const uint8_t* data, size_t len) {
	uint16_t crc = 0xFFFF;
	while (len--) {
		crc ^= (uint16_t)(*data++) << 8;
		for (int k = 0; k < 8; k++) crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
	}
	return crc;
}
