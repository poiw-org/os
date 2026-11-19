// Sits in front of Supabase's OAuth token endpoint so the ID token can carry the user's roles, which
// Supabase's own ID token cannot (its claims are fixed). Apps use these two URLs in place of
// Supabase's Token and JWKS URLs; the authorize endpoint, consent and userinfo stay Supabase's.
//
// POST /oidc/token  the app's token request, passed on unchanged -> Supabase's reply, with id_token
//                   re-signed and a `roles` claim from public.user_roles. For an app in
//                   public.lab_access_apps, someone without lab-access gets invalid_grant instead,
//                   on sign-in and on every refresh.
// GET  /oidc/jwks   the key that signs those ID tokens
//
// Auth: none of its own. Supabase checks the client, its secret, the code and PKCE; this only ever
// rewrites an ID token Supabase has just issued. verify_jwt is off at the gateway: apps send no
// Supabase JWT.
import { env, serviceClient } from '../_shared/http.ts';
import { claimsOf, jwks, reissueIdToken } from '../_shared/oidc.ts';

type Db = ReturnType<typeof serviceClient>;

// The roles the database gives this user (public.user_roles): "admin", "lab-access".
async function rolesOf(db: Db, userId: string) {
	const { data, error } = await db.from('user_roles').select('role').eq('user_id', userId);
	if (error) throw new Error(error.message);
	return data.map((row) => row.role as string).sort();
}

async function labOnly(db: Db, clientId: string) {
	const { data, error } = await db
		.from('lab_access_apps')
		.select('client_id')
		.eq('client_id', clientId)
		.maybeSingle();
	if (error) throw new Error(error.message);
	return data !== null;
}

const json = (body: unknown, status = 200) =>
	new Response(JSON.stringify(body), {
		status,
		headers: { 'Content-Type': 'application/json', 'Cache-Control': 'no-store' }
	});

Deno.serve(async (req) => {
	const path = new URL(req.url).pathname;
	try {
		const seed = env('OIDC_KEY_SEED');

		if (req.method === 'GET' && path.endsWith('/jwks')) return json(jwks(seed));

		if (req.method === 'POST' && path.endsWith('/token')) {
			const supabaseUrl = env('SUPABASE_URL');
			const headers = new Headers({ 'Content-Type': req.headers.get('Content-Type') ?? '' });
			const basic = req.headers.get('Authorization');
			if (basic) headers.set('Authorization', basic);
			const upstream = await fetch(`${supabaseUrl}/auth/v1/oauth/token`, {
				method: 'POST',
				headers,
				body: await req.text()
			});
			const reply = await upstream.json().catch(() => ({ error: 'server_error' }));
			// Errors (a bad code, a wrong secret) go back exactly as Supabase gave them.
			if (!upstream.ok) return json(reply, upstream.status);

			// Supabase's OAuth access token names the user and the app it was issued to; without them
			// the lab-access rule cannot be checked, so the token is not handed out.
			const { sub, client_id } = claimsOf(String(reply.access_token));
			if (typeof sub !== 'string' || typeof client_id !== 'string')
				throw new Error('Access token has no sub or client_id');
			const db = serviceClient();
			const roles = await rolesOf(db, sub);
			if (!roles.includes('lab-access') && (await labOnly(db, client_id)))
				return json(
					{
						error: 'invalid_grant',
						error_description: 'This app is only for people with an active key.'
					},
					400
				);

			if (typeof reply.id_token === 'string')
				reply.id_token = await reissueIdToken(
					seed,
					reply.id_token,
					`${supabaseUrl}/functions/v1/oidc`,
					async () => roles
				);
			return json(reply);
		}

		return json({ error: 'not_found' }, 404);
	} catch (e) {
		console.error(e);
		return json({ error: 'server_error' }, 500);
	}
});
