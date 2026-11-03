// Tells a door the public key that verifies v2 key tokens.
//
// GET -> { "n": "<base64url 256-byte RSA modulus>" }   (the exponent is always 65537)
//
// Auth: the door's Supabase access token, which must belong to a row in public.door_users.
// verify_jwt is off at the gateway because that check is done here. The public key is not secret,
// but only doors and the people running them have a reason to ask for it.
import { createClient } from 'npm:@supabase/supabase-js@2';
import { publicModulus } from '../_shared/rsa.ts';

const headers = { 'Content-Type': 'application/json' };
const json = (body: unknown, status = 200) => new Response(JSON.stringify(body), { status, headers });

Deno.serve(async (req) => {
	if (req.method !== 'GET') return json({ error: 'Method not allowed' }, 405);

	try {
		const url = Deno.env.get('SUPABASE_URL');
		const serviceKey = Deno.env.get('SUPABASE_SERVICE_ROLE_KEY');
		const seed = Deno.env.get('RSA_KEY_SEED');
		if (!url || !serviceKey || !seed) return json({ error: 'Not configured' }, 500);
		const db = createClient(url, serviceKey, { auth: { persistSession: false } });

		const token = req.headers.get('Authorization')?.replace(/^Bearer\s+/i, '');
		if (!token) return json({ error: 'Missing access token' }, 401);
		const { data, error } = await db.auth.getUser(token);
		if (error || !data.user) return json({ error: 'Invalid access token' }, 401);

		const { data: door, error: doorError } = await db
			.from('door_users')
			.select('user_id')
			.eq('user_id', data.user.id)
			.maybeSingle();
		if (doorError) return json({ error: 'Could not check permissions' }, 500);
		if (!door) return json({ error: 'Not a door' }, 403);

		return json({ n: publicModulus(seed) });
	} catch (e) {
		console.error(e);
		return json({ error: 'Internal error' }, 500);
	}
});
