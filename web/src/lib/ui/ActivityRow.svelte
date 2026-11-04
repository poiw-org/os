<script lang="ts">
	import { resolve } from '$app/paths';
	import { keyPath, userPath } from '#lib/routes.ts';
	import { formatDateTime, timeAgo } from '#lib/format.ts';
	import { EntryType, type Entry } from '#lib/types.ts';

	// One log line for the cross-key feeds (home, user page); the key page has its own expandable list.
	let { entry, label, now }: { entry: Entry; label: string; now: number } = $props();

	const ok = $derived(entry.type === EntryType.SUCCESSFUL);
	const web = $derived(entry.openedBy !== null);
</script>

<a
	href={entry.openedBy !== null
		? userPath(entry.openedBy)
		: entry.keyId
			? keyPath(entry.keyId)
			: resolve('/os')}
	class="row"
>
	<span class="flex min-w-0 items-center gap-3">
		<span
			class="lamp shrink-0 {ok ? 'lamp-green' : 'lamp-red'}"
			title={ok ? 'Door opened' : 'Denied'}
		></span>
		<span class="flex min-w-0 flex-col">
			<span class="truncate">{label}</span>
			<span class="flex items-center gap-1.5 text-xs text-gray-400"
				>{#if web}<span
						class="rounded border px-1 font-mono text-[10px] leading-4 tracking-wide text-gray-500"
						>WEB</span
					>Opened the door from the web UI{:else}{ok
						? 'Opened the door'
						: (entry.reason ?? 'Denied')}{/if}</span
			>
		</span>
	</span>
	<time
		datetime={entry.timestamp.toISOString()}
		title={formatDateTime(entry.timestamp)}
		class="shrink-0 text-xs text-gray-400">{timeAgo(entry.timestamp, now)}</time
	>
</a>
