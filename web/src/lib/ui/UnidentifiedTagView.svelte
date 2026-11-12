<script lang="ts">
	import { userPath } from '#lib/routes.ts';
	import {
		attachUnidentifiedTag,
		listAllUsers,
		listEntries,
		toEntry,
		toUnidentifiedTag
	} from '#lib/api.ts';
	import { formatDate, formatDateTime, shortId, withName } from '#lib/format.ts';
	import { liveRows, watchTable } from '#lib/live.svelte.ts';
	import { ask, toast } from '#lib/notify.svelte.ts';
	import { EntryType, type UnidentifiedTag, type UserOption } from '#lib/types.ts';
	import Breadcrumb from '#lib/ui/Breadcrumb.svelte';
	import UserPicker from '#lib/ui/UserPicker.svelte';

	let { tag: initial }: { tag: UnidentifiedTag } = $props();
	// svelte-ignore state_referenced_locally
	let tag = $state.raw(initial);

	let people = $state.raw<UserOption[]>([]);
	$effect(() => {
		listAllUsers().then(
			(list) =>
				(people = list
					.filter((user) => user.email)
					.map((user) => ({ value: user.id, email: user.email, label: withName(user) }))),
			() => {}
		);
	});

	const nameOf = (id: string | null | undefined) =>
		(id && people.find((user) => user.value === id)?.label) || undefined;

	const entries = liveRows({
		table: 'entries',
		load: async () =>
			(
				await listEntries({ tag: { keyId: tag.id, serialNumber: tag.serialNumber }, limit: 200 })
			).filter((entry) => entry.tagId === tag.id),
		map: toEntry,
		accept: (entry) => entry.tagId === tag.id,
		sort: (a, b) => b.timestamp.getTime() - a.timestamp.getTime()
	});

	// Another admin may attach it meanwhile.
	$effect(() =>
		watchTable('unidentified_tags', `id=eq.${tag.id}`, {
			status: () => {},
			subscribed: () => {},
			change: ({ eventType, new: row }) => {
				if (eventType !== 'DELETE') tag = toUnidentifiedTag(row);
			}
		})
	);

	let chosen = $state<UserOption>();
	let busy = $state(false);

	const attach = async (userId: string | null) => {
		const who = nameOf(userId);
		const accepted = await ask(
			userId
				? {
						title: 'Attach this tag?',
						message: `Its door entries will be shown as ${who}'s. This does not give the tag access to the door.`,
						confirmLabel: 'Attach tag'
					}
				: {
						title: 'Detach this tag?',
						message: 'It will be listed as unidentified again.',
						confirmLabel: 'Detach'
					}
		);
		if (!accepted) return;
		busy = true;
		try {
			await attachUnidentifiedTag(tag.id, userId);
			tag = { ...tag, userId, attachedAt: userId ? new Date() : null };
			chosen = undefined;
			toast.success(userId ? `Tag attached to ${who}.` : 'Tag detached.');
		} catch (e) {
			toast.error(e instanceof Error ? e.message : 'Could not attach the tag.');
		} finally {
			busy = false;
		}
	};

	const claims = $derived(tag.claims);
	const facts = $derived<[string, string][]>([
		['Tag S/N (read)', tag.serialNumber ?? '-'],
		['Key id (jti)', claims?.jti || '-'],
		['Issued to (sub)', claims?.sub ? (nameOf(claims.sub) ?? claims.sub) : '-'],
		['Issued for S/N (aud)', claims?.aud || '-'],
		['Issued at (iat)', claims?.iat ? formatDate(new Date(claims.iat * 1000)) : '-'],
		['Valid not before (nbf)', claims?.nbf ? formatDate(new Date(claims.nbf * 1000)) : '-'],
		['Expires at (exp)', claims?.exp ? formatDate(new Date(claims.exp * 1000)) : '-'],
		['Signature', claims ? (claims.verified ? 'Valid' : 'Did not verify') : 'No key on the tag'],
		['First seen', formatDateTime(tag.firstSeen)],
		['Last seen', formatDateTime(tag.lastSeen)],
		['Times used', String(tag.sightings)],
		['Last outcome', tag.lastReason ?? 'Opened']
	]);
</script>

<Breadcrumb crumbs={[{ label: 'Users', href: '/os' }, { label: shortId(tag.id) }]} />

<div class="flex flex-wrap items-center gap-3">
	<span class="font-mono text-xl font-bold" title={tag.id}>{shortId(tag.id)}</span>
	{#if tag.userId}
		<span class="text-xs text-green-500">attached</span>
	{:else}
		<span class="flex items-center gap-1.5 text-xs text-red-500"
			><span class="lamp lamp-red"></span>unidentified tag</span
		>
	{/if}
</div>

{#if tag.userId}
	<div class="panel flex flex-wrap items-center justify-between gap-3 px-4 py-3 text-sm">
		<span
			>Belongs to <a href={userPath(tag.userId)} class="font-bold underline underline-offset-4"
				>{nameOf(tag.userId) ?? tag.userId}</a
			>{#if tag.attachedAt}<span class="text-gray-400">
					· since {formatDate(tag.attachedAt)}</span
				>{/if}</span
		>
		<button type="button" class="btn" disabled={busy} onclick={() => attach(null)}>Detach</button>
	</div>
{:else}
	<div class="panel flex flex-col gap-3 border-red-500/60 bg-red-500/5 px-4 py-3 text-sm">
		<p>
			No key accounts for this tag: its key was deleted, its token did not verify, or it carries
			none. Attach it to the person it belongs to.
			{#if claims?.sub && nameOf(claims.sub)}
				Its token names <b>{nameOf(claims.sub)}</b>{claims.verified ? '' : ' (unverified)'}.
			{/if}
		</p>
		<div class="flex flex-col gap-2 sm:flex-row sm:items-start">
			<div class="min-w-0 flex-1"><UserPicker users={people} bind:value={chosen} /></div>
			{#if claims?.sub && nameOf(claims.sub) && !chosen}
				<button type="button" class="btn" disabled={busy} onclick={() => attach(claims.sub)}
					>Attach to {nameOf(claims.sub)}</button
				>
			{/if}
			<button
				type="button"
				class="btn"
				disabled={!chosen || busy}
				onclick={() => chosen && attach(chosen.value)}>Attach</button
			>
		</div>
	</div>
{/if}

<dl class="panel text-sm">
	{#each facts as [label, value] (label)}
		<div class="row">
			<dt class="text-gray-400">{label}</dt>
			<dd class="min-w-0 text-right font-mono break-all">{value}</dd>
		</div>
	{/each}
</dl>

{#if entries.items}
	<div class="flex flex-col gap-2">
		<h2 class="font-bold">Log</h2>
		<div class="panel">
			{#each entries.items as entry (entry.id)}
				<div class="row">
					<span class="flex items-center gap-3"
						><span class="lamp {entry.type == EntryType.SUCCESSFUL ? 'lamp-green' : 'lamp-red'}"
						></span>
						<span class="tabular-nums">{formatDateTime(entry.timestamp)}</span></span
					>
					<span class="text-xs text-gray-400"
						>{entry.type == EntryType.SUCCESSFUL ? 'Opened' : (entry.reason ?? 'Denied')}</span
					>
				</div>
			{:else}
				<span class="block p-8 text-gray-400">No entries yet.</span>
			{/each}
		</div>
	</div>
{/if}
