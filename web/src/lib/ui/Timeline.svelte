<script lang="ts">
	import { keyPath, userPath } from '#lib/routes.ts';
	import { resolve } from '$app/paths';
	import { formatDate, formatDateTime, shortId, timeAgo } from '#lib/format.ts';
	import { arrive } from '#lib/motion.ts';
	import { EntryType, type Activity, type ActivityKind, type Entry } from '#lib/types.ts';

	// One member's history, newest first: what happened to them and their keys (logged by the
	// database when the change was made) interleaved with their door opens.
	let {
		activity,
		entries,
		now,
		actorName,
		serialByKey
	}: {
		activity: Activity[];
		entries: Entry[];
		now: number;
		/** Display name for the person who made a change. */
		actorName: (id: string) => string;
		serialByKey: Map<string, string | null>;
	} = $props();

	type Tone = 'good' | 'bad' | 'plain';
	type Icon =
		| 'user'
		| 'form'
		| 'pencil'
		| 'ban'
		| 'shield'
		| 'key'
		| 'lock'
		| 'unlock'
		| 'renew'
		| 'trash'
		| 'tag'
		| 'door'
		| 'deny';
	type Item = {
		id: string;
		at: Date;
		tone: Tone;
		icon: Icon;
		title: string;
		detail?: string;
		by?: { id: string; name: string };
		/** The key page to open, when the key still exists. */
		keyId?: string;
	};

	// 24x24 stroke paths.
	const ICONS: Record<Icon, string> = {
		user: 'M16 21v-2a4 4 0 0 0-4-4H6a4 4 0 0 0-4 4v2M9 11a4 4 0 1 0 0-8 4 4 0 0 0 0 8M19 8v6M22 11h-6',
		form: 'M9 5H7a2 2 0 0 0-2 2v12a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V7a2 2 0 0 0-2-2h-2M9 3h6v4H9zM9 14l2 2 4-4',
		pencil: 'M12 20h9M16.5 3.5a2.1 2.1 0 0 1 3 3L7 19l-4 1 1-4z',
		ban: 'M12 22a10 10 0 1 0 0-20 10 10 0 0 0 0 20M4.9 4.9l14.2 14.2',
		shield: 'M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z',
		key: 'M21 2l-9.6 9.6M15.5 7.5l3 3L22 7l-3-3M7.5 10.5a5.5 5.5 0 1 0 0 11 5.5 5.5 0 0 0 0-11',
		lock: 'M5 11h14v10H5zM8 11V7a4 4 0 0 1 8 0v4',
		unlock: 'M5 11h14v10H5zM8 11V7a4 4 0 0 1 7.5-2',
		renew: 'M21 12a9 9 0 1 1-3-6.7M21 3v6h-6',
		trash: 'M3 6h18M8 6V4h8v2M6 6l1 14h10l1-14',
		tag: 'M20.6 13.4l-7.2 7.2a2 2 0 0 1-2.8 0L3 13V3h10l7.6 7.6a2 2 0 0 1 0 2.8zM7.5 7.5h.01',
		door: 'M13 4h3a2 2 0 0 1 2 2v14M2 20h3M13 20h9M13 4l-4 1.5V20h4z',
		deny: 'M18 6 6 18M6 6l12 12'
	};

	const KINDS: Record<ActivityKind, [string, Tone, Icon]> = {
		registered: ['Registered', 'plain', 'user'],
		onboarded: ['Completed the questionnaire', 'plain', 'form'],
		answers_edited: ['Questionnaire answers edited', 'plain', 'pencil'],
		email_changed: ['Email changed', 'plain', 'pencil'],
		user_blocked: ['Blocked', 'bad', 'ban'],
		user_unblocked: ['Unblocked', 'good', 'user'],
		admin_granted: ['Made admin', 'good', 'shield'],
		admin_revoked: ['Admin removed', 'bad', 'shield'],
		key_issued: ['Key issued', 'good', 'key'],
		key_renewed: ['Key renewed', 'good', 'renew'],
		key_disabled: ['Key disabled', 'bad', 'lock'],
		key_enabled: ['Key enabled', 'good', 'unlock'],
		key_tag_released: ['Key tag released', 'plain', 'tag'],
		key_deleted: ['Key deleted', 'bad', 'trash']
	};

	const keyLabel = (id: string | null, serial: string | null | undefined) =>
		serial ? serial : id ? `key ${shortId(id)}` : '';

	const fromActivity = (a: Activity): Item => {
		const [title, tone, icon] = KINDS[a.kind];
		const label = keyLabel(a.keyId, a.serialNumber ?? serialByKey.get(a.keyId ?? '') ?? null);
		const until = a.expiresAt ? `until ${formatDate(a.expiresAt).split(',')[0]}` : '';
		const email = a.emailTo ? `${a.emailFrom ?? 'none'} → ${a.emailTo}` : '';
		return {
			id: `a${a.id}`,
			at: a.timestamp,
			tone,
			icon,
			title,
			detail: [label, until, email].filter(Boolean).join(' · ') || undefined,
			by: a.actorId ? { id: a.actorId, name: actorName(a.actorId) } : undefined,
			// A deleted key has no page to open.
			keyId: a.keyId && a.kind !== 'key_deleted' ? a.keyId : undefined
		};
	};

	const fromEntry = (e: Entry): Item => {
		const ok = e.type === EntryType.SUCCESSFUL;
		return {
			id: `e${e.id}`,
			at: e.timestamp,
			tone: ok ? 'good' : 'bad',
			icon: ok ? 'door' : 'deny',
			title: ok ? 'Opened the door' : `Denied${e.reason ? ` · ${e.reason}` : ''}`,
			detail: e.openedBy ? 'from the web UI' : keyLabel(e.keyId, serialByKey.get(e.keyId ?? ''))
		};
	};

	const day = (date: Date) => {
		const days = Math.round(
			(new Date(now).setHours(0, 0, 0, 0) - new Date(date).setHours(0, 0, 0, 0)) / 86_400_000
		);
		if (days === 0) return 'Today';
		if (days === 1) return 'Yesterday';
		return date.toLocaleDateString('en-GB', { day: 'numeric', month: 'short', year: 'numeric' });
	};

	// Events in order, with a day label ahead of the first event of each day.
	const rows = $derived.by(() => {
		const items = [...activity.map(fromActivity), ...entries.map(fromEntry)].sort(
			(a, b) => b.at.getTime() - a.at.getTime()
		);
		return items.map((item, i) => ({
			item,
			label: i === 0 || day(items[i - 1].at) !== day(item.at) ? day(item.at) : '',
			first: i === 0,
			last: i === items.length - 1
		}));
	});

	const NODE: Record<Tone, string> = {
		good: 'border-green-500/50 bg-green-500/15 text-green-500',
		bad: 'border-red-500/50 bg-red-500/15 text-red-500',
		plain: 'border-gray-300 bg-paper-2 text-gray-400'
	};
</script>

<ol>
	{#each rows as { item, label, first, last }, at (item.id)}
		<li in:arrive={{ index: at }}>
			{#if label}
				<div class="eyebrow pb-3 {first ? '' : 'pt-2'}">{label}</div>
			{/if}
			<div class="relative flex gap-3 pb-5 {last ? 'pb-0' : ''}">
				{#if !last}
					<!-- the rail runs from this node to the next one -->
					<span class="absolute top-7 bottom-0 left-[13px] w-px bg-gray-300" aria-hidden="true"
					></span>
				{/if}
				<span
					class="relative z-10 flex size-7 shrink-0 items-center justify-center border {NODE[
						item.tone
					]}"
					aria-hidden="true"
				>
					<svg
						viewBox="0 0 24 24"
						class="size-3.5"
						fill="none"
						stroke="currentColor"
						stroke-width="2"
						stroke-linecap="round"
						stroke-linejoin="round"><path d={ICONS[item.icon]} /></svg
					>
				</span>
				<div class="flex min-w-0 flex-1 items-start justify-between gap-3 pt-px">
					<div class="flex min-w-0 flex-col gap-0.5">
						{#if item.keyId}
							<a
								href={keyPath(item.keyId)}
								class="leading-6 hover:text-brand-text">{item.title}</a
							>
						{:else}
							<span class="leading-6">{item.title}</span>
						{/if}
						{#if item.detail}
							<span class="font-mono text-xs break-words text-gray-400">{item.detail}</span>
						{/if}
						{#if item.by}
							<span class="text-xs text-gray-400"
								>by
								<a
									href={userPath(item.by.id)}
									class="text-gray-600 hover:text-brand-text">{item.by.name}</a
								></span
							>
						{/if}
					</div>
					<time
						datetime={item.at.toISOString()}
						title={formatDateTime(item.at)}
						class="shrink-0 pt-1 font-mono text-[11px] text-gray-400">{timeAgo(item.at, now)}</time
					>
				</div>
			</div>
		</li>
	{:else}
		<li class="text-gray-400">Nothing has happened yet.</li>
	{/each}
</ol>
