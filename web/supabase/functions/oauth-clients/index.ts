// Admin only. Registers the apps that may sign people in with their doora account through the
// Supabase OAuth 2.1 server (Cloudflare Access, for one). The admin API needs the service role, so
// the console asks this function instead of Supabase directly.
//
// POST { action: "list" }                                                   -> { clients }
// POST { action: "create", client_name, redirect_uris, token_endpoint_auth_method } -> { client }
// POST { action: "update", client_id, client_name, redirect_uris }          -> { client }
// POST { action: "regenerate_secret", client_id }                           -> { client }
// POST { action: "delete", client_id }                                      -> {}
//
// A client's secret is only ever in the reply to create and regenerate_secret.
//
// Auth: the caller's Supabase access token, which must belong to an admin signed in with 2FA.
// verify_jwt is off at the gateway because that check is done here.
import { HttpError, handlePost, requireAdmin, serviceClient } from '../_shared/http.ts';

const AUTH_METHODS = ['none', 'client_secret_basic', 'client_secret_post'] as const;
const UUID = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/;
const LOOPBACK = new Set(['localhost', '127.0.0.1', '[::1]']);

function parseName(value: unknown) {
	const name = typeof value === 'string' ? value.trim() : '';
	if (name.length < 1 || name.length > 100)
		throw new HttpError(400, 'Name must be 1 to 100 characters');
	return name;
}

// Supabase matches redirect URIs exactly, so each one is a full https URL (or a loopback one, for
// native apps) with no fragment.
function parseRedirectUris(value: unknown) {
	if (!Array.isArray(value) || value.length < 1 || value.length > 10)
		throw new HttpError(400, 'Give 1 to 10 redirect URIs');
	return value.map((raw) => {
		let url: URL;
		try {
			url = new URL(String(raw).trim());
		} catch {
			throw new HttpError(400, `Not a URL: ${raw}`);
		}
		const secure =
			url.protocol === 'https:' || (url.protocol === 'http:' && LOOPBACK.has(url.hostname));
		if (!secure) throw new HttpError(400, `Redirect URIs must use https: ${raw}`);
		if (url.hash) throw new HttpError(400, `Redirect URIs cannot have a #fragment: ${raw}`);
		return url.href;
	});
}

function parseClientId(value: unknown) {
	if (typeof value !== 'string' || !UUID.test(value)) throw new HttpError(400, 'Invalid client_id');
	return value;
}

// The admin API answers with { data, error }; its message is the one worth showing.
function unwrap<T>({
	data,
	error
}: {
	data: T;
	error: { message: string; status?: number } | null;
}) {
	if (error)
		throw new HttpError(error.status && error.status < 500 ? error.status : 502, error.message);
	return data;
}

Deno.serve((req) =>
	handlePost(req, async (body) => {
		const db = serviceClient();
		await requireAdmin(req, db);
		const oauth = db.auth.admin.oauth;

		if (body.action === 'list') {
			// shortcut: one page of 100; page through listClients if a door ever has more apps.
			const { clients } = unwrap(await oauth.listClients({ page: 1, perPage: 100 }));
			return { clients };
		}

		if (body.action === 'create') {
			const method = AUTH_METHODS.find((m) => m === body.token_endpoint_auth_method);
			if (!method) throw new HttpError(400, 'Invalid token_endpoint_auth_method');
			const client = unwrap(
				await oauth.createClient({
					client_name: parseName(body.client_name),
					redirect_uris: parseRedirectUris(body.redirect_uris),
					token_endpoint_auth_method: method
				})
			);
			return { client };
		}

		if (body.action === 'update') {
			const client = unwrap(
				await oauth.updateClient(parseClientId(body.client_id), {
					client_name: parseName(body.client_name),
					redirect_uris: parseRedirectUris(body.redirect_uris)
				})
			);
			return { client };
		}

		if (body.action === 'regenerate_secret') {
			const client = unwrap(await oauth.regenerateClientSecret(parseClientId(body.client_id)));
			return { client };
		}

		if (body.action === 'delete') {
			unwrap(await oauth.deleteClient(parseClientId(body.client_id)));
			return {};
		}

		throw new HttpError(400, 'Unknown action');
	})
);
