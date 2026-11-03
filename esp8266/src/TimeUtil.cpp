#include "TimeUtil.h"
#include <string.h>

static int64_t daysFromCivil(int y, unsigned m, unsigned d) {
	y -= m <= 2;
	const int era = (y >= 0 ? y : y - 399) / 400;
	const unsigned yoe = (unsigned)(y - era * 400);
	const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
	const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

uint32_t epochFromCivil(int year, int month, int day, int hour, int minute, int second) {
	if (year < 1970 || year > 2105 || month < 1 || month > 12 || day < 1 || day > 31) return 0;
	int64_t s = daysFromCivil(year, (unsigned)month, (unsigned)day) * 86400 + hour * 3600 + minute * 60 + second;
	return (s < 0 || s > 0xFFFFFFFFLL) ? 0 : (uint32_t)s;
}

void formatIso(uint32_t epoch, char* out) {
	int64_t z = (int64_t)(epoch / 86400) + 719468;
	const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
	const unsigned doe = (unsigned)(z - era * 146097);
	const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	int y = (int)yoe + (int)era * 400;
	const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	const unsigned mp = (5 * doy + 2) / 153;
	const unsigned d = doy - (153 * mp + 2) / 5 + 1;
	const unsigned m = mp < 10 ? mp + 3 : mp - 9;
	if (m <= 2) y++;
	uint32_t sod = epoch % 86400;
	unsigned hh = sod / 3600, mm = (sod % 3600) / 60, ss = sod % 60;
	char* p = out;
	auto two = [&p](unsigned v) { *p++ = '0' + v / 10; *p++ = '0' + v % 10; };
	*p++ = '0' + y / 1000 % 10; *p++ = '0' + y / 100 % 10; *p++ = '0' + y / 10 % 10; *p++ = '0' + y % 10;
	*p++ = '-'; two(m); *p++ = '-'; two(d); *p++ = 'T';
	two(hh); *p++ = ':'; two(mm); *p++ = ':'; two(ss); *p++ = 'Z'; *p = 0;
}

static bool digits(const char* s, int n, int& out) {
	out = 0;
	for (int i = 0; i < n; i++) {
		if (s[i] < '0' || s[i] > '9') return false;
		out = out * 10 + (s[i] - '0');
	}
	return true;
}

uint32_t parseIso(const char* t) {
	if (!t || strlen(t) < 19) return 0;
	int Y, M, D, h, m, s;
	if (!digits(t, 4, Y) || t[4] != '-' || !digits(t + 5, 2, M) || t[7] != '-' || !digits(t + 8, 2, D)) return 0;
	if ((t[10] != 'T' && t[10] != ' ') || !digits(t + 11, 2, h) || t[13] != ':' || !digits(t + 14, 2, m) ||
	    t[16] != ':' || !digits(t + 17, 2, s))
		return 0;
	const char* p = t + 19;
	if (*p == '.') { p++; while (*p >= '0' && *p <= '9') p++; }
	int64_t offset = 0;
	if (*p == '+' || *p == '-') {
		int sign = *p == '-' ? -1 : 1, oh = 0, om = 0;
		p++;
		if (!digits(p, 2, oh)) return 0;
		p += 2;
		if (*p == ':') p++;
		if (*p) digits(p, 2, om);
		offset = sign * (oh * 3600 + om * 60);
	} else if (*p && *p != 'Z') {
		return 0;
	}
	uint32_t base = epochFromCivil(Y, M, D, h, m, s);
	if (!base) return 0;
	int64_t r = (int64_t)base - offset;
	return (r <= 0 || r > 0xFFFFFFFFLL) ? 0 : (uint32_t)r;
}

uint32_t buildEpoch(const char* date, const char* time) {
	static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
	if (strlen(date) < 11 || strlen(time) < 8) return 0;
	int month = 0;
	for (int i = 0; i < 12; i++)
		if (!strncmp(date, months + i * 3, 3)) month = i + 1;
	int day = (date[4] == ' ' ? 0 : date[4] - '0') * 10 + (date[5] - '0');
	int year = 0, hh = 0, mm = 0, ss = 0;
	if (!month || !digits(date + 7, 4, year) || !digits(time, 2, hh) || !digits(time + 3, 2, mm) ||
	    !digits(time + 6, 2, ss))
		return 0;
	uint32_t t = epochFromCivil(year, month, day, hh, mm, ss);
	return t > 86400 ? t - 86400 : 0;
}

size_t formatUid(const uint8_t* uid, size_t len, char* out, size_t cap) {
	static const char hex[] = "0123456789abcdef";
	size_t need = len ? len * 3 : 1;
	if (cap < need) { if (cap) out[0] = 0; return 0; }
	size_t n = 0;
	for (size_t i = 0; i < len; i++) {
		if (i) out[n++] = ':';
		out[n++] = hex[uid[i] >> 4];
		out[n++] = hex[uid[i] & 15];
	}
	out[n] = 0;
	return n;
}

int parseUid(const char* text, uint8_t* out, size_t cap) {
	uint8_t nib[32];
	size_t n = 0;
	for (const char* p = text; *p; p++) {
		char c = *p;
		if (c == 'x' || c == 'X') {
			if (!n || nib[n - 1] != 0) return -1;
			n--;  // drop the '0' of a "0x" prefix
			continue;
		}
		if (c == ':' || c == ' ' || c == '-') continue;
		int v = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10
		        : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
		if (v < 0 || n >= sizeof(nib)) return -1;
		nib[n++] = (uint8_t)v;
	}
	if (!n || (n & 1) || n / 2 > cap) return -1;
	for (size_t i = 0; i < n / 2; i++) out[i] = (uint8_t)(nib[2 * i] << 4 | nib[2 * i + 1]);
	return (int)(n / 2);
}

size_t jsonEscape(const char* in, char* out, size_t cap) {
	static const char hex[] = "0123456789abcdef";
	size_t n = 0;
	for (const unsigned char* p = (const unsigned char*)in; *p; p++) {
		size_t need = (*p == '"' || *p == '\\' || *p == '\n' || *p == '\r' || *p == '\t') ? 2 : *p < 0x20 ? 6 : 1;
		if (n + need + 1 > cap) return 0;
		if (need == 1) out[n++] = (char)*p;
		else if (need == 2) {
			out[n++] = '\\';
			out[n++] = *p == '"' ? '"' : *p == '\\' ? '\\' : *p == '\n' ? 'n' : *p == '\r' ? 'r' : 't';
		} else {
			out[n++] = '\\'; out[n++] = 'u'; out[n++] = '0'; out[n++] = '0';
			out[n++] = hex[*p >> 4]; out[n++] = hex[*p & 15];
		}
	}
	out[n] = 0;
	return n;
}
