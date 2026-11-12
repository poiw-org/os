// Every key token is built here, so a tag key and a phone credential cannot drift apart. The
// functions that sign (keys, ble-credential) pass the seed in; nothing in this file reads the
// environment, which keeps it testable on its own.
import { Buffer } from "node:buffer";
import { signRs256 } from "./rsa.ts";

/** The most characters the door accepts in one token (kMaxTokenLen in doora/src/Jwt.h). */
export const MAX_TOKEN_LENGTH = 860;

export const seconds = (date: Date) => Math.floor(date.getTime() / 1000);

const base64Url = (value: unknown) =>
  Buffer.from(JSON.stringify(value)).toString("base64url");

export type KeyToSign = {
  id: string;
  userId: string;
  serialNumber: string | null;
  issuedAt: Date;
  notBefore: Date;
  expiresAt: Date;
  /** base64url of the phone's 65-byte P-256 public key; only for a virtual key. */
  devicePubkey?: string | null;
};

// Claims are what the door firmware reads: sub = user, jti = key id, aud = tag serial number.
//
// A virtual key with a bound phone is a BLE credential: aud "ble" and cnf, the phone's public key
// (docs/ble-protocol.md, section 1). One without a phone yet has an audience that matches no tag and
// is not "ble", so the door refuses it everywhere; and a tag key never carries cnf.
export async function signKey(seed: string, key: KeyToSign) {
  if (key.devicePubkey && key.serialNumber)
    throw new Error("A key with a tag cannot be bound to a phone");
  const audience = key.serialNumber ?? (key.devicePubkey ? "ble" : "virtual");
  const claims: Record<string, unknown> = {
    sub: key.userId,
    jti: key.id,
    aud: audience,
    ...(audience === "ble" ? { cnf: key.devicePubkey } : {}),
    iat: seconds(key.issuedAt),
    nbf: seconds(key.notBefore),
    exp: seconds(key.expiresAt),
  };
  const signed = `${base64Url({ alg: "RS256", typ: "JWT" })}.${base64Url(claims)}`;
  const token = `${signed}.${await signRs256(seed, signed)}`;
  if (token.length > MAX_TOKEN_LENGTH)
    throw new Error(`Token is ${token.length} characters, over the door's ${MAX_TOKEN_LENGTH}`);
  return token;
}

/** The claims of one of our tokens, without checking the signature: for comparing, never for trust. */
export function peekClaims(token: string): Record<string, unknown> {
  try {
    return JSON.parse(Buffer.from(token.split(".")[1] ?? "", "base64url").toString());
  } catch {
    return {};
  }
}
