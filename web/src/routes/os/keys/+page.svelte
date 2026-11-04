<script lang="ts">
	import { page } from '$app/state';
	import { resolve } from '$app/paths';
	import { listAllKeys, toKeySummary } from '#lib/api.ts';
	import { formatDate, shortId } from '#lib/format.ts';
	import { liveRows } from '#lib/live.svelte.ts';
	import { arrive } from '#lib/motion.ts';
	import { keyStatus } from '#lib/status.ts';
	import Loading from '#lib/loading.svelte';
	import KeyStatusBadge from '#lib/ui/KeyStatusBadge.svelte';
	import V1Badge from '#lib/ui/V1Badge.svelte';
	import VirtualBadge from '#lib/ui/VirtualBadge.svelte';

	const userId = page.data.user.id as string;

	// Row-level security already limits keys to the member's own; the filter keeps the stream narrow.
	const keys = liveRows({
		table: 'keys',
		filter: `user_id=eq.${userId}`,
		load: () => listAllKeys(userId),
		map: toKeySummary
	});

	let now = $state(Date.now());
	$effect(() => {
		const timer = setInterval(() => (now = Date.now()), 30_000);
		return () => clearInterval(timer);
	});
</script>

<svelte:head>
	<title>My keys</title>
</svelte:head>

<div class="stagger flex max-w-4xl flex-col gap-6 px-4 py-6 md:px-8 md:py-10">
	<div class="flex flex-col gap-1">
		<h1 class="text-xl font-bold">My keys</h1>
		<p class="text-gray-400">
			You can disable a key you no longer trust. Only an admin can enable it again.
		</p>
	</div>

	{#if keys.error}
		<span class="flex justify-center pt-10 text-red-500">{keys.error}</span>
	{:else if !keys.items}
		<Loading />
	{:else}
		<div class="panel">
			{#each keys.items as key, at (key.id)}
				<a href={resolve(`/os/items?id=${key.id}`)} in:arrive={{ index: at }} class="row">
					<span class="flex min-w-0 flex-wrap items-baseline gap-x-4 gap-y-0.5">
						<span class="font-mono text-[13px]">{shortId(key.id)}</span>
						{#if key.serialNumber}<span class="font-mono text-xs text-gray-400"
								>{key.serialNumber}</span
							>{:else}<VirtualBadge />{/if}
						{#if key.v1}<V1Badge />{/if}
					</span>
					<span class="flex shrink-0 items-center gap-4">
						<span class="hidden text-xs text-gray-400 md:inline"
							>until {formatDate(key.expiresAt).split(',')[0]}</span
						>
						<KeyStatusBadge status={keyStatus(key, new Date(now))} />
					</span>
				</a>
			{:else}
				<span class="block p-8 text-gray-400">You have no keys yet.</span>
			{/each}
		</div>
	{/if}
</div>
