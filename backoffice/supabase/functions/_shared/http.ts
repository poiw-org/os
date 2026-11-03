// Plumbing shared by the browser-facing edge functions: CORS, JSON replies, errors, the
// service-role client and the caller's access token. verify_jwt is off at the gateway for these
// functions because each one authorises the caller itself.
import { createClient } from 'npm:@supabase/supabase-js@2';

const corsHeaders = {
	'Access-Control-Allow-Origin': '*',
	'Access-Control-Allow-Headers': 'authorization, x-client-info, apikey, content-type',
	'Access-Control-Allow-Methods': 'POST, OPTIONS'
};

export class HttpError extends Error {
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

export const env = (name: string) => {
	const value = Deno.env.get(name);
	if (!value) throw new HttpError(500, `${name} is not configured`);
	return value;
};

export const serviceClient = () =>
	createClient(env('SUPABASE_URL'), env('SUPABASE_SERVICE_ROLE_KEY'), {
		auth: { persistSession: false }
	});

export function accessToken(req: Request) {
	const token = req.headers.get('Authorization')?.replace(/^Bearer\s+/i, '');
	if (!token) throw new HttpError(401, 'Missing access token');
	return token;
}

export async function callerId(db: ReturnType<typeof serviceClient>, token: string) {
	const { data, error } = await db.auth.getUser(token);
	if (error || !data.user) throw new HttpError(401, 'Invalid access token');
	return data.user.id;
}

/** Answers CORS preflights and non-POSTs, runs `handler`, and turns its result or error into JSON. */
export async function handlePost(req: Request, handler: (body: Record<string, unknown>) => Promise<unknown>) {
	if (req.method === 'OPTIONS') return new Response(null, { status: 204, headers: corsHeaders });
	if (req.method !== 'POST') return json({ error: 'Method not allowed' }, 405);

	try {
		let body: Record<string, unknown>;
		try {
			body = await req.json();
		} catch {
			body = {};
		}
		return json(await handler(body));
	} catch (e) {
		if (e instanceof HttpError) return json({ error: e.message }, e.status);
		console.error(e);
		return json({ error: 'Internal error' }, 500);
	}
}
