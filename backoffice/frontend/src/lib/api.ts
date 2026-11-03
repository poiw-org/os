import { FunctionsHttpError } from '@supabase/supabase-js';
import { supabase } from '#lib/supabase.ts';
import {
	type EntryType,
	type Entry,
	type FailReason,
	type Key,
	type KeySummary,
	type UserOption,
	type UserSummary
} from '#lib/types.ts';

// There is a single door; entries are logged per door id.
const DOOR_ID = 'main';

const KEY_COLUMNS =
	'id, serial_number, not_before, expires_at, issued_at, disabled, jwt, user:profiles!user_id(id, email), issuer:profiles!issued_by(email)';

// Rows arrive in the same snake_case shape from queries and from realtime events.
type Raw = Record<string, unknown>;

export const toUserSummary = (row: Raw): UserSummary => ({
	id: row.id as string,
	email: (row.email as string | null) ?? '',
	blocked: row.blocked as boolean
});

export const toKeySummary = (row: Raw): KeySummary => ({
	id: row.id as string,
	userId: row.user_id as string,
	serialNumber: row.serial_number as string,
	notBefore: new Date(row.not_before as string),
	expiresAt: new Date(row.expires_at as string),
	disabled: row.disabled as boolean
});

export const toEntry = (row: Raw): Entry => {
	const metadata = (row.metadata ?? {}) as { serialNumberFromTag?: string };
	return {
		id: row.id as number,
		keyId: (row.key_id as string | null) ?? null,
		type: row.type as EntryType,
		reason: (row.reason as FailReason | null) ?? undefined,
		timestamp: new Date(row.created_at as string),
		serialNumberFromTag: metadata.serialNumberFromTag
	};
};

const KEY_SUMMARY_COLUMNS = 'id, user_id, serial_number, not_before, expires_at, disabled';
const ENTRY_COLUMNS = 'id, key_id, type, reason, created_at, metadata';

export async function listAllUsers(): Promise<UserSummary[]> {
	const { data, error } = await supabase
		.from('profiles')
		.select('id, email, blocked')
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
		.select('id, email, blocked')
		.eq('id', id)
		.maybeSingle();
	if (error) throw error;
	return data ? toUserSummary(data) : undefined;
}

function toKey(row: {
	id: string;
	serial_number: string;
	not_before: string;
	expires_at: string;
	issued_at: string;
	disabled: boolean;
	jwt: string | null;
	user: { id: string; email: string | null };
	issuer: { email: string | null } | null;
}): Key {
	return {
		id: row.id,
		user: { id: row.user.id, email: row.user.email ?? '' },
		serialNumber: row.serial_number,
		notBefore: new Date(row.not_before),
		expiresAt: new Date(row.expires_at),
		issuedAt: new Date(row.issued_at),
		issuedBy: row.issuer?.email ?? '',
		disabled: row.disabled,
		jwt: row.jwt ?? ''
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
		.maybeSingle();
	if (error) throw error;
	return data ? toKey(data) : undefined;
}

export async function setKeyDisabled(id: string, disabled: boolean) {
	const { error } = await supabase.from('keys').update({ disabled }).eq('id', id);
	if (error) throw error;
}

export async function deleteKey(id: string) {
	const { error } = await supabase.from('keys').delete().eq('id', id);
	if (error) throw error;
}

export async function listUsers(): Promise<UserOption[]> {
	const { data, error } = await supabase
		.from('profiles')
		.select('id, email')
		.eq('blocked', false)
		.not('email', 'is', null)
		.order('email');
	if (error) throw error;
	return data.map((user) => ({ value: user.id, label: user.email ?? '' }));
}

/** Newest first. Pass `keyIds` to scope the log to some keys; omit it for the whole door. */
export async function listEntries(options: { keyIds?: string[]; limit?: number } = {}) {
	let query = supabase
		.from('entries')
		.select(ENTRY_COLUMNS)
		.order('created_at', { ascending: false });
	if (options.keyIds) query = query.in('key_id', options.keyIds);
	if (options.limit) query = query.limit(options.limit);
	const { data, error } = await query;
	if (error) throw error;
	return data.map(toEntry);
}

// Issuing and renewing sign the key's JWT, so they run in the `keys` edge function.
async function invokeKeys(body: Record<string, unknown>): Promise<{ id: string }> {
	const { data, error } = await supabase.functions.invoke('keys', { body });
	if (error) {
		if (error instanceof FunctionsHttpError) {
			const payload = await error.context.json().catch(() => null);
			throw new Error(payload?.error ?? error.message);
		}
		throw error;
	}
	return data;
}

export const issueKey = (key: {
	userId: string;
	serialNumber: string;
	notBefore: Date;
	expiresAt: Date;
}) =>
	invokeKeys({
		action: 'issue',
		user_id: key.userId,
		serial_number: key.serialNumber,
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

const withTimeout = <T>(promise: Promise<T>, message: string) =>
	new Promise<T>((resolve, reject) => {
		const timer = setTimeout(() => reject(new Error(message)), COMMAND_TIMEOUT_MS);
		promise.then(resolve, reject).finally(() => clearTimeout(timer));
	});

// Sends "force open" to the door over its private realtime channel and waits for the door to
// confirm. Broadcasts are never stored, so a command that finds the door offline is lost instead
// of unlocking it hours later; the caller learns that from the missing confirmation.
export async function forceOpenDoor() {
	const id = crypto.randomUUID();
	const channel = supabase.channel(`door:${DOOR_ID}`, { config: { private: true } });

	let confirm!: () => void;
	const confirmed = new Promise<void>((resolve) => (confirm = resolve));
	channel.on('broadcast', { event: 'force_open_ack' }, ({ payload }) => {
		if (payload?.id === id) confirm();
	});

	try {
		await supabase.realtime.setAuth();
		await withTimeout(
			new Promise<void>((resolve, reject) =>
				channel.subscribe((status) => {
					if (status === 'SUBSCRIBED') resolve();
					else if (status === 'CHANNEL_ERROR' || status === 'TIMED_OUT' || status === 'CLOSED')
						reject(new Error('Could not reach the realtime service.'));
				})
			),
			'Could not reach the realtime service.'
		);
		const sent = await channel.send({
			type: 'broadcast',
			event: 'force_open',
			payload: { id, ts: Date.now() }
		});
		if (sent !== 'ok') throw new Error('The command could not be sent.');
		await withTimeout(
			confirmed,
			'The door did not answer, so nothing was unlocked. It may be offline.'
		);
	} finally {
		void supabase.removeChannel(channel);
	}
}
