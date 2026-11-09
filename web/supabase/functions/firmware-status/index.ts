// Where a build is on its way to the door, for the `make release` that uploaded it. The CLI holds a
// release token rather than a Supabase session, so it cannot read the table or subscribe to Realtime.
//
// POST { id }, header Authorization: Bearer <token>  ->  { status, door_state, door_error }
import { accessToken, env, HttpError, handlePost, serviceClient } from "../_shared/http.ts";
import { UPLOAD_AUDIENCE, verifyReleaseToken } from "../_shared/release-token.ts";

const ID = /^[0-9a-f]{8}(-[0-9a-f]{4}){3}-[0-9a-f]{12}$/i;

Deno.serve((req) =>
  handlePost(req, async (body) => {
    const claims = await verifyReleaseToken(env("FIRMWARE_CLI_JWT_SECRET"), accessToken(req), UPLOAD_AUDIENCE);
    if (!claims) throw new HttpError(401, "Invalid or expired release token");
    if (typeof body.id !== "string" || !ID.test(body.id)) throw new HttpError(400, "Invalid id");

    const { data, error } = await serviceClient()
      .from("firmware_releases")
      .select("status, door_state, door_error")
      .eq("id", body.id)
      .eq("uploaded_by", claims.sub) // only your own builds
      .maybeSingle();
    if (error) throw new Error(error.message);
    if (!data) throw new HttpError(404, "No such build of yours");
    return data;
  })
);
