import { test as base, expect, type Page, type Route } from '@playwright/test';

const SUPABASE = 'https://yshwchmbsuafribrttlt.supabase.co';
// supabase-js keeps the session in localStorage under sb-<project ref>-auth-token.
const STORAGE_KEY = 'sb-yshwchmbsuafribrttlt-auth-token';
const ADMIN_TOKEN = 'test-admin-token';
const ADMIN_ID = '00000000-0000-4000-8000-0000000000aa';
export const PASSWORD = 'correct-password';

export const USERS = [
	{ id: 'a1111111-0000-4000-8000-000000000001', email: 'alice@example.test', blocked: false },
	{ id: 'b2222222-0000-4000-8000-000000000002', email: 'bob@example.test', blocked: false },
	// Has no keys: hidden on the home page until "Show users without keys".
	{ id: 'c3333333-0000-4000-8000-000000000003', email: 'carol@example.test', blocked: false }
];

type KeyRow = {
	id: string;
	user_id: string;
	serial_number: string;
	not_before: string;
	expires_at: string;
	issued_at: string;
	disabled: boolean;
	jwt: string;
	user: { id: string; email: string };
	issuer: { email: string } | null;
};

const keyRow = (id: string, user: (typeof USERS)[number], serialNumber: string): KeyRow => ({
	id,
	user_id: user.id,
	serial_number: serialNumber,
	not_before: '2026-01-01T00:00:00.000Z',
	expires_at: '2027-01-01T00:00:00.000Z',
	issued_at: '2025-12-20T09:30:00.000Z',
	disabled: false,
	jwt: 'eyJhbGciOiJub25lIn0.eyJ0ZXN0Ijp0cnVlfQ.signature',
	user: { id: user.id, email: user.email },
	issuer: { email: 'admin@example.test' }
});

export const KEY_IDS = {
	first: '0a1b2c3d-0000-4000-8000-000000000001',
	second: '9f8e7d6c-0000-4000-8000-000000000002',
	created: 'c3c3c3c3-0000-4000-8000-000000000003'
};

export const KEYS = [
	keyRow(KEY_IDS.first, USERS[0], '04A1B2C3D4E5F6'),
	keyRow(KEY_IDS.second, USERS[1], '04F6E5D4C3B2A1')
];

const ENTRIES = [
	{
		id: 1,
		key_id: KEY_IDS.first,
		type: 'successful',
		reason: null,
		created_at: '2026-02-01T08:15:00.000Z',
		metadata: {}
	},
	{
		id: 2,
		key_id: KEY_IDS.first,
		type: 'failed',
		reason: 'KeyExpired',
		created_at: '2026-02-02T18:40:00.000Z',
		metadata: { serialNumberFromTag: '04A1B2C3D4E5F6' }
	}
];

const adminUser = (email = 'tester@example.test') => ({
	id: ADMIN_ID,
	aud: 'authenticated',
	role: 'authenticated',
	email,
	app_metadata: {},
	user_metadata: {},
	created_at: '2021-06-06T13:07:05.772Z'
});

const sessionFor = (token: string, email: string) => ({
	access_token: token,
	refresh_token: 'test-refresh-token',
	token_type: 'bearer',
	expires_in: 3600,
	expires_at: 4102444800, // 2100-01-01
	user: adminUser(email)
});

async function seedSession(page: Page) {
	await page.addInitScript(
		([key, value]) => localStorage.setItem(key, value),
		[STORAGE_KEY, JSON.stringify(sessionFor(ADMIN_TOKEN, 'tester@example.test'))]
	);
}

export type ApiState = {
	keys: KeyRow[];
	calls: string[];
	/** Bodies of requests, in order, as "METHOD path" -> parsed JSON. */
	bodies: { call: string; body: unknown }[];
	/** Emits a Realtime change to every channel subscribed to `table` (as the server would). */
	push: (
		table: string,
		type: 'INSERT' | 'UPDATE' | 'DELETE',
		record: Record<string, unknown>
	) => void;
};

// A minimal Phoenix/Realtime server: acknowledges joins and heartbeats, and can push changes.
// Frames use the v2 array format: [join_ref, ref, topic, event, payload].
async function mockRealtime(page: Page) {
	const channels: {
		topic: string;
		ws: { send: (data: string) => void };
		ids: Record<string, number[]>;
	}[] = [];
	await page.routeWebSocket(/\/realtime\/v1\/websocket/, (ws) => {
		ws.onMessage((raw) => {
			const [joinRef, ref, topic, event, payload] = JSON.parse(String(raw));
			if (event === 'heartbeat') {
				ws.send(
					JSON.stringify([null, ref, 'phoenix', 'phx_reply', { status: 'ok', response: {} }])
				);
			} else if (event === 'phx_join') {
				const changes = (payload.config?.postgres_changes ?? []) as { table: string }[];
				const withIds = changes.map((c, i) => ({ ...c, id: i + 1 }));
				const ids: Record<string, number[]> = {};
				withIds.forEach((c) => (ids[c.table] = [...(ids[c.table] ?? []), c.id]));
				channels.push({ topic, ws, ids });
				ws.send(
					JSON.stringify([
						joinRef,
						ref,
						topic,
						'phx_reply',
						{ status: 'ok', response: { postgres_changes: withIds } }
					])
				);
			}
		});
	});
	return (table: string, type: string, record: Record<string, unknown>) => {
		for (const channel of channels.filter((c) => c.ids[table])) {
			channel.ws.send(
				JSON.stringify([
					null,
					null,
					channel.topic,
					'postgres_changes',
					{
						ids: channel.ids[table],
						data: {
							schema: 'public',
							table,
							commit_timestamp: '2026-03-01T12:00:00Z',
							type,
							...(type === 'DELETE' ? { old_record: record } : { record }),
							columns: [],
							errors: null
						}
					}
				])
			);
		}
	};
}

async function mockSupabase(page: Page, state: ApiState) {
	await page.route(`${SUPABASE}/**`, async (route: Route) => {
		const req = route.request();
		const cors = {
			'access-control-allow-origin': '*',
			'access-control-allow-headers': '*',
			'access-control-allow-methods': '*'
		};
		if (req.method() === 'OPTIONS') return route.fulfill({ status: 204, headers: cors });

		const url = new URL(req.url());
		const path = url.pathname;
		const call = `${req.method()} ${path}`;
		const param = (name: string) => url.searchParams.get(name)?.replace(/^eq\./, '');
		const isAdmin = req.headers()['authorization'] === `Bearer ${ADMIN_TOKEN}`;
		const body = req.postData() ? JSON.parse(req.postData()!) : undefined;

		state.calls.push(call);
		if (body !== undefined) state.bodies.push({ call, body });

		const json = (data: unknown, status = 200) =>
			route.fulfill({
				status,
				headers: cors,
				contentType: 'application/json',
				body: JSON.stringify(data)
			});
		const empty = () => route.fulfill({ status: 204, headers: cors });

		// ---- auth
		if (path === '/auth/v1/token') {
			if (body.password !== PASSWORD) {
				return json(
					{ code: 400, error_code: 'invalid_credentials', msg: 'Invalid login credentials' },
					400
				);
			}
			// "nobody" is a valid Supabase account that is not in public.admins.
			return body.email === 'nobody@example.test'
				? json(sessionFor('non-admin-token', body.email))
				: json(sessionFor(ADMIN_TOKEN, body.email));
		}
		if (path === '/auth/v1/recover') return json({});
		if (path === '/auth/v1/logout') return empty();
		if (path === '/auth/v1/user') return json(adminUser());

		// ---- rest
		if (path === '/rest/v1/admins') {
			return json(isAdmin && param('user_id') === ADMIN_ID ? [{ user_id: ADMIN_ID }] : []);
		}
		if (path === '/rest/v1/profiles') {
			if (req.method() === 'PATCH') return empty();
			const id = param('id');
			if (id === ADMIN_ID) return json([{ nickname: 'tester' }]);
			if (id) return json(USERS.filter((u) => u.id === id));
			return json(USERS);
		}
		if (path === '/rest/v1/keys') {
			if (req.method() === 'GET') {
				const id = param('id');
				const serial = param('serial_number');
				const userId = param('user_id');
				return json(
					state.keys.filter(
						(k) =>
							(!id || k.id === id) &&
							(!serial || k.serial_number === serial) &&
							(!userId || k.user_id === userId)
					)
				);
			}
			if (req.method() === 'PATCH') {
				const key = state.keys.find((k) => k.id === param('id'));
				if (key) Object.assign(key, body);
				return empty();
			}
			if (req.method() === 'DELETE') {
				state.keys = state.keys.filter((k) => k.id !== param('id'));
				return empty();
			}
		}
		if (path === '/rest/v1/doors' && req.method() === 'PATCH') return empty();
		if (path === '/rest/v1/entries') {
			// key_id is `eq.<id>` on the key page and `in.(<ids>)` on the user page.
			const keyFilter = url.searchParams.get('key_id');
			if (!keyFilter) return json(ENTRIES);
			return json(keyFilter.includes(KEY_IDS.first) ? ENTRIES : []);
		}

		// ---- edge function
		if (path === '/functions/v1/keys') {
			if (body.action === 'issue') {
				if (body.serial_number === 'DUPLICATE') {
					return json({ error: 'This tag is already bound to a key' }, 409);
				}
				const user = USERS.find((u) => u.id === body.user_id) ?? USERS[0];
				state.keys.push({
					...keyRow(KEY_IDS.created, user, body.serial_number),
					not_before: body.not_before,
					expires_at: body.expires_at
				});
				return json({ id: KEY_IDS.created });
			}
			if (body.action === 'renew') {
				const key = state.keys.find((k) => k.id === body.id);
				if (key) Object.assign(key, { not_before: body.not_before, expires_at: body.expires_at });
				return json({ id: body.id });
			}
		}

		return json({ error: `unmocked ${call}` }, 500);
	});
}

export const test = base.extend<{ api: ApiState; signedIn: boolean }>({
	signedIn: [true, { option: true }],
	// auto: every test gets the mocks, so none can reach the real Supabase project or Sentry.
	api: [
		async ({ page, signedIn }, use) => {
			// Fixed clock keeps date pickers ("today") and relative UI deterministic.
			await page.clock.setFixedTime(new Date('2026-03-01T12:00:00.000Z'));
			if (signedIn) await seedSession(page);
			const state: ApiState = {
				keys: structuredClone(KEYS),
				calls: [],
				bodies: [],
				push: await mockRealtime(page)
			};
			await mockSupabase(page, state);
			// Avatars fall back to the generated one; never hit Gravatar from tests.
			await page.route(/gravatar\.com/, (route) => route.fulfill({ status: 404 }));
			await page.route(/sentry\.io/, (route) => route.fulfill({ status: 200, body: '' }));
			await use(state);
		},
		{ auto: true }
	]
});

export { expect };
