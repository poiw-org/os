// Issues and renews door keys. A key is a row in public.keys plus an HS256 JWT that is written
// to the NFC tag; the door verifies it offline with the same secret (JWT_SIGNING_KEY).
//
// POST { action: "issue", user_id, serial_number, not_before, expires_at } -> { id }
// POST { action: "renew", id, not_before, expires_at }                      -> { id }
//
// Auth: the caller's Supabase access token, which must belong to a row in public.admins.
// verify_jwt is off at the gateway because that check is done here.
import { createClient } from 'npm:@supabase/supabase-js@2';
import { SignJWT } from 'npm:jose@5';

const corsHeaders = {
	'Access-Control-Allow-Origin': '*',
	'Access-Control-Allow-Headers': 'authorization, x-client-info, apikey, content-type',
	'Access-Control-Allow-Methods': 'POST, OPTIONS'
};

class HttpError extends Error {
	constructor(
		public status: number,
		message: string
	) {
		super(message);
	}
}

const json = (body: unknown, status = 200) =>
	new Response(JSON.stringify(body), {
		status,
		headers: { ...corsHeaders, 'Content-Type': 'application/json' }
	});

const env = (name: string) => {
	const value = Deno.env.get(name);
	if (!value) throw new HttpError(500, `${name} is not configured`);
	return value;
};

const admin = () =>
	createClient(env('SUPABASE_URL'), env('SUPABASE_SERVICE_ROLE_KEY'), {
		auth: { persistSession: false }
	});

async function requireAdmin(req: Request, db: ReturnType<typeof admin>) {
	const token = req.headers.get('Authorization')?.replace(/^Bearer\s+/i, '');
	if (!token) throw new HttpError(401, 'Missing access token');

	const { data, error } = await db.auth.getUser(token);
	if (error || !data.user) throw new HttpError(401, 'Invalid access token');

	const { data: row, error: adminError } = await db
		.from('admins')
		.select('user_id')
		.eq('user_id', data.user.id)
		.maybeSingle();
	if (adminError) throw new HttpError(500, 'Could not check permissions');
	if (!row) throw new HttpError(403, 'Not an admin');

	return data.user.id;
}

function parseDate(value: unknown, field: string) {
	const date = typeof value === 'string' ? new Date(value) : undefined;
	if (!date || Number.isNaN(date.getTime())) throw new HttpError(400, `${field} must be an ISO date`);
	return date;
}

function parseRange(body: Record<string, unknown>) {
	const notBefore = parseDate(body.not_before, 'not_before');
	const expiresAt = parseDate(body.expires_at, 'expires_at');
	if (expiresAt <= notBefore) throw new HttpError(400, 'expires_at must be after not_before');
	return { notBefore, expiresAt };
}

const seconds = (date: Date) => Math.floor(date.getTime() / 1000);

// Claims are what the door firmware reads: sub = user, jti = key id, aud = tag serial number.
async function signKey(key: {
	id: string;
	userId: string;
	serialNumber: string;
	issuedAt: Date;
	notBefore: Date;
	expiresAt: Date;
}) {
	return await new SignJWT({})
		.setProtectedHeader({ alg: 'HS256', typ: 'JWT' })
		.setSubject(key.userId)
		.setJti(key.id)
		.setAudience(key.serialNumber)
		.setIssuedAt(seconds(key.issuedAt))
		.setNotBefore(seconds(key.notBefore))
		.setExpirationTime(seconds(key.expiresAt))
		.sign(new TextEncoder().encode(env('JWT_SIGNING_KEY')));
}

async function issue(db: ReturnType<typeof admin>, callerId: string, body: Record<string, unknown>) {
	const userId = typeof body.user_id === 'string' ? body.user_id : '';
	const serialNumber = typeof body.serial_number === 'string' ? body.serial_number.trim() : '';
	if (!userId) throw new HttpError(400, 'user_id is required');
	if (!serialNumber || serialNumber.length > 64) throw new HttpError(400, 'serial_number is invalid');
	const { notBefore, expiresAt } = parseRange(body);

	const { data: profile } = await db
		.from('profiles')
		.select('id, blocked')
		.eq('id', userId)
		.maybeSingle();
	if (!profile) throw new HttpError(404, 'User not found');
	if (profile.blocked) throw new HttpError(422, 'User is blocked');

	const id = crypto.randomUUID();
	const issuedAt = new Date();
	const jwt = await signKey({ id, userId, serialNumber, issuedAt, notBefore, expiresAt });

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

	return { id };
}

async function renew(db: ReturnType<typeof admin>, body: Record<string, unknown>) {
	const id = typeof body.id === 'string' ? body.id : '';
	if (!id) throw new HttpError(400, 'id is required');
	const { notBefore, expiresAt } = parseRange(body);

	const { data: key } = await db
		.from('keys')
		.select('id, user_id, serial_number, issued_at')
		.eq('id', id)
		.maybeSingle();
	if (!key) throw new HttpError(404, 'Key not found');

	const jwt = await signKey({
		id: key.id,
		userId: key.user_id,
		serialNumber: key.serial_number,
		issuedAt: new Date(key.issued_at),
		notBefore,
		expiresAt
	});

	const { error } = await db
		.from('keys')
		.update({ not_before: notBefore.toISOString(), expires_at: expiresAt.toISOString(), jwt })
		.eq('id', id);
	if (error) throw new HttpError(500, 'Could not renew the key');

	return { id };
}

Deno.serve(async (req) => {
	if (req.method === 'OPTIONS') return new Response(null, { status: 204, headers: corsHeaders });
	if (req.method !== 'POST') return json({ error: 'Method not allowed' }, 405);

	try {
		const db = admin();
		const callerId = await requireAdmin(req, db);

		let body: Record<string, unknown>;
		try {
			body = await req.json();
		} catch {
			throw new HttpError(400, 'Body must be JSON');
		}

		if (body.action === 'issue') return json(await issue(db, callerId, body));
		if (body.action === 'renew') return json(await renew(db, body));
		throw new HttpError(400, 'Unknown action');
	} catch (e) {
		if (e instanceof HttpError) return json({ error: e.message }, e.status);
		console.error(e);
		return json({ error: 'Internal error' }, 500);
	}
});
