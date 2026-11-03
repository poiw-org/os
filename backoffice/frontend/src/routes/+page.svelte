<script lang="ts">
	import Fa from 'svelte-fa';
	import { faSearch, faWifi, faChevronRight } from '@fortawesome/free-solid-svg-icons';
	import { goto } from '$app/navigation';
	import { resolve } from '$app/paths';
	import { getKeyBySerialNumber, listAllKeys, listAllUsers, listEntries } from '#lib/api.ts';
	import { toEntry, toKeySummary, toUserSummary } from '#lib/api.ts';
	import { liveRows } from '#lib/live.svelte.ts';
	import { arrive } from '#lib/motion.ts';
	import { scanTag } from '#lib/nfc.ts';
	import { toast } from '#lib/notify.svelte.ts';
	import { keyStatus } from '#lib/status.ts';
	import Loading from '#lib/loading.svelte';
	import ActivityRow from '#lib/ui/ActivityRow.svelte';
	import Avatar from '#lib/ui/Avatar.svelte';

	let searchQuery = $state('');
	let showAll = $state(false);
	let scanning = $state(false);

	const users = liveRows({
		table: 'profiles',
		load: listAllUsers,
		map: toUserSummary
	});
	const keys = liveRows({
		table: 'keys',
		load: () => listAllKeys(),
		map: toKeySummary
	});
	const activity = liveRows({
		table: 'entries',
		load: () => listEntries({ limit: 40 }),
		map: toEntry,
		sort: (a, b) => b.timestamp.getTime() - a.timestamp.getTime(),
		limit: 40
	});

	// Relative times and key statuses are re-evaluated every 30s.
	let now = $state(Date.now());
	$effect(() => {
		const timer = setInterval(() => (now = Date.now()), 30_000);
		return () => clearInterval(timer);
	});

	const keysByUser = $derived(Map.groupBy(keys.items ?? [], (key) => key.userId));
	const emailByKey = $derived(
		new Map(
			(keys.items ?? []).map((key) => [
				key.id,
				users.items?.find((user) => user.id === key.userId)?.email ?? 'Unknown user'
			])
		)
	);

	const query = $derived(searchQuery.trim().toLowerCase());
	const rows = $derived(
		(users.items ?? [])
			.map((user) => ({ user, keys: keysByUser.get(user.id) ?? [] }))
			.filter(({ user, keys }) => {
				if (!query) return showAll || keys.length > 0;
				return (
					user.email.toLowerCase().includes(query) ||
					keys.some((key) => key.serialNumber.toLowerCase().includes(query))
				);
			})
			.sort((a, b) => b.keys.length - a.keys.length || a.user.email.localeCompare(b.user.email))
	);
	const withoutKeys = $derived(
		(users.items ?? []).filter((user) => !keysByUser.has(user.id)).length
	);
	const loaded = $derived(users.items && keys.items);
	const loadError = $derived(users.error || keys.error);

	const findByTag = async () => {
		scanning = true;
		await scanTag(
			async (serialNumber, _ndef, stop) => {
				stop();
				const key = await getKeyBySerialNumber(serialNumber);
				if (key) await goto(resolve(`/items?id=${key.id}`));
				else toast.error('This key was not found on the database.');
			},
			() => (scanning = false)
		);
	};
</script>

<svelte:head>
	<title>Users</title>
</svelte:head>

<div class="lg:grid lg:grid-cols-[minmax(0,1.25fr)_minmax(0,1fr)]">
	<section class="flex min-w-0 flex-col gap-5 p-4 md:p-8">
		<div class="flex flex-col gap-1">
			<div class="flex items-center justify-between">
				<h1 class="text-xl font-semibold tracking-tight">Users</h1>
			</div>
			<p class="text-gray-400">Everyone with access to the door, and the keys they hold.</p>
		</div>

		{#if !scanning}
			<div
				class="flex items-center gap-3 rounded-lg border bg-paper-2 py-1.5 pr-1.5 pl-3.5 text-gray-400 focus-within:border-gray-400"
			>
				<Fa icon={faSearch} />
				<input
					type="text"
					bind:value={searchQuery}
					id="search"
					placeholder="Search users or tag S/N"
					class="w-full border-0! bg-transparent! p-1 text-ink shadow-none! outline-none!"
				/>
				<!-- svelte-ignore a11y_click_events_have_key_events, a11y_no_static_element_interactions -->
				<span onclick={findByTag} class="btn shrink-0"><Fa icon={faWifi} />Scan tag</span>
			</div>
		{:else}
			<span class="btn self-start"><Fa icon={faSearch} />Please approach the tag ...</span>
		{/if}

		{#if loadError}
			<span class="flex items-center justify-center pt-10 text-center text-red-500"
				>{loadError}</span
			>
		{:else if !loaded}
			<Loading />
		{:else}
			<div class="flex flex-col gap-2" id="results">
				<div class="panel">
					{#each rows as { user, keys } (user.id)}
						{@const active = keys.filter(
							(key) => keyStatus(key, new Date(now)) === 'active'
						).length}
						<a href={resolve(`/users?id=${user.id}`)} in:arrive class="row">
							<span class="flex min-w-0 items-center gap-3">
								<Avatar email={user.email} size={32} />
								<span class="truncate">{user.email}</span>
								{#if user.blocked}<span
										class="rounded-full border border-red-500/40 px-2 py-0.5 text-[11px] text-red-500"
										>BLOCKED</span
									>{/if}
							</span>
							<span class="flex shrink-0 items-center gap-3 text-xs text-gray-400">
								{#if keys.length}
									<span class="flex items-center gap-1.5"
										><span
											class="h-1.5 w-1.5 rounded-full {active ? 'bg-green-500' : 'bg-gray-300'}"
										></span>{keys.length}
										{keys.length === 1 ? 'key' : 'keys'}{#if active}<span class="hidden md:inline">
												· {active} active</span
											>{/if}</span
									>
								{:else}
									<span>no keys</span>
								{/if}
								<Fa icon={faChevronRight} />
							</span>
						</a>
					{:else}
						<span class="block p-8 text-center text-gray-400">No users were found.</span>
					{/each}
				</div>
				{#if !query && withoutKeys > 0}
					<button
						onclick={() => (showAll = !showAll)}
						class="self-start text-xs text-gray-400 underline underline-offset-4 hover:text-ink"
						>{showAll
							? 'Hide users without keys'
							: `Show ${withoutKeys} users without keys`}</button
					>
				{/if}
			</div>
		{/if}
	</section>

	<aside
		class="flex min-w-0 flex-col gap-2 border-t bg-paper/60 p-4 md:p-8 lg:sticky lg:top-[49px] lg:h-[calc(100vh-49px)] lg:overflow-y-auto lg:border-t-0 lg:border-l"
	>
		<div class="flex flex-col gap-1 pb-3">
			<div class="flex items-center justify-between">
				<h2 class="text-xl font-semibold tracking-tight">Live entries</h2>
			</div>
			<p class="text-gray-400">Every tap on the door, as it happens.</p>
		</div>
		<div class="panel">
			{#each activity.items ?? [] as entry (entry.id)}
				<div in:arrive>
					<ActivityRow
						{entry}
						{now}
						label={(entry.keyId && emailByKey.get(entry.keyId)) || 'Deleted key'}
					/>
				</div>
			{:else}
				<span class="block p-8 text-center text-gray-400">No door activity yet.</span>
			{/each}
		</div>
	</aside>
</div>
