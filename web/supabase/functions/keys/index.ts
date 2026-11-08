// Issues and renews door keys. A key is a row in public.keys plus an RS256 JWT (a "v2" key) that is
// written to the NFC tag. The door verifies it offline with the public key it fetches from the
// `door-key` function. The key pair is derived from the RSA_KEY_SEED secret on every signature.
// Older HS256 keys stay valid on the door; nothing here issues them any more.
//
// POST { action: "issue", user_id, serial_number, not_before, expires_at } -> { id }
// POST { action: "issue", user_id, virtual: true, not_before, expires_at }  -> { id }   (no tag)
// POST { action: "renew", id, not_before, expires_at }                      -> { id }
// POST { action: "reset_second_factors", user_id }                          -> { removed }
//
// Issuing to a member with no passkey or authenticator app also needs acknowledge_no_second_factor:
// true, because that key will be suspended 14 days later unless the member registers one.
//
// Auth: the caller's Supabase access token, which must belong to a row in public.admins.
// verify_jwt is off at the gateway because that check is done here.
import { Buffer } from 'node:buffer';
import { signRs256 } from '../_shared/rsa.ts';
import {
	accessToken,
	callerId,
	env,
	HttpError,
	handlePost,
	serviceClient
} from '../_shared/http.ts';

type Db = ReturnType<typeof serviceClient>;

async function requireAdmin(req: Request, db: Db) {
	const id = await callerId(db, accessToken(req));

	const { data: row, error } = await db
		.from('admins')
		.select('user_id')
		.eq('user_id', id)
		.maybeSingle();
	if (error) throw new HttpError(500, 'Could not check permissions');
	if (!row) throw new HttpError(403, 'Not an admin');

	return id;
}

function parseDate(value: unknown, field: string) {
	const date = typeof value === 'string' ? new Date(value) : undefined;
	if (!date || Number.isNaN(date.getTime()))
		throw new HttpError(400, `${field} must be an ISO date`);
	return date;
}

function parseRange(body: Record<string, unknown>) {
	const notBefore = parseDate(body.not_before, 'not_before');
	const expiresAt = parseDate(body.expires_at, 'expires_at');
	if (expiresAt <= notBefore) throw new HttpError(400, 'expires_at must be after not_before');
	return { notBefore, expiresAt };
}

const seconds = (date: Date) => Math.floor(date.getTime() / 1000);

const base64Url = (value: unknown) => Buffer.from(JSON.stringify(value)).toString('base64url');

// Claims are what the door firmware reads: sub = user, jti = key id, aud = tag serial number.
// A virtual key has no tag; its audience matches no serial number, so no tag can ever carry it.
async function signKey(key: {
	id: string;
	userId: string;
	serialNumber: string | null;
	issuedAt: Date;
	notBefore: Date;
	expiresAt: Date;
}) {
	const signed = `${base64Url({ alg: 'RS256', typ: 'JWT' })}.${base64Url({
		sub: key.userId,
		jti: key.id,
		aud: key.serialNumber ?? 'virtual',
		iat: seconds(key.issuedAt),
		nbf: seconds(key.notBefore),
		exp: seconds(key.expiresAt)
	})}`;
	return `${signed}.${await signRs256(env('RSA_KEY_SEED'), signed)}`;
}

async function issue(db: Db, callerId: string, body: Record<string, unknown>) {
	const userId = typeof body.user_id === 'string' ? body.user_id : '';
	const serialNumber =
		body.virtual === true
			? null
			: typeof body.serial_number === 'string'
				? body.serial_number.trim()
				: '';
	if (!userId) throw new HttpError(400, 'user_id is required');
	if (serialNumber !== null && (!serialNumber || serialNumber.length > 64))
		throw new HttpError(400, 'serial_number is invalid');
	const { notBefore, expiresAt } = parseRange(body);

	const { data: profile } = await db
		.from('profiles')
		.select('id, blocked')
		.eq('id', userId)
		.maybeSingle();
	if (!profile) throw new HttpError(404, 'User not found');
	if (profile.blocked) throw new HttpError(422, 'User is blocked');

	const { data: security, error: securityError } = await db.rpc('second_factor_status', {
		target: userId
	});
	if (securityError) throw new HttpError(500, 'Could not check the second factor');
	if (!security?.email_verified) throw new HttpError(422, 'This user has not verified their email');
	if (!security?.has_factor && body.acknowledge_no_second_factor !== true)
		throw new HttpError(428, 'Acknowledge that this user has no second factor');

	const id = crypto.randomUUID();
	const issuedAt = new Date();
	const jwt = await signKey({
		id,
		userId,
		serialNumber,
		issuedAt,
		notBefore,
		expiresAt
	});

	const { error } = await db.from('keys').insert({
		id,
		user_id: userId,
		serial_number: serialNumber,
		not_before: notBefore.toISOString(),
		expires_at: expiresAt.toISOString(),
		issued_at: issuedAt.toISOString(),
		issued_by: callerId,
		jwt
	});
	if (error?.code === '23505') {
		throw new HttpError(409, 'This tag is already bound to a key');
	}
	if (error) throw new HttpError(500, 'Could not save the key');

	// The 14 days the admin just acknowledged start now, not at the next daily run.
	if (!security?.has_factor) {
		const { error: clockError } = await db
			.from('profiles')
			.update({ security_required_since: issuedAt.toISOString() })
			.eq('id', userId)
			.is('security_required_since', null);
		if (clockError) console.error(clockError);
	}

	return { id };
}

// For a member who lost their device. They get a fresh 14 days to register a new factor.
async function resetSecondFactors(db: Db, body: Record<string, unknown>) {
	const userId = typeof body.user_id === 'string' ? body.user_id : '';
	if (!userId) throw new HttpError(400, 'user_id is required');

	const { data: factors, error: factorsError } = await db.auth.admin.mfa.listFactors({ userId });
	if (factorsError) throw new HttpError(500, 'Could not read the factors');
	const { data: passkeys, error: passkeysError } = await db.auth.admin.passkey.listPasskeys({
		userId
	});
	if (passkeysError) throw new HttpError(500, 'Could not read the passkeys');

	for (const factor of factors.factors) {
		const { error } = await db.auth.admin.mfa.deleteFactor({ userId, id: factor.id });
		if (error) throw new HttpError(500, 'Could not remove a factor');
	}
	for (const passkey of passkeys ?? []) {
		const { error } = await db.auth.admin.passkey.deletePasskey({ userId, passkeyId: passkey.id });
		if (error) throw new HttpError(500, 'Could not remove a passkey');
	}

	const { error } = await db
		.from('profiles')
		.update({ security_required_since: null, security_reminded_at: null })
		.eq('id', userId);
	if (error) throw new HttpError(500, 'Could not reset the deadline');

	return { removed: factors.factors.length + (passkeys?.length ?? 0) };
}

async function renew(db: Db, callerId: string, body: Record<string, unknown>) {
	const id = typeof body.id === 'string' ? body.id : '';
	if (!id) throw new HttpError(400, 'id is required');
	const { notBefore, expiresAt } = parseRange(body);

	const { data: key } = await db
		.from('keys')
		.select('id, user_id, serial_number, issued_at, expires_at, disabled')
		.eq('id', id)
		.maybeSingle();
	if (!key) throw new HttpError(404, 'Key not found');
	if (key.disabled) throw new HttpError(409, 'This key is disabled');

	// The token already on the tag keeps the exp it was signed with, and the door verifies it
	// offline. An earlier date would only change the database copy (and drop a disabled key off the
	// door's blacklist early), so ending access sooner is done by disabling the key.
	const currentExpiry = new Date(key.expires_at);
	if (expiresAt < currentExpiry)
		throw new HttpError(
			409,
			"A key's expiry cannot be moved earlier. Disable the key to end access sooner."
		);

	const { data: profile } = await db
		.from('profiles')
		.select('blocked')
		.eq('id', key.user_id)
		.maybeSingle();
	if (!profile) throw new HttpError(404, 'User not found');
	if (profile.blocked) throw new HttpError(422, 'User is blocked');

	const jwt = await signKey({
		id: key.id,
		userId: key.user_id,
		serialNumber: key.serial_number,
		issuedAt: new Date(key.issued_at),
		notBefore,
		expiresAt
	});

	// Conditional on the row being unchanged since it was read, so a concurrent renew or disable
	// cannot slip past the checks above. The database also refuses an earlier expires_at.
	const { data: updated, error } = await db
		.from('keys')
		.update({
			not_before: notBefore.toISOString(),
			expires_at: expiresAt.toISOString(),
			jwt,
			renewed_by: callerId // the activity log's actor: the service role has no auth.uid()
		})
		.eq('id', id)
		.eq('disabled', false)
		.lte('expires_at', expiresAt.toISOString())
		.select('id');
	if (error) throw new HttpError(500, 'Could not renew the key');
	if (!updated?.length) throw new HttpError(409, 'The key changed meanwhile; reload and try again');

	return { id };
}

Deno.serve((req) =>
	handlePost(req, async (body) => {
		const db = serviceClient();
		const callerId = await requireAdmin(req, db);

		if (body.action === 'issue') return await issue(db, callerId, body);
		if (body.action === 'renew') return await renew(db, callerId, body);
		if (body.action === 'reset_second_factors') return await resetSecondFactors(db, body);
		throw new HttpError(400, 'Unknown action');
	})
);
