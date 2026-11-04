<script lang="ts">
	import { page } from '$app/state';
	import { resolve } from '$app/paths';
	import { onMount } from 'svelte';
	import {
		getAnswers,
		getUser,
		isAdmin,
		listAllKeys,
		listActivity,
		listAllUsers,
		listEntries,
		saveAnswers,
		setAdmin,
		toActivity,
		toEntry,
		toKeySummary
	} from '#lib/api.ts';
	import { formatDate, shortId, withName } from '#lib/format.ts';
	import { liveRows } from '#lib/live.svelte.ts';
	import { ask, toast } from '#lib/notify.svelte.ts';
	import { arrive } from '#lib/motion.ts';
	import { keyStatus } from '#lib/status.ts';
	import Loading from '#lib/loading.svelte';
	import type { OnboardingAnswers } from '#lib/auth.ts';
	import type { UserSummary } from '#lib/types.ts';
	import Avatar from '#lib/ui/Avatar.svelte';
	import Breadcrumb from '#lib/ui/Breadcrumb.svelte';
	import KeyStatusBadge from '#lib/ui/KeyStatusBadge.svelte';
	import Timeline from '#lib/ui/Timeline.svelte';
	import V1Badge from '#lib/ui/V1Badge.svelte';
	import VirtualBadge from '#lib/ui/VirtualBadge.svelte';
	import IssueKeyMenu from '#lib/ui/IssueKeyMenu.svelte';

	const userId = page.url.searchParams.get('id') ?? '';

	let user = $state.raw<UserSummary>();
	let admin = $state(false);
	let changingRole = $state(false);
	let loadError = $state('');
	// The database refuses changes to your own role, so the control is hidden for yourself.
	const self = userId === page.data.user.id;

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
			return listEntries({ keyIds: ids, openedBy: [userId], limit: 25 });
		},
		map: toEntry,
		accept: (entry) =>
			entry.openedBy === userId || (keys.items ?? []).some((key) => key.id === entry.keyId),
		sort: (a, b) => b.timestamp.getTime() - a.timestamp.getTime(),
		limit: 25
	});

	// Changes made to this member, written by database triggers; admins only (RLS).
	const activity = liveRows({
		table: 'activity',
		filter: `user_id=eq.${userId}`,
		load: () => listActivity(userId),
		map: toActivity,
		sort: (a, b) => b.timestamp.getTime() - a.timestamp.getTime(),
		limit: 50
	});
	// Names for the people behind the changes.
	let people = $state.raw(new Map<string, string>());
	const actorName = (id: string) => people.get(id) ?? 'Unknown user';

	let now = $state(Date.now());
	$effect(() => {
		const timer = setInterval(() => (now = Date.now()), 30_000);
		return () => clearInterval(timer);
	});

	const serialByKey = $derived(
		new Map((keys.items ?? []).map((key) => [key.id, key.serialNumber]))
	);

	const REFERRALS: [OnboardingAnswers['referralSource'], string][] = [
		['friend', 'A friend'],
		['welcome_day', 'Freshmen welcome day'],
		['newsletter', 'Newsletter'],
		['campaign', 'Promotional campaign']
	];
	// The member's questionnaire answers: undefined while loading, null when not yet completed.
	let answers = $state<OnboardingAnswers | null>();
	let draft = $state<OnboardingAnswers>();
	let savingAnswers = $state(false);
	const answersChanged = $derived(
		Boolean(answers && draft && JSON.stringify(answers) !== JSON.stringify(draft))
	);

	const saveDraft = async (event: SubmitEvent) => {
		event.preventDefault();
		if (!draft || !user) return;
		savingAnswers = true;
		try {
			await saveAnswers(userId, draft);
			answers = { ...draft };
			user = { ...user, fullName: `${draft.firstName} ${draft.lastName}`.trim() };
			toast.success('Answers saved.');
		} catch (e) {
			toast.error(e instanceof Error ? e.message : 'Could not save the answers.');
		}
		savingAnswers = false;
	};

	onMount(async () => {
		listAllUsers()
			.then((all) => (people = new Map(all.map((u) => [u.id, withName(u)]))))
			.catch(() => {});
		getAnswers(userId)
			.then((loaded) => {
				answers = loaded;
				draft = loaded ? { ...loaded } : undefined;
			})
			.catch(() => (answers = null));
		try {
			[user, admin] = userId
				? await Promise.all([getUser(userId), isAdmin(userId)])
				: [undefined, false];
			if (!user) loadError = 'User not found.';
		} catch (e) {
			loadError = e instanceof Error ? e.message : 'Could not load the user.';
		}
	});

	const toggleAdmin = async () => {
		if (!user) return;
		const accepted = await ask(
			admin
				? {
						title: 'Remove admin?',
						message: `${withName(user)} will no longer manage users, keys or the door. Their own keys keep working.`,
						confirmLabel: 'Remove admin',
						tone: 'danger'
					}
				: {
						title: 'Make admin?',
						message: `${withName(user)} will be able to manage every user, key and the door, including other admins.`,
						confirmLabel: 'Make admin'
					}
		);
		if (!accepted) return;
		changingRole = true;
		try {
			await setAdmin(user.id, !admin);
			admin = !admin;
			toast.success(
				admin ? `${withName(user)} is now an admin.` : `${withName(user)} is no longer an admin.`
			);
		} catch (e) {
			toast.error(e instanceof Error ? e.message : 'Could not change the role.');
		} finally {
			changingRole = false;
		}
	};
</script>

<svelte:head>
	<title>{user ? user.fullName || user.email : 'User'}</title>
</svelte:head>

<div class="stagger flex flex-col gap-8 px-4 py-6 md:px-8 md:py-10 lg:flex-row lg:items-start">
	{#if loadError}
		<span class="flex w-full justify-center px-10 pt-20 text-red-500">{loadError}</span>
	{:else if !user || !keys.items}
		<Loading />
	{:else}
		<div class="flex max-w-4xl min-w-0 flex-1 flex-col gap-6">
			<Breadcrumb
				crumbs={[{ label: 'Users', href: '/os' }, { label: user.fullName || user.email }]}
			/>

			<div class="flex flex-wrap items-center justify-between gap-4">
				<span class="flex items-center gap-4">
					<Avatar email={user.email} size={56} />
					<span class="flex flex-col">
						<span class="text-xl font-bold break-all">{user.fullName || user.email}</span>
						{#if user.fullName}<span class="break-all text-gray-400">{user.email}</span>{/if}
						<span class="text-gray-400"
							>{#if admin}<span class="text-ink">admin</span> ·
							{/if}{keys.items.length} key{keys.items.length === 1 ? '' : 's'}{#if user.blocked}
								· <span class="text-red-500">blocked</span>{/if}</span
						>
					</span>
				</span>
				<span class="flex flex-wrap gap-2">
					{#if !self}
						<button class="btn" onclick={toggleAdmin} disabled={changingRole}
							>{admin ? 'Remove admin' : 'Make admin'}</button
						>
					{/if}
					{#if !user.blocked}
						<IssueKeyMenu trigger="btn btn-primary" user={user.id} />
					{/if}
				</span>
			</div>

			<div class="flex flex-col gap-2">
				<h2 class="font-bold">Questionnaire</h2>
				{#if draft}
					<form onsubmit={saveDraft} class="panel grid gap-4 p-4 sm:grid-cols-2">
						<label class="flex flex-col gap-1.5">
							<span class="eyebrow">First name</span>
							<input required bind:value={draft.firstName} class="px-3 py-2" />
						</label>
						<label class="flex flex-col gap-1.5">
							<span class="eyebrow">Last name</span>
							<input required bind:value={draft.lastName} class="px-3 py-2" />
						</label>
						<label class="flex flex-col gap-1.5">
							<span class="eyebrow">Studies</span>
							<select bind:value={draft.studies} class="px-3 py-2">
								<option value={true}>Yes</option>
								<option value={false}>No</option>
							</select>
						</label>
						{#if draft.studies}
							<label class="flex flex-col gap-1.5">
								<span class="eyebrow">Department</span>
								<input required bind:value={draft.department} class="px-3 py-2" />
							</label>
						{/if}
						<label class="flex flex-col gap-1.5">
							<span class="eyebrow">How they found the lab</span>
							<select bind:value={draft.referralSource} class="px-3 py-2">
								{#each REFERRALS as [value, text] (value)}
									<option {value}>{text}</option>
								{/each}
							</select>
						</label>
						<label class="flex flex-col gap-1.5">
							<span class="eyebrow">Familiar with DIY equipment</span>
							<select bind:value={draft.diyFamiliar} class="px-3 py-2">
								<option value={true}>Yes</option>
								<option value={false}>No</option>
							</select>
						</label>
						<div class="flex gap-2 sm:col-span-2">
							<button
								type="submit"
								class="btn btn-primary"
								disabled={!answersChanged || savingAnswers}
							>
								{savingAnswers ? 'Saving…' : 'Save answers'}
							</button>
							{#if answersChanged}
								<button
									type="button"
									class="btn"
									onclick={() => answers && (draft = { ...answers })}>Reset</button
								>
							{/if}
						</div>
					</form>
				{:else if answers === null}
					<div class="panel">
						<span class="block p-8 text-gray-400">Has not completed the questionnaire yet.</span>
					</div>
				{/if}
			</div>

			<div class="flex flex-col gap-2">
				<h2 class="font-bold">Keys</h2>
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
						<span class="block p-8 text-gray-400">This user has no keys yet.</span>
					{/each}
				</div>
			</div>
		</div>

		<aside class="w-full lg:sticky lg:top-6 lg:w-80 lg:shrink-0 xl:w-96">
			<div class="panel flex flex-col lg:max-h-[calc(100vh-6rem)]">
				<div class="flex items-center justify-between border-b px-4 py-2.5">
					<h2 class="eyebrow">Timeline</h2>
					<span class="eyebrow"
						>{(activity.items?.length ?? 0) + (entries.items?.length ?? 0)} events</span
					>
				</div>
				<div class="overflow-y-auto p-4">
					<Timeline
						activity={activity.items ?? []}
						entries={entries.items ?? []}
						{now}
						{actorName}
						{serialByKey}
					/>
				</div>
			</div>
		</aside>
	{/if}
</div>
