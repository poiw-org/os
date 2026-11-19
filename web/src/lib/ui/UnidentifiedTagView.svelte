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
	import { t } from '#lib/i18n.svelte.ts';
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
						title: t('Attach this tag?'),
						message: t(
							'Its door entries will be shown under {who}. This does not give the tag access to the door.',
							{ who: who ?? '' }
						),
						confirmLabel: t('Attach tag')
					}
				: {
						title: t('Detach this tag?'),
						message: t('It will be listed as unidentified again.'),
						confirmLabel: t('Detach')
					}
		);
		if (!accepted) return;
		busy = true;
		try {
			await attachUnidentifiedTag(tag.id, userId);
			tag = { ...tag, userId, attachedAt: userId ? new Date() : null };
			chosen = undefined;
			toast.success(userId ? t('Tag attached to {who}.', { who: who ?? '' }) : t('Tag detached.'));
		} catch (e) {
			toast.error(e instanceof Error ? e.message : t('Could not attach the tag.'));
		} finally {
			busy = false;
		}
	};

	const claims = $derived(tag.claims);
	const facts = $derived<[string, string][]>([
		[t('Tag S/N (read)'), tag.serialNumber ?? '-'],
		[t('Key id (jti)'), claims?.jti || '-'],
		[t('Issued to (sub)'), claims?.sub ? (nameOf(claims.sub) ?? claims.sub) : '-'],
		[t('Issued for S/N (aud)'), claims?.aud || '-'],
		[t('Issued at (iat)'), claims?.iat ? formatDate(new Date(claims.iat * 1000)) : '-'],
		[t('Valid not before (nbf)'), claims?.nbf ? formatDate(new Date(claims.nbf * 1000)) : '-'],
		[t('Expires at (exp)'), claims?.exp ? formatDate(new Date(claims.exp * 1000)) : '-'],
		[
			t('Signature'),
			claims ? (claims.verified ? t('Valid') : t('Did not verify')) : t('No key on the tag')
		],
		[t('First seen'), formatDateTime(tag.firstSeen)],
		[t('Last seen'), formatDateTime(tag.lastSeen)],
		[t('Times used'), String(tag.sightings)],
		[t('Last outcome'), tag.lastReason ? t(tag.lastReason) : t('Opened')]
	]);
</script>

<Breadcrumb crumbs={[{ label: t('Users'), href: '/os' }, { label: shortId(tag.id) }]} />

<div class="flex flex-wrap items-center gap-3">
	<span class="font-mono text-xl font-bold" title={tag.id}>{shortId(tag.id)}</span>
	{#if tag.userId}
		<span class="text-xs text-green-500">{t('attached')}</span>
	{:else}
		<span class="flex items-center gap-1.5 text-xs text-red-500"
			><span class="lamp lamp-red"></span>{t('unidentified tag')}</span
		>
	{/if}
</div>

{#if tag.userId}
	<div class="panel flex flex-wrap items-center justify-between gap-3 px-4 py-3 text-sm">
		<span
			>{t('Belongs to')}
			<a href={userPath(tag.userId)} class="font-bold underline underline-offset-4"
				>{nameOf(tag.userId) ?? tag.userId}</a
			>{#if tag.attachedAt}<span class="text-gray-400">
					· {t('since {date}', { date: formatDate(tag.attachedAt) })}</span
				>{/if}</span
		>
		<button type="button" class="btn" disabled={busy} onclick={() => attach(null)}
			>{t('Detach')}</button
		>
	</div>
{:else}
	<div
		class="panel flex flex-col gap-3 overflow-visible border-red-500/60 bg-red-500/5 px-4 py-3 text-sm"
	>
		<p>
			{t(
				'No key accounts for this tag: its key was deleted, its token did not verify, or it carries none. Attach it to the person it belongs to.'
			)}
			{#if claims?.sub && nameOf(claims.sub)}
				{t('Its token names')} <b>{nameOf(claims.sub)}</b>{claims.verified
					? ''
					: ` (${t('unverified')})`}.
			{/if}
		</p>
		<div class="flex flex-col gap-2 sm:flex-row sm:items-start">
			<div class="min-w-0 flex-1"><UserPicker users={people} bind:value={chosen} /></div>
			{#if claims?.sub && nameOf(claims.sub) && !chosen}
				<button type="button" class="btn" disabled={busy} onclick={() => attach(claims.sub)}
					>{t('Attach to {who}', { who: nameOf(claims.sub) ?? '' })}</button
				>
			{/if}
			<button
				type="button"
				class="btn"
				disabled={!chosen || busy}
				onclick={() => chosen && attach(chosen.value)}>{t('Attach')}</button
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
		<h2 class="font-bold">{t('Log')}</h2>
		<div class="panel">
			{#each entries.items as entry (entry.id)}
				<div class="row">
					<span class="flex items-center gap-3"
						><span class="lamp {entry.type == EntryType.SUCCESSFUL ? 'lamp-green' : 'lamp-red'}"
						></span>
						<span class="tabular-nums">{formatDateTime(entry.timestamp)}</span></span
					>
					<span class="text-xs text-gray-400"
						>{entry.type == EntryType.SUCCESSFUL ? t('Opened') : t(entry.reason ?? 'Denied')}</span
					>
				</div>
			{:else}
				<span class="block p-8 text-gray-400">{t('No entries yet.')}</span>
			{/each}
		</div>
	</div>
{/if}
