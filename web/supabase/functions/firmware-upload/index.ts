// Publishes a firmware image for approval. The caller's credential is the image's own signature:
// only the holder of the signing key can create one, and an admin still has to approve it in the
// portal before any door will fetch it.
//
// POST <signed firmware.bin>, header x-firmware-version: <label>  ->  { id, sha256, size }
import { env, HttpError, handlePost, serviceClient } from "../_shared/http.ts";
import { MAX_IMAGE_BYTES, sha256Hex, verifySignedImage } from "./image.ts";

const VERSION = /^[\w.+-]{1,64}$/;

Deno.serve((req) =>
  // handlePost reads the body as JSON, so the image is taken from an untouched copy.
  handlePost(req.clone(), async () => {
    const version = req.headers.get("x-firmware-version") ?? "";
    if (!VERSION.test(version)) throw new HttpError(400, "Missing or invalid x-firmware-version");
    if (Number(req.headers.get("content-length")) > MAX_IMAGE_BYTES)
      throw new HttpError(413, "The image does not fit an app slot");

    const image = new Uint8Array(await req.arrayBuffer());
    if (!(await verifySignedImage(image, env("FIRMWARE_SIGNING_MODULUS"))))
      throw new HttpError(403, "The image is not signed with the firmware key");

    const sha256 = await sha256Hex(image);
    const db = serviceClient();
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
      .insert({ version, sha256, size: image.length })
      .select("id")
      .single();
    if (error) throw new Error(error.message);
    return { id: data.id, sha256, size: image.length };
  })
);
