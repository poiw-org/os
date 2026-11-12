/* micro-ecc (https://github.com/kmackay/micro-ecc, commit 541b3a7, BSD-2-Clause, see LICENSE.txt),
 * built for what the door needs: verifying ECDSA P-256 signatures. Only the secp256r1 curve is
 * compiled, and point compression and signing are left out. uECC.c itself is unmodified.
 *
 * Verification never calls the random number generator, so none is installed. */
#define uECC_SUPPORTS_secp160r1 0
#define uECC_SUPPORTS_secp192r1 0
#define uECC_SUPPORTS_secp224r1 0
#define uECC_SUPPORTS_secp256r1 1
#define uECC_SUPPORTS_secp256k1 0
#define uECC_SUPPORT_COMPRESSED_POINT 0
#define uECC_OPTIMIZATION_LEVEL 2
#include "uECC.c"
