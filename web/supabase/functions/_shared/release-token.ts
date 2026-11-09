// HS256 tokens for `make release`, signed with FIRMWARE_CLI_JWT_SECRET. They are not Supabase
// sessions and open nothing but firmware-upload. `aud` keeps the two kinds apart: a code is only
// good for exchanging, a token only for uploading.
export const CODE_AUDIENCE = "doora-release-code";
export const UPLOAD_AUDIENCE = "doora-firmware-upload";
export const CODE_SECONDS = 120;
export const UPLOAD_SECONDS = 30 * 24 * 3600;

type Claims = { sub: string; aud: string; exp: number; challenge?: string };

const HEADER = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9"; // {"alg":"HS256","typ":"JWT"}

const base64Url = (bytes: Uint8Array) =>
  btoa(String.fromCharCode(...bytes))
    .replace(/\+/g, "-")
    .replace(/\//g, "_")
    .replace(/=+$/, "");

const fromBase64Url = (text: string) =>
  Uint8Array.from(atob(text.replace(/-/g, "+").replace(/_/g, "/")), (c) => c.charCodeAt(0));

const hmacKey = (secret: string) =>
  crypto.subtle.importKey(
    "raw",
    new TextEncoder().encode(secret),
    { name: "HMAC", hash: "SHA-256" },
    false,
    ["sign", "verify"],
  );

export async function signReleaseToken(
  secret: string,
  claims: Omit<Claims, "exp">,
  seconds: number,
  now = Date.now() / 1000,
) {
  const payload = base64Url(
    new TextEncoder().encode(JSON.stringify({ ...claims, exp: Math.floor(now) + seconds })),
  );
  const body = `${HEADER}.${payload}`;
  const signature = await crypto.subtle.sign("HMAC", await hmacKey(secret), new TextEncoder().encode(body));
  return `${body}.${base64Url(new Uint8Array(signature))}`;
}

/** The claims of a valid, unexpired token for `audience`, or null. */
export async function verifyReleaseToken(
  secret: string,
  token: string,
  audience: string,
  now = Date.now() / 1000,
): Promise<Claims | null> {
  const [header, payload, signature, extra] = token.split(".");
  if (header !== HEADER || !payload || !signature || extra !== undefined) return null;
  try {
    const valid = await crypto.subtle.verify(
      "HMAC",
      await hmacKey(secret),
      fromBase64Url(signature),
      new TextEncoder().encode(`${header}.${payload}`),
    );
    if (!valid) return null;
    const claims = JSON.parse(new TextDecoder().decode(fromBase64Url(payload))) as Claims;
    return claims.aud === audience && claims.exp > now && claims.sub ? claims : null;
  } catch {
    return null;
  }
}

/** PKCE: the code is bound to a hash of a secret only the waiting CLI holds. */
export const challengeOf = async (verifier: string) =>
  base64Url(new Uint8Array(await crypto.subtle.digest("SHA-256", new TextEncoder().encode(verifier))));

export const NOT_ALLOWED = "Only admins and members with an active key can upload firmware";

/** Admins and members with an active key. Approving a build stays with admins. */
export async function mayRelease(
  // deno-lint-ignore no-explicit-any
  db: any,
  userId: string,
) {
  const { data, error } = await db.rpc("may_release_firmware", { subject: userId });
  if (error) throw new Error(error.message);
  return data === true;
}

export async function isAdmin(
  // deno-lint-ignore no-explicit-any
  db: any,
  userId: string,
) {
  const { data, error } = await db.from("admins").select("user_id").eq("user_id", userId).maybeSingle();
  if (error) throw new Error(error.message);
  return data !== null;
}
