<script lang="ts">
	import { resolve } from '$app/paths';
	import { formatDateTime, timeAgo } from '#lib/format.ts';
	import { EntryType, type Entry } from '#lib/types.ts';

	// One log line for the cross-key feeds (home, user page); the key page has its own expandable list.
	let { entry, label, now }: { entry: Entry; label: string; now: number } = $props();

	const ok = $derived(entry.type === EntryType.SUCCESSFUL);
</script>

<a href={resolve(entry.keyId ? `/items?id=${entry.keyId}` : '/')} class="row">
	<span class="flex min-w-0 items-center gap-3">
		<span
			class="h-1.5 w-1.5 shrink-0 rounded-full {ok ? 'bg-green-500' : 'bg-red-500'}"
			title={ok ? 'Door opened' : 'Denied'}
		></span>
		<span class="flex min-w-0 flex-col">
			<span class="truncate">{label}</span>
			<span class="text-xs text-gray-400"
				>{ok ? 'Opened the door' : (entry.reason ?? 'Denied')}</span
			>
		</span>
	</span>
	<time
		datetime={entry.timestamp.toISOString()}
		title={formatDateTime(entry.timestamp)}
		class="shrink-0 text-xs text-gray-400">{timeAgo(entry.timestamp, now)}</time
	>
</a>
