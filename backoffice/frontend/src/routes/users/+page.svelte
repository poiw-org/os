<script lang="ts">
	import { page } from '$app/state';
	import { resolve } from '$app/paths';
	import { onMount } from 'svelte';
	import { getUser, listAllKeys, listEntries, toEntry, toKeySummary } from '#lib/api.ts';
	import { formatDate, shortId } from '#lib/format.ts';
	import { liveRows } from '#lib/live.svelte.ts';
	import { arrive } from '#lib/motion.ts';
	import { keyStatus } from '#lib/status.ts';
	import Loading from '#lib/loading.svelte';
	import type { UserSummary } from '#lib/types.ts';
	import ActivityRow from '#lib/ui/ActivityRow.svelte';
	import Avatar from '#lib/ui/Avatar.svelte';
	import Breadcrumb from '#lib/ui/Breadcrumb.svelte';
	import KeyStatusBadge from '#lib/ui/KeyStatusBadge.svelte';
	import V1Badge from '#lib/ui/V1Badge.svelte';
	import VirtualBadge from '#lib/ui/VirtualBadge.svelte';

	const userId = page.url.searchParams.get('id') ?? '';

	let user = $state.raw<UserSummary>();
	let loadError = $state('');

	// The stream is scoped by RLS to admins and filtered to this user's keys on the server.
	const keys = liveRows({
		table: 'keys',
		filter: `user_id=eq.${userId}`,
		load: () => listAllKeys(userId),
		map: toKeySummary,
		sort: (a, b) => b.expiresAt.getTime() - a.expiresAt.getTime()
	});
	// Entries reference keys by text id with no FK, so the stream is not filterable by user;
	// the client keeps only rows for this user's keys.
	const entries = liveRows({
		table: 'entries',
		load: async () => {
			const ids = (await listAllKeys(userId)).map((key) => key.id);
			return listEntries({ keyIds: ids, openedBy: userId, limit: 25 });
		},
		map: toEntry,
		accept: (entry) =>
			entry.openedBy === userId || (keys.items ?? []).some((key) => key.id === entry.keyId),
		sort: (a, b) => b.timestamp.getTime() - a.timestamp.getTime(),
		limit: 25
	});

	let now = $state(Date.now());
	$effect(() => {
		const timer = setInterval(() => (now = Date.now()), 30_000);
		return () => clearInterval(timer);
	});

	const serialByKey = $derived(
		new Map((keys.items ?? []).map((key) => [key.id, key.serialNumber]))
	);

	onMount(async () => {
		try {
			user = userId ? await getUser(userId) : undefined;
			if (!user) loadError = 'User not found.';
		} catch (e) {
			loadError = e instanceof Error ? e.message : 'Could not load the user.';
		}
	});
</script>

<svelte:head>
	<title>{user?.email ?? 'User'}</title>
</svelte:head>

<div class="flex max-w-4xl flex-col gap-6 px-4 py-6 md:px-8 md:py-10">
	{#if loadError}
		<span class="flex w-full justify-center px-10 pt-20 text-red-500">{loadError}</span>
	{:else if !user || !keys.items}
		<Loading />
	{:else}
		<Breadcrumb crumbs={[{ label: 'Users', href: '/' }, { label: user.email }]} />

		<div class="flex flex-wrap items-center justify-between gap-4">
			<span class="flex items-center gap-4">
				<Avatar email={user.email} size={56} />
				<span class="flex flex-col">
					<span class="text-xl font-bold break-all">{user.email}</span>
					<span class="text-gray-400"
						>{keys.items.length} key{keys.items.length === 1 ? '' : 's'}{#if user.blocked}
							· <span class="text-red-500">blocked</span>{/if}</span
					>
				</span>
			</span>
			{#if !user.blocked}
				<a href={resolve(`/items/create?user=${user.id}`)} class="btn btn-primary">Issue key</a>
			{/if}
		</div>

		<div class="flex flex-col gap-2">
			<h2 class="font-bold">Keys</h2>
			<div class="panel">
				{#each keys.items as key (key.id)}
					<a href={resolve(`/items?id=${key.id}`)} in:arrive class="row">
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
					<span class="block p-8 text-gray-400">This user has no keys yet.</span>
				{/each}
			</div>
		</div>

		<div class="flex flex-col gap-2">
			<h2 class="font-bold">Recent entries</h2>
			<div class="panel">
				{#each entries.items ?? [] as entry (entry.id)}
					<div in:arrive>
						<ActivityRow
							{entry}
							{now}
							label={entry.openedBy
								? 'Web UI'
								: (entry.keyId && serialByKey.get(entry.keyId)) || 'Deleted key'}
						/>
					</div>
				{:else}
					<span class="block p-8 text-gray-400">No entries for this user's keys yet.</span>
				{/each}
			</div>
		</div>
	{/if}
</div>
