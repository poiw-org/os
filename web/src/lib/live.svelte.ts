import type { RealtimePostgresChangesPayload } from '@supabase/supabase-js';
import { supabase } from '#lib/supabase.ts';

export type LiveStatus = 'connecting' | 'live' | 'offline';
type Table = 'profiles' | 'keys' | 'entries' | 'activity' | 'firmware_releases' | 'unidentified_tags';
type Row = Record<string, unknown>;

let channels = 0;

/**
 * Subscribes to row changes on one table. Row-level security applies to the stream, so a
 * subscriber only ever receives rows its select policy allows. Returns the unsubscribe function.
 */
export function watchTable(
	table: Table,
	filter: string | undefined,
	handlers: {
		change: (payload: RealtimePostgresChangesPayload<Row>) => void;
		/** Connected; `again` is true after a reconnect, when events may have been missed. */
		subscribed: (again: boolean) => void;
		status: (status: LiveStatus) => void;
	}
) {
	let connectedBefore = false;
	const channel = supabase
		.channel(`${table}:${filter ?? 'all'}:${channels++}`)
		.on(
			'postgres_changes',
			{ event: '*', schema: 'public', table, ...(filter ? { filter } : {}) },
			handlers.change
		)
		.subscribe((state) => {
			if (state === 'SUBSCRIBED') {
				handlers.status('live');
				handlers.subscribed(connectedBefore);
				connectedBefore = true;
			} else if (state === 'CLOSED' || state === 'CHANNEL_ERROR' || state === 'TIMED_OUT') {
				handlers.status('offline');
			}
		});
	return () => void supabase.removeChannel(channel);
}

/**
 * A list kept in sync with the database: loaded once, then patched by realtime events and
 * reloaded after a reconnect. Call it while a component initialises; it stops with the component.
 */
export function liveRows<T extends { id: string | number }>(config: {
	table: Table;
	filter?: string;
	load: () => Promise<T[]>;
	map: (row: Row) => T;
	/** Drop events for rows this view does not show (the stream can be wider than the view). */
	accept?: (item: T) => boolean;
	sort?: (a: T, b: T) => number;
	limit?: number;
}) {
	let items = $state.raw<T[]>();
	let status = $state<LiveStatus>('connecting');
	let error = $state('');

	// Events that arrive while a (re)load is in flight are applied on top of its result.
	let loading = false;
	let queued: RealtimePostgresChangesPayload<Row>[] = [];

	const settle = (list: T[]) => {
		const sorted = config.sort ? list.toSorted(config.sort) : list;
		return config.limit ? sorted.slice(0, config.limit) : sorted;
	};

	const apply = (payload: RealtimePostgresChangesPayload<Row>) => {
		const current = items ?? [];
		if (payload.eventType === 'DELETE') {
			const id = (payload.old as Row).id;
			items = current.filter((item) => item.id !== id);
			return;
		}
		const item = config.map(payload.new);
		if (config.accept && !config.accept(item)) {
			items = current.filter((existing) => existing.id !== item.id);
			return;
		}
		items = settle([...current.filter((existing) => existing.id !== item.id), item]);
	};

	const reload = async () => {
		loading = true;
		try {
			items = settle(await config.load());
			error = '';
		} catch (e) {
			error = e instanceof Error ? e.message : 'Could not load the data.';
		}
		loading = false;
		queued.forEach(apply);
		queued = [];
	};

	const stop = watchTable(config.table, config.filter, {
		change: (payload) => (loading ? queued.push(payload) : apply(payload)),
		subscribed: (again) => again && void reload(),
		status: (next) => (status = next)
	});
	void reload();

	$effect(() => stop);

	return {
		get items() {
			return items;
		},
		get status() {
			return status;
		},
		get error() {
			return error;
		}
	};
}
