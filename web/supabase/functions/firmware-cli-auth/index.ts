// Lets `make release` upload on someone's behalf, in the style of OAuth for native apps. Only admins
// and members with an active key can grant it; admins must also be signed in with 2FA.
//
// POST { action: "authorize", challenge }       as that signed-in user  ->  { code }
// POST { action: "token", code, verifier }      from the CLI  ->  { token, expires_at }
//
// The portal hands the code only to a loopback address, and the code is useless without the verifier
// the CLI generated, so a forged consent link cannot get a token out to anyone else.
import { createClient } from "npm:@supabase/supabase-js@2";
import { accessToken, callerId, env, HttpError, handlePost, serviceClient } from "../_shared/http.ts";
import {
  challengeOf,
  CODE_AUDIENCE,
  CODE_SECONDS,
  isAdmin,
  mayRelease,
  NOT_ALLOWED,
  signReleaseToken,
  UPLOAD_AUDIENCE,
  UPLOAD_SECONDS,
  verifyReleaseToken,
} from "../_shared/release-token.ts";

const CHALLENGE = /^[\w-]{43}$/;

// Asked as the caller: only the database knows about passkey and email-code sessions, which can
// stay aal1 in the token even though they passed the second step.
async function sessionHasSecondFactor(token: string) {
  const asCaller = createClient(env("SUPABASE_URL"), env("SUPABASE_ANON_KEY"), {
    auth: { persistSession: false },
    global: { headers: { Authorization: `Bearer ${token}` } },
  });
  const { data, error } = await asCaller.rpc("session_has_second_factor");
  if (error) throw new Error(error.message);
  return data === true;
}

Deno.serve((req) =>
  handlePost(req, async (body) => {
    const secret = env("FIRMWARE_CLI_JWT_SECRET");
    const db = serviceClient();

    if (body.action === "authorize") {
      const token = accessToken(req);
      const user = await callerId(db, token); // validates the session with Supabase first
      if (!(await mayRelease(db, user))) throw new HttpError(403, NOT_ALLOWED);
      if ((await isAdmin(db, user)) && !(await sessionHasSecondFactor(token)))
        throw new HttpError(403, "Admins must be signed in with 2FA to allow this");
      if (typeof body.challenge !== "string" || !CHALLENGE.test(body.challenge))
        throw new HttpError(400, "Invalid challenge");
      const code = await signReleaseToken(
        secret,
        { sub: user, aud: CODE_AUDIENCE, challenge: body.challenge },
        CODE_SECONDS,
      );
      return { code };
    }

    if (body.action === "token") {
      const claims = await verifyReleaseToken(secret, String(body.code ?? ""), CODE_AUDIENCE);
      if (!claims || claims.challenge !== (await challengeOf(String(body.verifier ?? ""))))
        throw new HttpError(403, "Invalid or expired code");
      if (!(await mayRelease(db, claims.sub))) throw new HttpError(403, NOT_ALLOWED);
      const token = await signReleaseToken(secret, { sub: claims.sub, aud: UPLOAD_AUDIENCE }, UPLOAD_SECONDS);
      return { token, expires_at: Math.floor(Date.now() / 1000) + UPLOAD_SECONDS };
    }

    throw new HttpError(400, "Unknown action");
  })
);
