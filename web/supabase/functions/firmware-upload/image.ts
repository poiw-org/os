// A signed firmware image, as tools/ota.py builds it and doora/src/OtaImage.h checks it on the door:
//
//   image | RSASSA-PKCS1-v1_5 SHA-256 signature of image (256 bytes) | "DOORASIG"
export const MAX_IMAGE_BYTES = 1966080; // one app slot in min_spiffs.csv

const MAGIC = new TextEncoder().encode("DOORASIG");
const SIGNATURE_BYTES = 256;
const TRAILER_BYTES = SIGNATURE_BYTES + MAGIC.length;
const ESP_IMAGE_MAGIC = 0xe9;

const base64Url = (bytes: Uint8Array) =>
  btoa(String.fromCharCode(...bytes))
    .replace(/\+/g, "-")
    .replace(/\//g, "_")
    .replace(/=+$/, "");

const fromHex = (hex: string) =>
  Uint8Array.from(hex.match(/../g) ?? [], (byte) => parseInt(byte, 16));

export const sha256Hex = async (bytes: Uint8Array) =>
  Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256", bytes)))
    .map((byte) => byte.toString(16).padStart(2, "0"))
    .join("");

/** True when `file` is an ESP32 image with a valid trailer for the RSA-2048 modulus `modulusHex`. */
export async function verifySignedImage(file: Uint8Array, modulusHex: string) {
  if (file.length <= TRAILER_BYTES || file.length > MAX_IMAGE_BYTES) return false;
  const end = file.length - MAGIC.length;
  if (file[0] !== ESP_IMAGE_MAGIC || !MAGIC.every((b, i) => file[end + i] === b)) return false;
  const key = await crypto.subtle.importKey(
    "jwk",
    { kty: "RSA", n: base64Url(fromHex(modulusHex)), e: "AQAB", alg: "RS256", ext: true },
    { name: "RSASSA-PKCS1-v1_5", hash: "SHA-256" },
    false,
    ["verify"],
  );
  return crypto.subtle.verify(
    "RSASSA-PKCS1-v1_5",
    key,
    file.subarray(end - SIGNATURE_BYTES, end),
    file.subarray(0, end - SIGNATURE_BYTES),
  );
}
