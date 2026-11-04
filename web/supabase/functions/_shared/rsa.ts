// The RSA key pair that signs v2 key tokens, derived from one secret seed (RSA_KEY_SEED).
//
// Nothing about the pair is stored. The seed feeds an HMAC-SHA256 stream that picks the two primes,
// so the same seed always yields the same pair. The private key exists only inside one call to
// `signRs256`, and the door gets the public modulus from the `door-key` function.
//
// Keep the seed secret and never change it: a different seed is a different key pair, and every key
// already written to a tag would stop verifying.
import { Buffer } from "node:buffer";
import { createHmac } from "node:crypto";

const E = 65537n;
const BITS = 2048;

const modPow = (base: bigint, exp: bigint, mod: bigint) => {
  let result = 1n;
  base %= mod;
  while (exp > 0n) {
    if (exp & 1n) result = (result * base) % mod;
    base = (base * base) % mod;
    exp >>= 1n;
  }
  return result;
};

const gcd = (a: bigint, b: bigint): bigint => (b === 0n ? a : gcd(b, a % b));

// Modular inverse by the extended Euclidean algorithm.
function modInverse(a: bigint, m: bigint) {
  let [r0, r1, t0, t1] = [m, a % m, 0n, 1n];
  while (r1 !== 0n) {
    const q = r0 / r1;
    [r0, r1] = [r1, r0 - q * r1];
    [t0, t1] = [t1, t0 - q * t1];
  }
  if (r0 !== 1n) throw new Error("not invertible");
  return ((t0 % m) + m) % m;
}

const SMALL_PRIMES = (() => {
  const primes: bigint[] = [];
  for (let n = 3; primes.length < 200; n += 2) {
    if (primes.every((p) => n % Number(p) !== 0)) primes.push(BigInt(n));
  }
  return primes;
})();
const WITNESSES = SMALL_PRIMES.slice(0, 24);

function isProbablePrime(n: bigint) {
  for (const p of SMALL_PRIMES) if (n % p === 0n) return n === p;
  let d = n - 1n;
  let s = 0;
  while ((d & 1n) === 0n) {
    d >>= 1n;
    s++;
  }
  witness: for (const a of WITNESSES) {
    let x = modPow(a, d, n);
    if (x === 1n || x === n - 1n) continue;
    for (let i = 1; i < s; i++) {
      x = (x * x) % n;
      if (x === n - 1n) continue witness;
    }
    return false;
  }
  return true;
}

const toBigInt = (bytes: Uint8Array) =>
  BigInt("0x" + Buffer.from(bytes).toString("hex"));
const b64url = (n: bigint) =>
  Buffer.from(n.toString(16).padStart(BITS / 4, "0"), "hex").toString(
    "base64url",
  );

// JWK integers are big-endian without leading zero bytes.
const b64urlMinimal = (n: bigint) => {
  const hex = n.toString(16);
  return Buffer.from(hex.length % 2 ? "0" + hex : hex, "hex").toString(
    "base64url",
  );
};

// A prime of exactly BITS/2 bits whose product with another such prime has BITS bits: the top two
// bits are set. It is the first prime at or above a point the seed picks, with p - 1 coprime to e.
function derivePrime(seed: string, label: string) {
  const bytes = BITS / 16;
  const stream = Buffer.concat(
    [0, 1, 2, 3].map((i) =>
      createHmac("sha256", seed).update(`doora-rsa-v2/${label}/${i}`).digest(),
    ),
  ).subarray(0, bytes);
  let candidate = toBigInt(stream) | (3n << BigInt(BITS / 2 - 2)) | 1n;
  while (!(gcd(candidate - 1n, E) === 1n && isProbablePrime(candidate)))
    candidate += 2n;
  return candidate;
}

function derive(seed: string) {
  if (seed.length < 32)
    throw new Error("RSA_KEY_SEED must be at least 32 characters");
  const p = derivePrime(seed, "p");
  const q = derivePrime(seed, "q");
  const n = p * q;
  const lambda = ((p - 1n) * (q - 1n)) / gcd(p - 1n, q - 1n);
  const d = modInverse(E, lambda);
  return {
    n,
    d,
    p,
    q,
    dp: d % (p - 1n),
    dq: d % (q - 1n),
    qi: modInverse(q, p),
  };
}

/** The public modulus as base64url (256 bytes); the exponent is always 65537. */
export const publicModulus = (seed: string) => b64url(derive(seed).n);

/** RS256 signature of `data`, base64url. The private key is derived for this call and dropped. */
export async function signRs256(seed: string, data: string) {
  const k = derive(seed);
  const key = await crypto.subtle.importKey(
    "jwk",
    {
      kty: "RSA",
      n: b64urlMinimal(k.n),
      e: b64urlMinimal(E),
      d: b64urlMinimal(k.d),
      p: b64urlMinimal(k.p),
      q: b64urlMinimal(k.q),
      dp: b64urlMinimal(k.dp),
      dq: b64urlMinimal(k.dq),
      qi: b64urlMinimal(k.qi),
    },
    { name: "RSASSA-PKCS1-v1_5", hash: "SHA-256" },
    false,
    ["sign"],
  );
  const signature = await crypto.subtle.sign(
    "RSASSA-PKCS1-v1_5",
    key,
    new TextEncoder().encode(data),
  );
  return Buffer.from(signature).toString("base64url");
}
