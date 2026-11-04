import { FunctionsHttpError } from '@supabase/supabase-js';
import type { OnboardingAnswers } from '#lib/auth.ts';
import { withName } from '#lib/format.ts';
import { supabase } from '#lib/supabase.ts';
import {
	type Activity,
	type ActivityKind,
	type EntryType,
	type Entry,
	type FailReason,
	type Key,
	type KeySummary,
	type UserOption,
	type UserSummary
} from '#lib/types.ts';

// There is a single door; entries are logged per door id. This must match the id the door was
// provisioned with (public.door_users), or force-open commands go to a channel nobody listens on.
export const DOOR_ID = 'front';

const KEY_COLUMNS =
	'id, serial_number, not_before, expires_at, issued_at, disabled, serial_released, jwt, legacy_id, user:profiles!user_id(id, email, first_name, last_name, onboarded_at), issuer:profiles!issued_by(email)';

// Rows arrive in the same snake_case shape from queries and from realtime events.
type Raw = Record<string, unknown>;

// A name only counts once the member has completed the questionnaire.
const fullNameOf = (row: Raw) =>
	row.onboarded_at ? `${row.first_name ?? ''} ${row.last_name ?? ''}`.trim() : '';

export const toUserSummary = (row: Raw): UserSummary => ({
	id: row.id as string,
	email: (row.email as string | null) ?? '',
	fullName: fullNameOf(row),
	blocked: row.blocked as boolean
});

export const toKeySummary = (row: Raw): KeySummary => ({
	id: row.id as string,
	userId: row.user_id as string,
	serialNumber: (row.serial_number as string | null) ?? null,
	notBefore: new Date(row.not_before as string),
	expiresAt: new Date(row.expires_at as string),
	disabled: row.disabled as boolean,
	v1: row.legacy_id != null
});

export const toEntry = (row: Raw): Entry => {
	const metadata = (row.metadata ?? {}) as { serialNumberFromTag?: string };
	return {
		id: row.id as number,
		keyId: (row.key_id as string | null) ?? null,
		type: row.type as EntryType,
		reason: (row.reason as FailReason | null) ?? undefined,
		timestamp: new Date(row.created_at as string),
		serialNumberFromTag: metadata.serialNumberFromTag,
		openedBy: (row.opened_by as string | null) ?? null
	};
};

export const toActivity = (row: Raw): Activity => {
	const expires = ((row.detail ?? {}) as { expires_at?: string }).expires_at;
	return {
		id: row.id as number,
		timestamp: new Date(row.created_at as string),
		userId: row.user_id as string,
		actorId: (row.actor_id as string | null) ?? null,
		kind: row.kind as ActivityKind,
		keyId: (row.key_id as string | null) ?? null,
		serialNumber: (row.serial_number as string | null) ?? null,
		expiresAt: expires ? new Date(expires) : undefined
	};
};

const USER_COLUMNS = 'id, email, blocked, first_name, last_name, onboarded_at';
const KEY_SUMMARY_COLUMNS =
	'id, user_id, serial_number, not_before, expires_at, disabled, legacy_id';
const ENTRY_COLUMNS = 'id, key_id, type, reason, created_at, metadata, opened_by';

export async function listAllUsers(): Promise<UserSummary[]> {
	const { data, error } = await supabase
		.from('profiles')
		.select(USER_COLUMNS)
		.not('email', 'is', null);
	if (error) throw error;
	return data.map(toUserSummary);
}

export async function listAllKeys(userId?: string): Promise<KeySummary[]> {
	let query = supabase.from('keys').select(KEY_SUMMARY_COLUMNS);
	if (userId) query = query.eq('user_id', userId);
	const { data, error } = await query;
	if (error) throw error;
	return data.map(toKeySummary);
}

export async function getUser(id: string): Promise<UserSummary | undefined> {
	const { data, error } = await supabase
		.from('profiles')
		.select(USER_COLUMNS)
		.eq('id', id)
		.maybeSingle();
	if (error) throw error;
	return data ? toUserSummary(data) : undefined;
}

export async function isAdmin(userId: string): Promise<boolean> {
	const { data, error } = await supabase
		.from('admins')
		.select('user_id')
		.eq('user_id', userId)
		.maybeSingle();
	if (error) throw error;
	return Boolean(data);
}

// Admins cannot change their own role; the database refuses it.
export async function setAdmin(userId: string, admin: boolean) {
	const { error } = await supabase.rpc('set_admin', { target: userId, admin });
	if (error) throw error;
}

// The algorithm named in a token's header, or undefined when it cannot be read.
function jwtAlgorithm(jwt: string | null) {
	try {
		const header = JSON.parse(
			atob((jwt ?? '').split('.')[0].replace(/-/g, '+').replace(/_/g, '/'))
		);
		return typeof header.alg === 'string' ? header.alg : undefined;
	} catch {
		return undefined;
	}
}

function toKey(row: {
	id: string;
	serial_number: string | null;
	not_before: string;
	expires_at: string;
	issued_at: string;
	disabled: boolean;
	serial_released: boolean;
	jwt: string | null;
	legacy_id: string | null;
	user: {
		id: string;
		email: string | null;
		first_name: string | null;
		last_name: string | null;
		onboarded_at: string | null;
	};
	issuer: { email: string | null } | null;
}): Key {
	return {
		id: row.id,
		user: { id: row.user.id, email: row.user.email ?? '', fullName: fullNameOf(row.user) },
		serialNumber: row.serial_number,
		notBefore: new Date(row.not_before),
		expiresAt: new Date(row.expires_at),
		issuedAt: new Date(row.issued_at),
		issuedBy: row.issuer?.email ?? '',
		disabled: row.disabled,
		serialReleased: row.serial_released,
		jwt: row.jwt ?? '',
		v1: row.legacy_id != null,
		v2: jwtAlgorithm(row.jwt) === 'RS256'
	};
}

export async function getKey(id: string): Promise<Key | undefined> {
	const { data, error } = await supabase
		.from('keys')
		.select(KEY_COLUMNS)
		.eq('id', id)
		.maybeSingle();
	if (error) throw error;
	return data ? toKey(data) : undefined;
}

export async function getKeyBySerialNumber(serialNumber: string): Promise<Key | undefined> {
	const { data, error } = await supabase
		.from('keys')
		.select(KEY_COLUMNS)
		.eq('serial_number', serialNumber)
		.eq('serial_released', false)
		.maybeSingle();
	if (error) throw error;
	return data ? toKey(data) : undefined;
}

export async function setKeyDisabled(id: string, disabled: boolean) {
	const { error } = await supabase.from('keys').update({ disabled }).eq('id', id);
	if (error) throw error;
}

// Frees the tag's serial number for another key. The key must already be disabled.
export async function setSerialReleased(id: string, released: boolean) {
	const { error } = await supabase.from('keys').update({ serial_released: released }).eq('id', id);
	if (error) throw error;
}

export async function deleteKey(id: string) {
	const { error } = await supabase.from('keys').delete().eq('id', id);
	if (error) throw error;
}

export async function listUsers(): Promise<UserOption[]> {
	const { data, error } = await supabase
		.from('profiles')
		.select('id, email, first_name, last_name, onboarded_at')
		.eq('blocked', false)
		.not('email', 'is', null)
		.order('email');
	if (error) throw error;
	return data.map((user) => ({
		value: user.id,
		label: withName({ email: user.email ?? '', fullName: fullNameOf(user) })
	}));
}

/**
 * Newest first. Pass `keyIds` to scope the log to some keys, plus `openedBy` to include the door
 * opens those users gave from the web UI; omit both for the whole door. `since` is inclusive and
 * `until` exclusive.
 */
export async function listEntries(
	options: {
		keyIds?: string[];
		openedBy?: string[];
		since?: Date;
		until?: Date;
		type?: EntryType;
		limit?: number;
		/** Skip this many of the newest matches; pages through a long log with `limit`. */
		offset?: number;
	} = {}
) {
	let query = supabase
		.from('entries')
		.select(ENTRY_COLUMNS)
		.order('created_at', { ascending: false });
	if (options.openedBy?.length) {
		const keys = options.keyIds?.length ? `,key_id.in.(${options.keyIds.join(',')})` : '';
		query = query.or(`opened_by.in.(${options.openedBy.join(',')})${keys}`);
	} else if (options.keyIds) query = query.in('key_id', options.keyIds);
	if (options.since) query = query.gte('created_at', options.since.toISOString());
	if (options.until) query = query.lt('created_at', options.until.toISOString());
	if (options.type) query = query.eq('type', options.type);
	if (options.limit) {
		const start = options.offset ?? 0;
		query = query.range(start, start + options.limit - 1);
	}
	const { data, error } = await query;
	if (error) throw error;
	return data.map(toEntry);
}

/** Newest first: the changes made to one member and their keys. */
export async function listActivity(userId: string, limit = 50) {
	const { data, error } = await supabase
		.from('activity')
		.select('id, created_at, user_id, actor_id, kind, key_id, serial_number, detail')
		.eq('user_id', userId)
		.order('created_at', { ascending: false })
		.limit(limit);
	if (error) throw error;
	return data.map(toActivity);
}

// Edge functions answer failures with { error }, which is the message worth showing.
async function invoke<T>(name: string, body: Record<string, unknown> = {}): Promise<T> {
	const { data, error } = await supabase.functions.invoke(name, { body });
	if (error) {
		if (error instanceof FunctionsHttpError) {
			const payload = await error.context.json().catch(() => null);
			throw new Error(payload?.error ?? error.message);
		}
		throw error;
	}
	return data;
}

// Issuing and renewing sign the key's JWT, so they run in the `keys` edge function.
const invokeKeys = (body: Record<string, unknown>) => invoke<{ id: string }>('keys', body);

/** Without a serial number the key is virtual: it has no tag. */
export const issueKey = (key: {
	userId: string;
	serialNumber?: string;
	notBefore: Date;
	expiresAt: Date;
}) =>
	invokeKeys({
		action: 'issue',
		user_id: key.userId,
		...(key.serialNumber === undefined ? { virtual: true } : { serial_number: key.serialNumber }),
		not_before: key.notBefore.toISOString(),
		expires_at: key.expiresAt.toISOString()
	});

export const renewKey = (key: { id: string; notBefore: Date; expiresAt: Date }) =>
	invokeKeys({
		action: 'renew',
		id: key.id,
		not_before: key.notBefore.toISOString(),
		expires_at: key.expiresAt.toISOString()
	});

const COMMAND_TIMEOUT_MS = 6000;
// The door answers every copy of a command and acts on each id once, so repeating the command until
// it is confirmed is harmless, and it covers a door that is reconnecting at the moment of the first send.
const COMMAND_RESEND_MS = 700;

const withTimeout = <T>(promise: Promise<T>, message: string) =>
	new Promise<T>((resolve, reject) => {
		const timer = setTimeout(() => reject(new Error(message)), COMMAND_TIMEOUT_MS);
		promise.then(resolve, reject).finally(() => clearTimeout(timer));
	});

/** The checkpoints of a force open, in order. */
export const FORCE_OPEN_STEPS = ['auth', 'join', 'send', 'ack', 'unlock', 'log'] as const;
export type ForceOpenStep = (typeof FORCE_OPEN_STEPS)[number];

/** One line of a door command's choreography: what was exchanged, and how that went. */
export type CommandEvent<Step extends string> = {
	/** Milliseconds since the command started. */
	at: number;
	step: Step;
	state: 'active' | 'done' | 'failed';
	/** Direction of the line in the transcript: sent to, received from, or local. */
	dir: '>' | '<' | '·';
	text: string;
	/** Round trip of this exchange, when it has one. */
	latencyMs?: number;
};
export type ForceOpenEvent = CommandEvent<ForceOpenStep>;

// Authorises the realtime socket and joins the door's private channel; the first two checkpoints of
// every door command. Throws (after reporting it) when either fails.
async function joinDoorChannel(
	channel: ReturnType<typeof supabase.channel>,
	topic: string,
	emit: (e: Omit<CommandEvent<'auth' | 'join'>, 'at'>) => void,
	since: (t: number) => number
) {
	const fail = (step: 'auth' | 'join', message: string): never => {
		emit({ step, state: 'failed', dir: '·', text: message });
		throw new Error(message);
	};
	emit({ step: 'auth', state: 'active', dir: '·', text: 'Authorising the realtime socket' });
	let t = performance.now();
	await supabase.realtime.setAuth().catch((e: Error) => fail('auth', e.message));
	emit({
		step: 'auth',
		state: 'done',
		dir: '·',
		text: 'Access token attached',
		latencyMs: since(performance.now()) - since(t)
	});

	emit({ step: 'join', state: 'active', dir: '>', text: `phx_join realtime:${topic} (private)` });
	t = performance.now();
	await withTimeout(
		new Promise<void>((resolve, reject) =>
			channel.subscribe((status, err) => {
				if (status === 'SUBSCRIBED') resolve();
				else if (status === 'CHANNEL_ERROR' || status === 'TIMED_OUT' || status === 'CLOSED')
					reject(new Error(`Could not join ${topic} (${err?.message ?? status}).`));
			})
		),
		'Could not reach the realtime service.'
	).catch((e: Error) => fail('join', e.message));
	emit({
		step: 'join',
		state: 'done',
		dir: '<',
		text: 'phx_reply ok, channel joined',
		latencyMs: Math.round(performance.now() - t)
	});
}

// Remote unlock for members. They cannot join the door's channel (it carries key tokens), so the
// `unlock` edge function sends the command, waits for the door and logs the open. The same
// checkpoints are reported as in `forceOpenDoor`; the server handles the first two.
export async function unlockDoor(onEvent: (event: ForceOpenEvent) => void = () => {}) {
	const t0 = performance.now();
	const emit = (e: Omit<ForceOpenEvent, 'at'>) =>
		onEvent({ at: Math.round(performance.now() - t0), ...e });

	emit({ step: 'auth', state: 'done', dir: '·', text: 'Signed in' });
	emit({ step: 'join', state: 'done', dir: '·', text: 'The server relays to the door' });
	emit({ step: 'send', state: 'active', dir: '>', text: 'rpc unlock' });
	let result: { latency_ms: number; delay_ms: number; logged: boolean };
	try {
		result = await invoke('unlock');
	} catch (e) {
		emit({
			step: 'ack',
			state: 'failed',
			dir: '<',
			text: e instanceof Error ? e.message : 'Failed'
		});
		throw e;
	}
	emit({ step: 'send', state: 'done', dir: '·', text: 'Command on the wire' });
	emit({
		step: 'ack',
		state: 'done',
		dir: '<',
		text: 'force_open_ack',
		latencyMs: result.latency_ms
	});

	const unlockAt = performance.now() + result.delay_ms;
	emit({
		step: 'unlock',
		state: 'active',
		dir: '·',
		text: result.delay_ms ? 'Warning tone playing at the door' : 'Door unlocking'
	});
	for (let left = Math.ceil((unlockAt - performance.now()) / 1000); left > 0; left--) {
		emit({ step: 'unlock', state: 'active', dir: '·', text: `Unlocking in ${left} s` });
		await new Promise((resolve) =>
			setTimeout(resolve, Math.min(1000, unlockAt - performance.now()))
		);
	}
	emit({ step: 'unlock', state: 'done', dir: '·', text: 'Door unlocked' });
	if (result.logged)
		emit({ step: 'log', state: 'done', dir: '<', text: 'Entry recorded under your name' });
	else emit({ step: 'log', state: 'failed', dir: '<', text: 'Not logged' });
	return { latencyMs: result.latency_ms, logged: result.logged };
}

// Sends "force open" to the door over its private realtime channel, waits for the door to confirm,
// then logs the open as the signed-in admin's. Broadcasts are never stored, so a command that finds
// the door offline is lost instead of unlocking it hours later; the caller learns that from the
// missing confirmation. Every exchange is reported through `onEvent`.
export async function forceOpenDoor(onEvent: (event: ForceOpenEvent) => void = () => {}) {
	const id = crypto.randomUUID();
	const topic = `door:${DOOR_ID}`;
	const t0 = performance.now();
	const since = (t: number) => Math.round(t - t0);
	const emit = (e: Omit<ForceOpenEvent, 'at'>) => onEvent({ at: since(performance.now()), ...e });
	const fail = (step: ForceOpenStep, message: string): never => {
		emit({ step, state: 'failed', dir: '·', text: message });
		throw new Error(message);
	};

	const channel = supabase.channel(topic, { config: { private: true } });
	// The door confirms at once and unlocks after `delay_ms` (a warning tone plays meanwhile).
	let confirm!: (ack: { receivedAt: number; delayMs: number }) => void;
	const confirmed = new Promise<{ receivedAt: number; delayMs: number }>(
		(resolve) => (confirm = resolve)
	);
	channel.on('broadcast', { event: 'force_open_ack' }, ({ payload }) => {
		if (payload?.id === id)
			confirm({ receivedAt: performance.now(), delayMs: Number(payload.delay_ms) || 0 });
	});

	try {
		await joinDoorChannel(channel, topic, emit, since);

		const command = {
			type: 'broadcast' as const,
			event: 'force_open',
			payload: { id, ts: Date.now() }
		};
		emit({
			step: 'send',
			state: 'active',
			dir: '>',
			text: `broadcast force_open id=${id.slice(0, 8)}`
		});
		const sentAt = performance.now();
		const sent = await channel.send(command);
		if (sent !== 'ok') fail('send', `The command could not be sent (${sent}).`);
		emit({ step: 'send', state: 'done', dir: '·', text: 'Command on the wire' });

		emit({ step: 'ack', state: 'active', dir: '·', text: 'Waiting for the door to confirm' });
		let copies = 1;
		const resend = setInterval(() => {
			copies++;
			void channel.send(command);
			emit({
				step: 'ack',
				state: 'active',
				dir: '>',
				text: `No answer yet, resending (copy ${copies})`
			});
		}, COMMAND_RESEND_MS);
		let ack: { receivedAt: number; delayMs: number };
		try {
			ack = await withTimeout(
				confirmed,
				'The door did not answer, so nothing was unlocked. It may be offline.'
			).catch((e: Error) => fail('ack', e.message));
		} finally {
			clearInterval(resend);
		}
		const latencyMs = Math.round(ack.receivedAt - sentAt);
		emit({
			step: 'ack',
			state: 'done',
			dir: '<',
			text: `force_open_ack id=${id.slice(0, 8)}`,
			latencyMs
		});

		// The door sounds a warning, then unlocks on its own; nothing more is sent.
		const unlockAt = ack.receivedAt + ack.delayMs;
		emit({
			step: 'unlock',
			state: 'active',
			dir: '·',
			text: ack.delayMs ? 'Warning tone playing at the door' : 'Door unlocking'
		});
		for (let left = Math.ceil((unlockAt - performance.now()) / 1000); left > 0; left--) {
			emit({ step: 'unlock', state: 'active', dir: '·', text: `Unlocking in ${left} s` });
			await new Promise((resolve) =>
				setTimeout(resolve, Math.min(1000, unlockAt - performance.now()))
			);
		}
		emit({ step: 'unlock', state: 'done', dir: '·', text: 'Door unlocked' });

		// The door has opened; a failure to log must not read as a failure to open.
		emit({ step: 'log', state: 'active', dir: '>', text: 'rpc log_web_open' });
		const t = performance.now();
		const { error } = await supabase.rpc('log_web_open', {
			door: DOOR_ID,
			command_id: id,
			latency_ms: latencyMs
		});
		if (error) {
			emit({ step: 'log', state: 'failed', dir: '<', text: `Not logged: ${error.message}` });
			return { latencyMs, logged: false };
		}
		emit({
			step: 'log',
			state: 'done',
			dir: '<',
			text: 'Entry recorded under your name',
			latencyMs: Math.round(performance.now() - t)
		});
		return { latencyMs, logged: true };
	} finally {
		void supabase.removeChannel(channel);
	}
}

/** The checkpoints of writing a key to a tag with the door's reader, in order. */
export const WRITE_CARD_STEPS = ['auth', 'join', 'door', 'tag', 'write'] as const;
export type WriteCardStep = (typeof WRITE_CARD_STEPS)[number];
export type WriteCardEvent = CommandEvent<WriteCardStep>;

/** What the door reports while it is in writer mode. */
export type WriteCardState = 'ready' | 'wrong_tag' | 'writing' | 'done' | 'failed';

const WRITE_FAILURES: Record<string, string> = {
	too_small: 'The tag is too small to hold the key. Use an NTAG215/216 or a MIFARE Classic 1K.',
	unsupported:
		'The door can only write NTAG tags and MIFARE Classic cards that are already formatted for NDEF.',
	failed: 'The tag moved away or refused the write. Nothing was changed on the key.',
	timeout: 'No tag was presented in time.',
	cancelled: 'Cancelled.',
	rejected: 'The door refused to write this key.'
};

// Puts the door's reader in writer mode and has it write `token` to the tag the key is bound to.
// Without a token the reader only reads: the next tag's serial number comes back instead.
// The door answers `write_card_status` as it goes; `onState` follows each state so the UI can show
// when to present the tag. Resolves once the door has written and verified the tag. Aborting sends
// a cancel and rejects.
export async function writeCardViaDoor(
	token: string | undefined,
	options: {
		signal: AbortSignal;
		onEvent?: (event: WriteCardEvent) => void;
		onState?: (state: WriteCardState, timeoutMs: number) => void;
	}
) {
	const { signal, onEvent = () => {}, onState = () => {} } = options;
	const id = crypto.randomUUID();
	const topic = `door:${DOOR_ID}`;
	const t0 = performance.now();
	const since = (t: number) => Math.round(t - t0);
	const emit = (e: Omit<WriteCardEvent, 'at'>) => onEvent({ at: since(performance.now()), ...e });
	const fail = (step: WriteCardStep, message: string): never => {
		emit({ step, state: 'failed', dir: '·', text: message });
		throw new Error(message);
	};

	const channel = supabase.channel(topic, { config: { private: true } });
	// States arrive in order; each is consumed by the loop below.
	const inbox: { state: string; timeoutMs: number; serial: string }[] = [];
	let wake = () => {};
	channel.on('broadcast', { event: 'write_card_status' }, ({ payload }) => {
		if (payload?.id !== id) return;
		inbox.push({
			state: String(payload.state),
			timeoutMs: Number(payload.timeout_ms) || 0,
			serial: String(payload.serial ?? '')
		});
		wake();
	});
	const next = (ms: number, message: string) =>
		new Promise<(typeof inbox)[number]>((resolve, reject) => {
			const done = () => {
				clearTimeout(timer);
				signal.removeEventListener('abort', done);
				wake = () => {};
				if (signal.aborted) reject(new Error(WRITE_FAILURES.cancelled));
				else if (inbox.length) resolve(inbox.shift()!);
				else reject(new Error(message));
			};
			const timer = setTimeout(done, ms);
			wake = done;
			signal.addEventListener('abort', done);
			if (inbox.length || signal.aborted) done();
		});

	try {
		await joinDoorChannel(channel, topic, emit, since);
		// Cancelled while joining: never arm the door.
		if (signal.aborted) throw new Error(WRITE_FAILURES.cancelled);

		const command = {
			type: 'broadcast' as const,
			event: token ? 'write_card' : 'scan_card',
			payload: { id, ts: Date.now(), token }
		};
		emit({
			step: 'door',
			state: 'active',
			dir: '>',
			text: `broadcast ${token ? 'write_card' : 'scan_card'} id=${id.slice(0, 8)}`
		});
		const sent = await channel.send(command);
		if (sent !== 'ok') fail('door', `The command could not be sent (${sent}).`);

		// The door answers every copy and arms itself once, so repeating until it speaks is harmless.
		let copies = 1;
		const resend = setInterval(() => {
			copies++;
			void channel.send(command);
			emit({
				step: 'door',
				state: 'active',
				dir: '>',
				text: `No answer yet, resending (copy ${copies})`
			});
		}, COMMAND_RESEND_MS);
		let first: (typeof inbox)[number];
		try {
			first = await next(
				COMMAND_TIMEOUT_MS,
				'The door did not answer, so nothing was written. It may be offline.'
			).catch((e: Error) => fail('door', e.message));
		} finally {
			clearInterval(resend);
		}
		inbox.unshift(first);

		let announced = false;
		for (;;) {
			// The door gives up on its own at its timeout; allow a little for the last message to arrive.
			const { state, timeoutMs, serial } = await next(
				(first.timeoutMs || 60_000) + COMMAND_TIMEOUT_MS,
				'The door stopped answering.'
			).catch((e: Error) => fail(announced ? 'tag' : 'door', e.message));
			if (state === 'ready' || state === 'wrong_tag') {
				if (!announced) {
					announced = true;
					emit({ step: 'door', state: 'done', dir: '<', text: 'Door is in writer mode' });
					emit({ step: 'tag', state: 'active', dir: '·', text: 'Waiting for the tag' });
				} else if (state === 'wrong_tag') {
					emit({ step: 'tag', state: 'active', dir: '<', text: 'A different tag was presented' });
				}
				onState(state, timeoutMs);
			} else if (state === 'writing') {
				emit({ step: 'tag', state: 'done', dir: '<', text: 'Tag detected' });
				emit({ step: 'write', state: 'active', dir: '·', text: 'Writing and verifying the tag' });
				onState('writing', timeoutMs);
			} else if (state === 'done') {
				emit({ step: 'tag', state: 'done', dir: '<', text: 'Tag detected' });
				emit({
					step: 'write',
					state: 'done',
					dir: '<',
					text: token ? 'Written and read back' : `Tag ${serial}`
				});
				onState('done', timeoutMs);
				return { serial };
			} else {
				const message = WRITE_FAILURES[state] ?? `The door reported "${state}".`;
				onState('failed', timeoutMs);
				fail(announced ? (state === 'failed' ? 'write' : 'tag') : 'door', message);
			}
		}
	} catch (e) {
		// Leaving writer mode matters most when we gave up while the door was still waiting.
		if (signal.aborted)
			void channel.send({ type: 'broadcast', event: 'write_cancel', payload: { id } });
		throw e;
	} finally {
		// Give the cancel a moment to leave the socket before the channel goes away.
		setTimeout(() => void supabase.removeChannel(channel), signal.aborted ? 300 : 0);
	}
}

/** A member's questionnaire answers, or null until they have completed it. */
export async function getAnswers(userId: string): Promise<OnboardingAnswers | null> {
	const { data, error } = await supabase
		.from('profiles')
		.select(
			'first_name, last_name, studies, department, referral_source, diy_familiar, onboarded_at'
		)
		.eq('id', userId)
		.maybeSingle();
	if (error) throw error;
	if (!data?.onboarded_at) return null;
	return {
		firstName: data.first_name ?? '',
		lastName: data.last_name ?? '',
		studies: Boolean(data.studies),
		department: data.department ?? '',
		referralSource: data.referral_source as OnboardingAnswers['referralSource'],
		diyFamiliar: Boolean(data.diy_familiar)
	};
}

/** Admins correct a member's answers; the row stays onboarded and the database refuses gaps. */
export async function saveAnswers(userId: string, a: OnboardingAnswers) {
	const { data, error } = await supabase
		.from('profiles')
		.update({
			first_name: a.firstName.trim(),
			last_name: a.lastName.trim(),
			studies: a.studies,
			department: a.studies ? a.department.trim() : null,
			referral_source: a.referralSource,
			diy_familiar: a.diyFamiliar
		})
		.eq('id', userId)
		.select('id');
	if (error) throw error;
	// Row-level security hides rows from an update instead of failing it, so an empty result means
	// the database did not let this user change them.
	if (!data?.length) throw new Error('Not allowed to edit this profile.');
}
