// Gives the phone app its BLE credential: the virtual key's JWT, naming the phone's public key.
// The first call binds the key to that phone; later calls from the same phone return the current
// credential, which is how the app picks up a renewal. See docs/ble-protocol.md, section 1.
//
// POST { key_id, device_pubkey, device_name } -> { jwt, not_before, expires_at }
//
// Auth: the caller's Supabase access token. The caller must own the key; this function checks that
// itself because verify_jwt is off at the gateway. The browser has no grant on the device columns.
import { parseDeviceName, parseDevicePubkey } from "../_shared/device.ts";
import {
  accessToken,
  callerId,
  env,
  HttpError,
  handlePost,
  serviceClient,
} from "../_shared/http.ts";
import { peekClaims, seconds, signKey } from "../_shared/keys.ts";

type Db = ReturnType<typeof serviceClient>;

const COLUMNS =
  "id, user_id, serial_number, issued_at, not_before, expires_at, disabled, device_pubkey, jwt";

type Row = {
  id: string;
  user_id: string;
  serial_number: string | null;
  issued_at: string;
  not_before: string;
  expires_at: string;
  disabled: boolean;
  device_pubkey: string | null;
  jwt: string | null;
};

const credentialOf = (row: Row, jwt: string) => ({
  jwt,
  not_before: new Date(row.not_before).toISOString(),
  expires_at: new Date(row.expires_at).toISOString(),
});

const sign = (row: Row, devicePubkey: string) =>
  signKey(env("RSA_KEY_SEED"), {
    id: row.id,
    userId: row.user_id,
    serialNumber: null,
    issuedAt: new Date(row.issued_at),
    notBefore: new Date(row.not_before),
    expiresAt: new Date(row.expires_at),
    devicePubkey,
  });

// The stored token is still the right one when it says what the row says: a renew re-signs and
// stores it, but a token written by an older version of this code, or by hand, is not trusted.
function storedMatches(row: Row, devicePubkey: string) {
  if (!row.jwt) return false;
  const c = peekClaims(row.jwt);
  return (
    c.aud === "ble" &&
    c.cnf === devicePubkey &&
    c.sub === row.user_id &&
    c.jti === row.id &&
    c.nbf === seconds(new Date(row.not_before)) &&
    c.exp === seconds(new Date(row.expires_at)) &&
    c.iat === seconds(new Date(row.issued_at))
  );
}

async function credential(db: Db, caller: string, body: Record<string, unknown>) {
  const keyId = typeof body.key_id === "string" ? body.key_id : "";
  if (!keyId) throw new HttpError(400, "key_id is required");
  const devicePubkey = await parseDevicePubkey(body.device_pubkey);
  if (!devicePubkey) throw new HttpError(400, "device_pubkey is not a valid P-256 public key");
  const deviceName = parseDeviceName(body.device_name);
  if (!deviceName) throw new HttpError(400, "device_name must be 1 to 64 characters");

  // A key that is not the caller's looks the same as one that does not exist.
  const { data: row, error } = await db
    .from("keys")
    .select(COLUMNS)
    .eq("id", keyId)
    .eq("user_id", caller)
    .maybeSingle<Row>();
  if (error) throw new HttpError(500, "Could not read the key");
  if (!row) throw new HttpError(404, "Key not found");
  if (row.serial_number !== null)
    throw new HttpError(422, "This key is for a tag, not a phone");
  if (row.disabled) throw new HttpError(403, "This key has been disabled");

  const { data: profile } = await db
    .from("profiles")
    .select("blocked")
    .eq("id", caller)
    .maybeSingle();
  if (!profile || profile.blocked) throw new HttpError(403, "Your account is blocked");
  const { data: usable, error: usableError } = await db.rpc("can_use_app_as", { actor: caller });
  if (usableError) throw new HttpError(500, "Could not check the key");
  if (usable !== true)
    throw new HttpError(403, "This key isn't active right now. Check its dates with an admin.");

  const anotherPhone = new HttpError(409, "This key is set up on another phone");

  if (row.device_pubkey) {
    if (row.device_pubkey !== devicePubkey) throw anotherPhone;
    if (storedMatches(row, devicePubkey)) return credentialOf(row, row.jwt!);
    // The row moved on (a renew by an older version, say): sign again and keep it.
    const jwt = await sign(row, devicePubkey);
    const { error: saveError } = await db.from("keys").update({ jwt }).eq("id", row.id);
    if (saveError) throw new HttpError(500, "Could not save the credential");
    return credentialOf(row, jwt);
  }

  // Bind. The conditional update makes two phones racing for one key a clean 409 for the loser.
  const jwt = await sign(row, devicePubkey);
  const { data: bound, error: bindError } = await db
    .from("keys")
    .update({
      device_pubkey: devicePubkey,
      device_name: deviceName,
      device_bound_at: new Date().toISOString(),
      jwt,
    })
    .eq("id", row.id)
    .is("device_pubkey", null)
    .eq("disabled", false)
    .select("id");
  if (bindError?.code === "23505") throw anotherPhone; // this phone already has another key
  if (bindError) throw new HttpError(500, "Could not set up this phone");
  if (!bound?.length) {
    // Someone bound it, or disabled it, between the read and the write.
    const { data: now } = await db.from("keys").select(COLUMNS).eq("id", row.id).maybeSingle<Row>();
    if (now?.disabled) throw new HttpError(403, "This key has been disabled");
    if (now?.device_pubkey === devicePubkey && now.jwt) return credentialOf(now, now.jwt);
    throw anotherPhone;
  }
  return credentialOf(row, jwt);
}

Deno.serve((req) =>
  handlePost(req, async (body) => {
    const db = serviceClient();
    const caller = await callerId(db, accessToken(req));
    return await credential(db, caller, body);
  }),
);
