export const MAX_IMAGE_BYTES = 1966080; // one app slot in min_spiffs.csv

const ESP_IMAGE_MAGIC = 0xe9;

export const sha256Hex = async (bytes: Uint8Array) =>
  Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256", bytes)))
    .map((byte) => byte.toString(16).padStart(2, "0"))
    .join("");

/** True when `file` looks like an ESP32 app image that fits a slot. */
export const isFirmwareImage = (file: Uint8Array) =>
  file.length > 0 && file.length <= MAX_IMAGE_BYTES && file[0] === ESP_IMAGE_MAGIC;
