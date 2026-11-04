<script lang="ts">
	import { goto } from '$app/navigation';
	import { resolve } from '$app/paths';
	import { getKeyBySerialNumber, listAllKeys, listAllUsers, listEntries } from '#lib/api.ts';
	import { toEntry, toKeySummary, toUserSummary } from '#lib/api.ts';
	import { timeAgo, withName } from '#lib/format.ts';
	import { liveRows } from '#lib/live.svelte.ts';
	import { arrive } from '#lib/motion.ts';
	import { nfcSupported, scanTag } from '#lib/nfc.ts';
	import { toast } from '#lib/notify.svelte.ts';
	import { canonicalSerial, serialDigits } from '#lib/serial.ts';
	import { keyStatus } from '#lib/status.ts';
	import { EntryType, type Entry, type UserOption } from '#lib/types.ts';
	import Loading from '#lib/loading.svelte';
	import ActivityRow from '#lib/ui/ActivityRow.svelte';
	import Avatar from '#lib/ui/Avatar.svelte';
	import DateRange from '#lib/ui/DateRange.svelte';
	import UnlockChart from '#lib/ui/UnlockChart.svelte';
	import UserMultiSelect from '#lib/ui/UserMultiSelect.svelte';

	// Someone who opened the door more recently than this gets a "last seen" badge.
	const RECENT_MS = 2 * 3_600_000;
	const PAGE_SIZE = 50;
	// The unlocks chart covers today and the days before it, in local time.
	const CHART_DAYS = 30;
	// PostgREST caps a response at 1000 rows, so the chart's window is read in pages of that.
	const CHART_PAGE = 1000;

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
	// Successful door opens inside the "recently seen" window, to rank and badge users.
	const recent = liveRows({
		table: 'entries',
		load: async () =>
			(
				await listEntries({
					type: EntryType.SUCCESSFUL,
					since: new Date(Date.now() - RECENT_MS)
				})
			).filter((entry) => entry.openedBy === null),
		map: toEntry,
		accept: (entry) => entry.type === EntryType.SUCCESSFUL && entry.openedBy === null,
		sort: (a, b) => b.timestamp.getTime() - a.timestamp.getTime()
	});
	// Every successful door open (tag or web) since the first day of the chart.
	const chartStart = new Date();
	const chartSince = new Date(
		chartStart.getFullYear(),
		chartStart.getMonth(),
		chartStart.getDate() - (CHART_DAYS - 1)
	);
	const unlocks = liveRows({
		table: 'entries',
		load: async () => {
			const all: Entry[] = [];
			for (;;) {
				const page = await listEntries({
					type: EntryType.SUCCESSFUL,
					since: chartSince,
					limit: CHART_PAGE,
					offset: all.length
				});
				all.push(...page);
				// A row logged mid-read shifts the offsets and repeats one row across pages.
				if (page.length < CHART_PAGE) return [...new Map(all.map((e) => [e.id, e])).values()];
			}
		},
		map: toEntry,
		accept: (entry) => entry.type === EntryType.SUCCESSFUL && entry.timestamp >= chartSince
	});

	// Entry search: any user or date filter replaces the latest-entries feed with a query.
	let selectedUsers = $state<UserOption[]>([]);
	let from = $state<Date | null>(null);
	let to = $state<Date | null>(null);
	let results = $state.raw<Entry[]>();
	let searchError = $state('');
	let exhausted = $state(false);
	let loadingMore = $state(false);
	// Bumped whenever the search restarts, so a page that was in flight is dropped.
	let generation = 0;
	const filtered = $derived(selectedUsers.length > 0 || !!from || !!to);

	const search = $derived.by(() => {
		if (!filtered) return undefined;
		const userIds = selectedUsers.map((user) => user.value);
		const keyIds = (keys.items ?? [])
			.filter((key) => userIds.includes(key.userId))
			.map((key) => key.id);
		// The range is whole days: from the start of the first to the end of the last.
		const since = from ? new Date(from) : undefined;
		since?.setHours(0, 0, 0, 0);
		const until = to ? new Date(to) : undefined;
		until?.setHours(0, 0, 0, 0);
		until?.setDate(until.getDate() + 1);
		return {
			keyIds: userIds.length ? keyIds : undefined,
			openedBy: userIds.length ? userIds : undefined,
			since,
			until,
			limit: PAGE_SIZE
		};
	});

	// Restart from the newest page when the filters change or a new door entry arrives.
	$effect(() => {
		const options = search;
		void activity.items?.[0]?.id;
		const current = ++generation;
		loadingMore = false;
		if (!options) {
			results = undefined;
			return;
		}
		listEntries(options).then(
			(list) => {
				if (current !== generation) return;
				results = list;
				exhausted = list.length < PAGE_SIZE;
				searchError = '';
			},
			(e) => {
				if (current === generation)
					searchError = e instanceof Error ? e.message : 'Could not search entries.';
			}
		);
	});

	const loadMore = async () => {
		if (!search || !results || exhausted || loadingMore) return;
		const current = generation;
		loadingMore = true;
		try {
			const list = await listEntries({ ...search, offset: results.length });
			if (current !== generation) return;
			// New entries shift the offsets, so drop rows that were already loaded.
			const loaded = new Set(results.map((entry) => entry.id));
			results = [...results, ...list.filter((entry) => !loaded.has(entry.id))];
			exhausted = list.length < PAGE_SIZE;
		} catch (e) {
			if (current === generation)
				searchError = e instanceof Error ? e.message : 'Could not load more entries.';
		} finally {
			if (current === generation) loadingMore = false;
		}
	};

	// Loads the next page when the end of the list scrolls into view. Re-observing after each
	// page covers a sentinel that is still on screen.
	const sentinel = (node: Element) => {
		void results?.length;
		const observer = new IntersectionObserver(
			([entry]) => entry.isIntersecting && void loadMore(),
			{ rootMargin: '300px' }
		);
		observer.observe(node);
		return () => observer.disconnect();
	};

	const clearFilters = () => {
		selectedUsers = [];
		from = null;
		to = null;
	};
	const pickable = $derived(
		(users.items ?? [])
			.filter((user) => user.email)
			.map((user) => ({ value: user.id, label: withName(user) }))
	);
	const shownEntries = $derived(filtered ? results : activity.items);

	// Relative times and key statuses are re-evaluated every 30s.
	let now = $state(Date.now());
	$effect(() => {
		const timer = setInterval(() => (now = Date.now()), 30_000);
		return () => clearInterval(timer);
	});

	const withNameOrFallback = (
		user: { email: string; fullName: string } | undefined,
		fallback: string
	) => (user ? withName(user) : fallback);
	const keysByUser = $derived(Map.groupBy(keys.items ?? [], (key) => key.userId));
	const emailByKey = $derived(
		new Map(
			(keys.items ?? []).map((key) => [
				key.id,
				withNameOrFallback(
					users.items?.find((user) => user.id === key.userId),
					'Unknown user'
				)
			])
		)
	);

	const query = $derived(searchQuery.trim().toLowerCase());
	// A typed S/N matches however it is spelled ("04A2", "04:a2").
	// Only for queries that look like a serial, so typing a name does not match stray hex digits.
	const queryDigits = $derived(/^[0-9a-fx:\s-]+$/.test(query) ? serialDigits(query) : '');
	const keyOwner = $derived(new Map((keys.items ?? []).map((key) => [key.id, key.userId])));
	const emailById = $derived(new Map((users.items ?? []).map((user) => [user.id, user.email])));
	// A web open belongs to the admin who gave it; a tag open to the key's owner.
	const unlockedBy = (entry: Entry) => {
		const userId = entry.openedBy ?? (entry.keyId ? keyOwner.get(entry.keyId) : undefined);
		return userId ? emailById.get(userId) || undefined : undefined;
	};
	// Latest door open per user, only while it still counts as recent.
	const lastSeen = $derived.by(() => {
		const seen = new Map<string, Date>();
		for (const entry of recent.items ?? []) {
			const userId = entry.keyId ? keyOwner.get(entry.keyId) : undefined;
			if (userId && !seen.has(userId) && now - entry.timestamp.getTime() < RECENT_MS)
				seen.set(userId, entry.timestamp);
		}
		return seen;
	});
	const rows = $derived(
		(users.items ?? [])
			.map((user) => {
				const own = keysByUser.get(user.id) ?? [];
				return {
					user,
					keys: own,
					active: own.filter((key) => keyStatus(key, new Date(now)) === 'active').length,
					seen: lastSeen.get(user.id)
				};
			})
			.filter(({ user, keys }) => {
				if (!query) return showAll || keys.length > 0;
				return (
					user.email.toLowerCase().includes(query) ||
					user.fullName.toLowerCase().includes(query) ||
					keys.some(
						(key) =>
							key.serialNumber?.toLowerCase().includes(query) ||
							(queryDigits.length >= 2 &&
								!!key.serialNumber &&
								serialDigits(key.serialNumber).includes(queryDigits))
					)
				);
			})
			// Users with a key first, then those with an active key, then whoever came in last.
			.sort(
				(a, b) =>
					Number(b.keys.length > 0) - Number(a.keys.length > 0) ||
					Number(b.active > 0) - Number(a.active > 0) ||
					(b.seen?.getTime() ?? 0) - (a.seen?.getTime() ?? 0) ||
					b.keys.length - a.keys.length ||
					a.user.email.localeCompare(b.user.email)
			)
	);
	const withoutKeys = $derived(
		(users.items ?? []).filter((user) => !keysByUser.has(user.id)).length
	);
	const loaded = $derived(users.items && keys.items);
	const loadError = $derived(users.error || keys.error);

	// A tag scan stops on its own after this long, so a forgotten scan does not linger.
	const SCAN_TIMEOUT_MS = 60_000;
	let stopScan: (() => void) | undefined;
	const findByTag = async () => {
		scanning = true;
		let timer: ReturnType<typeof setTimeout> | undefined;
		let ended = false;
		const stop = await scanTag(
			async (serialNumber, _ndef, stop) => {
				stop();
				try {
					const key = await getKeyBySerialNumber(canonicalSerial(serialNumber) ?? serialNumber);
					if (key) await goto(resolve(`/os/items?id=${key.id}`));
					else toast.error('This key was not found on the database.');
				} catch (e) {
					toast.error(e instanceof Error ? e.message : 'Could not look up the tag.');
				}
			},
			() => {
				ended = true;
				clearTimeout(timer);
				stopScan = undefined;
				scanning = false;
			}
		);
		if (!stop || ended) return;
		stopScan = stop;
		timer = setTimeout(stop, SCAN_TIMEOUT_MS);
	};
	// Leaving the page ends the scan.
	$effect(() => () => stopScan?.());
</script>

<svelte:head>
	<title>Users</title>
</svelte:head>

{#if unlocks.items && loaded}
	<UnlockChart unlocks={unlocks.items} personOf={unlockedBy} days={CHART_DAYS} {now} />
{/if}

<div class="stagger lg:grid lg:grid-cols-[minmax(0,1.25fr)_minmax(0,1fr)]">
	<section class="flex min-w-0 flex-col gap-5 p-4 md:p-8">
		<div class="flex flex-col gap-1">
			<h1 class="text-xl font-bold">Users</h1>
		</div>

		{#if !scanning}
			<div
				class="flex items-center gap-3 rounded-lg border bg-paper-2 py-1.5 pr-1.5 pl-3.5 text-gray-400 focus-within:border-brand-text"
			>
				<input
					type="text"
					bind:value={searchQuery}
					id="search"
					placeholder="Search users or tag S/N"
					class="w-full border-0! bg-transparent! p-1 text-ink shadow-none! outline-none!"
				/>
				{#if nfcSupported}
					<!-- svelte-ignore a11y_click_events_have_key_events, a11y_no_static_element_interactions -->
					<span onclick={findByTag} class="btn shrink-0">Scan tag</span>
				{/if}
			</div>
		{:else}
			<span class="flex items-center gap-2 self-start">
				<span class="btn">Hold the tag to the phone…</span>
				<button type="button" class="btn" onclick={() => stopScan?.()}>Cancel</button>
			</span>
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
					{#each rows as { user, keys, active, seen }, at (user.id)}
						<a href={resolve(`/os/users?id=${user.id}`)} in:arrive={{ index: at }} class="row">
							<span class="flex min-w-0 items-center gap-3">
								<Avatar email={user.email} size={32} />
								<span class="truncate">
									{#if user.fullName}<span class="font-bold">{user.fullName}</span>
										<span class="text-gray-400">{user.email}</span>{:else}{user.email}{/if}
								</span>
								{#if user.blocked}<span class="text-xs text-red-500">blocked</span>{/if}
								{#if seen}
									<span
										class="inline-flex shrink-0 items-center gap-1.5 text-xs text-gray-600"
										title={seen.toLocaleString('el-GR')}
										><span class="h-1.5 w-1.5 rounded-full bg-green-500"></span>Last seen {timeAgo(
											seen,
											now
										)}</span
									>
								{/if}
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
							</span>
						</a>
					{:else}
						<span class="block p-8 text-gray-400">No users found.</span>
					{/each}
				</div>
				{#if !query && withoutKeys > 0}
					<button
						onclick={() => (showAll = !showAll)}
						class="self-start text-xs text-brand-text underline"
						>{showAll
							? 'Hide users without keys'
							: `Show ${withoutKeys} users without keys`}</button
					>
				{/if}
			</div>
		{/if}
	</section>

	<aside
		class="flex min-w-0 flex-col gap-2 border-t p-4 md:p-8 lg:sticky lg:top-[49px] lg:h-[calc(100vh-49px)] lg:overflow-y-auto lg:border-t-0 lg:border-l"
	>
		<div class="flex items-center justify-between gap-3 pb-1">
			<h2 class="text-xl font-bold">Door entries</h2>
			{#if filtered}
				<button type="button" onclick={clearFilters} class="btn">Clear filters</button>
			{/if}
		</div>
		<div class="flex flex-col gap-2 pb-3">
			<UserMultiSelect users={pickable} bind:selected={selectedUsers} />
			<DateRange bind:from bind:to />
		</div>
		{#if searchError && filtered}
			<span class="block pb-2 text-sm text-red-500">{searchError}</span>
		{/if}
		<div class="panel">
			{#each shownEntries ?? [] as entry, at (entry.id)}
				<div in:arrive={{ index: at }}>
					<ActivityRow
						{entry}
						{now}
						label={entry.openedBy
							? withNameOrFallback(
									users.items?.find((user) => user.id === entry.openedBy),
									'An admin'
								)
							: (entry.keyId && emailByKey.get(entry.keyId)) || 'Deleted key'}
					/>
				</div>
			{:else}
				<span class="block p-8 text-gray-400"
					>{!shownEntries && filtered
						? 'Searching…'
						: filtered
							? 'No entries match these filters.'
							: 'No door activity yet.'}</span
				>
			{/each}
		</div>
		{#if filtered && results}
			{#if !exhausted}
				<div {@attach sentinel} class="py-4 text-center text-xs text-gray-400">
					{loadingMore ? 'Loading more…' : ''}
				</div>
			{:else if results.length > PAGE_SIZE}
				<p class="py-4 text-center text-xs text-gray-400">End of results</p>
			{/if}
		{/if}
	</aside>
</div>
