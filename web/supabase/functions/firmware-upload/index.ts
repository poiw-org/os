// Publishes a firmware image for approval, on behalf of the person who granted the release token
// (firmware-cli-auth). It only queues a build: an admin still has to approve that exact hash in the
// portal before any door will fetch it.
//
// POST <firmware.bin>, headers Authorization: Bearer <token>, x-firmware-version: <label>
//   ->  { id, sha256, size }
import { accessToken, env, HttpError, handlePost, serviceClient } from "../_shared/http.ts";
import { mayRelease, NOT_ALLOWED, UPLOAD_AUDIENCE, verifyReleaseToken } from "../_shared/release-token.ts";
import { isFirmwareImage, MAX_IMAGE_BYTES, sha256Hex } from "./image.ts";

const VERSION = /^[\w.+-]{1,64}$/;

Deno.serve((req) =>
  // handlePost reads the body as JSON, so the image is taken from an untouched copy.
  handlePost(req.clone(), async () => {
    const db = serviceClient();
    const claims = await verifyReleaseToken(env("FIRMWARE_CLI_JWT_SECRET"), accessToken(req), UPLOAD_AUDIENCE);
    if (!claims) throw new HttpError(401, "Invalid or expired release token");
    // Checked on every upload, so losing access ends it without waiting for the token to expire.
    if (!(await mayRelease(db, claims.sub))) throw new HttpError(403, NOT_ALLOWED);
    const version = req.headers.get("x-firmware-version") ?? "";
    if (!VERSION.test(version)) throw new HttpError(400, "Missing or invalid x-firmware-version");
    if (Number(req.headers.get("content-length")) > MAX_IMAGE_BYTES)
      throw new HttpError(413, "The image does not fit an app slot");

    const image = new Uint8Array(await req.arrayBuffer());
    if (!isFirmwareImage(image)) throw new HttpError(400, "That is not an ESP32 firmware image");

    const sha256 = await sha256Hex(image);
    const { error: stored } = await db.storage
      .from("firmware")
      .upload(`${sha256}.bin`, image, { upsert: true, contentType: "application/octet-stream" });
    if (stored) throw new Error(stored.message);

    // A new upload replaces whatever was still waiting for approval.
    const { error: replaced } = await db
      .from("firmware_releases")
      .update({ status: "rejected" })
      .eq("status", "pending");
    if (replaced) throw new Error(replaced.message);

    const { data, error } = await db
      .from("firmware_releases")
      .insert({ version, sha256, size: image.length, uploaded_by: claims.sub })
      .select("id")
      .single();
    if (error) throw new Error(error.message);
    return { id: data.id, sha256, size: image.length };
  })
);
