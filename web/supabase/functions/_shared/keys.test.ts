// Run with: node --test supabase/functions/_shared/keys.test.ts   (from web/)
import assert from "node:assert/strict";
import { createPublicKey, createVerify, generateKeyPairSync, randomBytes } from "node:crypto";
import test from "node:test";
import { parseDeviceName, parseDevicePubkey } from "./device.ts";
import { MAX_TOKEN_LENGTH, peekClaims, signKey } from "./keys.ts";
import { publicModulus } from "./rsa.ts";

const SEED = "unit-test-seed-not-a-secret-0123456789";
const key = {
  id: "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee",
  userId: "11111111-2222-3333-4444-555555555555",
  issuedAt: new Date("2026-01-01T00:00:00Z"),
  notBefore: new Date("2026-01-01T00:00:00Z"),
  expiresAt: new Date("2027-01-01T00:00:00Z"),
};
const phone = () => {
  const { publicKey } = generateKeyPairSync("ec", { namedCurve: "P-256" });
  const jwk = publicKey.export({ format: "jwk" });
  return Buffer.concat([Buffer.from([4]), Buffer.from(jwk.x!, "base64url"), Buffer.from(jwk.y!, "base64url")]).toString("base64url");
};

test("a bound virtual key is a BLE credential that verifies against the door's public key", async () => {
  const pub = phone();
  const jwt = await signKey(SEED, { ...key, serialNumber: null, devicePubkey: pub });
  const c = peekClaims(jwt);
  assert.equal(c.aud, "ble");
  assert.equal(c.cnf, pub);
  assert.equal(pub.length, 87);
  assert.ok(jwt.length <= MAX_TOKEN_LENGTH);
  const [h, p, s] = jwt.split(".");
  const n = publicModulus(SEED);
  const rsa = createPublicKey({ key: { kty: "RSA", n, e: "AQAB" }, format: "jwk" });
  assert.ok(createVerify("RSA-SHA256").update(`${h}.${p}`).verify(rsa, Buffer.from(s, "base64url")));
});

test("a tag key keeps its serial and never carries cnf; an unbound virtual key matches nothing", async () => {
  const tag = peekClaims(await signKey(SEED, { ...key, serialNumber: "04a1b2c3d4e5f6" }));
  assert.equal(tag.aud, "04a1b2c3d4e5f6");
  assert.equal("cnf" in tag, false);
  const virtual = peekClaims(await signKey(SEED, { ...key, serialNumber: null }));
  assert.equal(virtual.aud, "virtual");
  assert.equal("cnf" in virtual, false);
});

test("a tag key cannot be bound to a phone", async () => {
  await assert.rejects(signKey(SEED, { ...key, serialNumber: "04a1", devicePubkey: phone() }));
});

test("public key validation", async () => {
  const pub = phone();
  assert.equal(await parseDevicePubkey(pub), pub);
  const bytes = Buffer.from(pub, "base64url");
  const offCurve = Buffer.from(bytes);
  offCurve[64] ^= 1;
  assert.equal(await parseDevicePubkey(offCurve.toString("base64url")), undefined);
  const compressed = Buffer.from(bytes.subarray(0, 33)).toString("base64url");
  assert.equal(await parseDevicePubkey(compressed), undefined);
  assert.equal(await parseDevicePubkey(pub.slice(1)), undefined);
  assert.equal(await parseDevicePubkey(`${pub.slice(0, 86)}=`), undefined);
  assert.equal(await parseDevicePubkey(randomBytes(65).toString("base64url")), undefined);
  assert.equal(await parseDevicePubkey(undefined), undefined);
  assert.equal(await parseDevicePubkey(42), undefined);
});

test("device name validation", () => {
  assert.equal(parseDeviceName("  Ann's iPhone "), "Ann's iPhone");
  assert.equal(parseDeviceName(""), undefined);
  assert.equal(parseDeviceName("   "), undefined);
  assert.equal(parseDeviceName("x".repeat(64)), "x".repeat(64));
  assert.equal(parseDeviceName("x".repeat(65)), undefined);
  assert.equal(parseDeviceName(null), undefined);
});
