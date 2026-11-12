// Validation for the phone a virtual key is bound to. Free of the database and the environment.
import { Buffer } from "node:buffer";

const PUBKEY = /^[A-Za-z0-9_-]{87}$/;

/**
 * A P-256 public key as 87 base64url characters (04 || X || Y), imported with WebCrypto so a point
 * that is not on the curve is refused before it is stored. Returns the canonical string, or
 * undefined when it is not a valid key.
 */
export async function parseDevicePubkey(value: unknown): Promise<string | undefined> {
  if (typeof value !== "string" || !PUBKEY.test(value)) return undefined;
  const raw = Buffer.from(value, "base64url");
  if (raw.length !== 65 || raw[0] !== 0x04) return undefined;
  try {
    await crypto.subtle.importKey("raw", raw, { name: "ECDSA", namedCurve: "P-256" }, true, ["verify"]);
  } catch {
    return undefined;
  }
  return value;
}

/** 1 to 64 characters after trimming. */
export function parseDeviceName(value: unknown): string | undefined {
  if (typeof value !== "string") return undefined;
  const name = value.trim();
  return name.length >= 1 && name.length <= 64 ? name : undefined;
}
