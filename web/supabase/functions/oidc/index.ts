// Sits in front of Supabase's OAuth token endpoint so the ID token can carry the user's roles, which
// Supabase's own ID token cannot (its claims are fixed). Apps use these two URLs in place of
// Supabase's Token and JWKS URLs; the authorize endpoint, consent and userinfo stay Supabase's.
//
// POST /oidc/token  the app's token request, passed on unchanged -> Supabase's reply, with id_token
//                   re-signed and a `roles` claim from public.user_roles
// GET  /oidc/jwks   the key that signs those ID tokens
//
// Auth: none of its own. Supabase checks the client, its secret, the code and PKCE; this only ever
// rewrites an ID token Supabase has just issued. verify_jwt is off at the gateway: apps send no
// Supabase JWT.
import { env, serviceClient } from '../_shared/http.ts';
import { jwks, reissueIdToken } from '../_shared/oidc.ts';

type Db = ReturnType<typeof serviceClient>;

// The roles the database gives this user (public.user_roles): "admin", "lab-access".
async function rolesOf(db: Db, userId: string) {
	const { data, error } = await db.from('user_roles').select('role').eq('user_id', userId);
	if (error) throw new Error(error.message);
	return data.map((row) => row.role as string).sort();
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
			if (!upstream.ok || typeof reply.id_token !== 'string') return json(reply, upstream.status);

			const db = serviceClient();
			reply.id_token = await reissueIdToken(
				seed,
				reply.id_token,
				`${supabaseUrl}/functions/v1/oidc`,
				(userId) => rolesOf(db, userId)
			);
			return json(reply);
		}

		return json({ error: 'not_found' }, 404);
	} catch (e) {
		console.error(e);
		return json({ error: 'server_error' }, 500);
	}
});
