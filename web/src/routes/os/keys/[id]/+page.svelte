<script lang="ts">
	import { keyPath, userPath } from '#lib/routes.ts';
	import { goto } from '$app/navigation';
	import { page } from '$app/state';
	import { resolve } from '$app/paths';
	import { onMount } from 'svelte';
	import {
		deleteKey as removeKey,
		getKey,
		getKeyBySerialNumber,
		setSerialReleased,
		listEntries,
		setKeyDisabled,
		toEntry
	} from '#lib/api.ts';
	import { formatDate, formatDateTime, shortId, withName } from '#lib/format.ts';
	import { liveRows, watchTable } from '#lib/live.svelte.ts';
	import { arrive } from '#lib/motion.ts';
	import { nfcSupported, scanTag } from '#lib/nfc.ts';
	import { ask, toast } from '#lib/notify.svelte.ts';
	import { sameSerial } from '#lib/serial.ts';
	import { keyStatus } from '#lib/status.ts';
	import Loading from '#lib/loading.svelte';
	import { EntryType, type Entry, type Key } from '#lib/types.ts';
	import Breadcrumb from '#lib/ui/Breadcrumb.svelte';
	import KeyStatusBadge from '#lib/ui/KeyStatusBadge.svelte';
	import V1Badge from '#lib/ui/V1Badge.svelte';
	import V2Badge from '#lib/ui/V2Badge.svelte';
	import VirtualBadge from '#lib/ui/VirtualBadge.svelte';
	import WriteCardModal from '#lib/header/WriteCardModal.svelte';
	import RenewModal from '#lib/header/RenewModal.svelte';

	const PREVIEW = 50;
	const keyId = page.params.id ?? '';
	// Members see their own key and can disable it; everything else is for admins.
	const admin = page.data.user.isAdmin as boolean;

	let key = $state.raw<Key>();
	let selectedId = $state<number>();
	let showAllEntries = $state(false);
	let writerOpen = $state(false);
	let renewOpen = $state(false);
	// Only meaningful while the key has released its serial: the key that holds it now.
	let serialHolder = $state.raw<Key>();
	$effect(() => {
		const current = key;
		serialHolder = undefined;
		if (!current?.serialReleased || !current.serialNumber) return;
		getKeyBySerialNumber(current.serialNumber)
			.then((holder) => {
				if (key === current) serialHolder = holder?.id === current.id ? undefined : holder;
			})
			.catch(() => {});
	});

	// Entries stream in for this key only (server-side filter, further limited by RLS).
	const entries = liveRows({
		table: 'entries',
		filter: `key_id=eq.${keyId}`,
		load: () => listEntries({ keyIds: [keyId] }),
		map: toEntry,
		sort: (a, b) => b.timestamp.getTime() - a.timestamp.getTime()
	});
	let processing = $state(true);
	let status = $state('');
	let loadError = $state('');

	// A tag write stops on its own after this long, so a forgotten scan does not linger.
	const WRITE_TIMEOUT_MS = 60_000;
	let stopWrite = $state.raw<() => void>();
	const checkOut = async () => {
		if (!key?.serialNumber || !key.jwt) return;

		processing = true;
		status = `Please approach the tag with S/N ${key.serialNumber}`;
		let timer: ReturnType<typeof setTimeout> | undefined;
		let ended = false;
		const done = () => {
			ended = true;
			clearTimeout(timer);
			stopWrite = undefined;
			processing = false;
			status = '';
		};

		const stop = await scanTag(async (serialNumber, ndef, stop) => {
			// The key as it is now: a renew while waiting replaces its JWT.
			const live = key;
			status = 'Validating the tag…';
			if (!live?.serialNumber || !live.jwt) {
				toast.error('This key no longer has anything to write.');
				stop();
				return;
			}
			if (!sameSerial(live.serialNumber, serialNumber)) {
				toast.error("The tag's serial number has changed. Please re-issue the key.");
				stop();
				return;
			}
			status = `Writing key ${shortId(live.id)}`;
			try {
				await ndef.write(live.jwt);
				toast.success('Key written to the tag.');
			} catch (e) {
				toast.error(e instanceof Error ? e.message : 'Could not write to the tag.');
			}
			stop();
		}, done);
		if (!stop || ended) return;
		stopWrite = stop;
		timer = setTimeout(stop, WRITE_TIMEOUT_MS);
	};
	// Leaving the page ends a pending write.
	$effect(() => () => stopWrite?.());

	// Runs a key action, then continues. The key row is also streamed, so other admins' changes
	// show up here without a reload.
	const act = async (action: () => Promise<void>, next: () => void) => {
		try {
			await action();
		} catch (e) {
			toast.error(e instanceof Error ? e.message : 'Something went wrong.');
			return;
		}
		next();
	};

	const disableKey = async () => {
		if (!key) return;
		const accepted = await ask({
			title: 'Disable this key?',
			message: 'It will stop opening the door until you enable it again.',
			confirmLabel: 'Disable key'
		});
		if (!accepted) return;
		const { id } = key;
		await act(
			() => setKeyDisabled(id, true),
			() => {
				key = key && { ...key, disabled: true };
				toast.success('Key disabled.');
			}
		);
	};

	const releaseSerial = async () => {
		if (!key) return;
		const accepted = await ask({
			title: 'Release the tag number?',
			message: `Another key can then be issued on tag ${key.serialNumber}. This key stays disabled, and can only be enabled again while no other key uses that tag.`,
			confirmLabel: 'Release tag'
		});
		if (!accepted) return;
		const { id } = key;
		await act(
			() => setSerialReleased(id, true),
			() => {
				key = key && { ...key, serialReleased: true };
				toast.success('Tag number released.');
			}
		);
	};

	const enableKey = async () => {
		if (!key) return;
		const accepted = await ask({
			title: 'Enable this key?',
			message: 'It will open the door again, within its validity dates.',
			confirmLabel: 'Enable key'
		});
		if (!accepted) return;
		const { id } = key;
		await act(
			() => setKeyDisabled(id, false),
			() => {
				key = key && { ...key, disabled: false, serialReleased: false };
				toast.success('Key enabled.');
			}
		);
	};

	const deleteKey = async () => {
		if (!key) return;
		const accepted = await ask({
			title: 'Delete this key?',
			message:
				'This key has expired, so its tag no longer opens the door. Deleting it removes the key and its record here.',
			confirmLabel: 'Delete key',
			tone: 'danger'
		});
		if (!accepted) return;
		const { id } = key;
		await act(
			() => removeKey(id),
			async () => {
				toast.success('Key deleted.');
				await goto(resolve('/os'));
			}
		);
	};

	const reload = async () => {
		key = await getKey(keyId);
	};
	// The renewed token has to be written to the tag again.
	const reflash = (via: 'door' | 'nfc') => (via === 'door' ? (writerOpen = true) : checkOut());

	onMount(() => {
		(async () => {
			try {
				if (keyId) key = await getKey(keyId);
				if (!key) loadError = 'Key not found.';
			} catch (e) {
				loadError = e instanceof Error ? e.message : 'Could not load the key.';
			}
			processing = false;
		})();

		// Edits made elsewhere (renew, enable/disable, delete) arrive on the key row.
		return watchTable('keys', `id=eq.${keyId}`, {
			status: () => {},
			subscribed: () => {},
			change: ({ eventType, new: row }) => {
				if (eventType === 'DELETE') {
					key = undefined;
					loadError = 'This key was deleted.';
				} else if (key) {
					key = {
						...key,
						serialNumber: (row.serial_number as string | null) ?? null,
						notBefore: new Date(row.not_before as string),
						expiresAt: new Date(row.expires_at as string),
						disabled: row.disabled as boolean,
						jwt: (row.jwt as string | null) ?? ''
					};
				}
			}
		});
	});

	const showEntry = (entry: Entry) => {
		selectedId = selectedId === entry.id ? undefined : entry.id;
	};

	const keyState = $derived(key ? keyStatus(key) : undefined);
	const shownEntries = $derived(showAllEntries ? entries.items : entries.items?.slice(0, PREVIEW));
</script>

<div class="stagger flex max-w-4xl flex-col gap-6 px-4 py-6 md:px-8 md:py-10">
	{#if loadError}
		<span class="flex w-full justify-center px-10 pt-20 text-red-500">{loadError}</span>
	{:else if processing || !key}
		<span class="w-full text-gray-400">{status}</span>
		{#if stopWrite}
			<button type="button" class="btn self-start" onclick={() => stopWrite?.()}>Cancel</button>
		{/if}
		<Loading />
	{:else}
		<Breadcrumb
			crumbs={admin
				? [
						{ label: 'Users', href: '/os' },
						{ label: withName(key.user), href: { user: key.user.id } },
						{ label: shortId(key.id) }
					]
				: [{ label: 'My keys', href: '/os/keys' }, { label: shortId(key.id) }]}
		/>
		<div class="flex flex-wrap items-center justify-between gap-4">
			<span class="flex flex-wrap items-center gap-3">
				<span class="font-mono text-xl font-bold" title={key.id}>{shortId(key.id)}</span>
				{#if keyState}<KeyStatusBadge status={keyState} />{/if}
				{#if !key.serialNumber}<VirtualBadge />{/if}
				{#if key.v1}<V1Badge />{:else if key.v2}<V2Badge />{/if}
			</span>
			<span class="flex flex-wrap items-center gap-2">
				{#if admin}
					{#if !key.disabled}
						<button onclick={() => (renewOpen = true)} class="btn">Renew</button>
					{/if}
					{#if key.serialNumber && key.jwt}
						{#if nfcSupported}
							<button onclick={checkOut} class="btn">Write to tag</button>
						{/if}
						<button onclick={() => (writerOpen = true)} class="btn">Write at door</button>
					{/if}
				{/if}
				{#if key.disabled}
					{#if admin}
						{#if key.serialNumber && !key.serialReleased}
							<button onclick={releaseSerial} class="btn">Release tag</button>
						{/if}
						<button
							onclick={enableKey}
							disabled={!!serialHolder}
							title={serialHolder ? 'Its tag now belongs to another key' : undefined}
							class="btn disabled:cursor-not-allowed disabled:opacity-40">Enable</button
						>
					{/if}
				{:else}
					<button onclick={disableKey} class="btn">Disable</button>
				{/if}
				{#if admin && key.expiresAt <= new Date()}
					<button onclick={deleteKey} class="btn tone-red">Delete</button>
				{/if}
			</span>
		</div>

		{#if key.serialReleased}
			<div class="panel border-yellow-500/50 px-4 py-3 text-sm text-yellow-500">
				{#if serialHolder}
					Tag {key.serialNumber} was released and now belongs to
					<a href={keyPath(serialHolder.id)} class="font-mono underline underline-offset-4"
						>{shortId(serialHolder.id)}</a
					>. This key cannot be enabled unless that key is removed.
				{:else}
					Tag {key.serialNumber} was released for reuse. Enabling this key will claim it again, if no
					other key has taken it by then.
				{/if}
			</div>
		{/if}

		<dl class="panel text-sm">
			{#each [['User', withName(key.user)], ['Tag S/N', key.serialNumber ? key.serialNumber + (key.serialReleased ? ' (released)' : '') : 'Virtual (no tag)'], ['Valid not before', formatDate(key.notBefore)], ['Expires at', formatDate(key.expiresAt)], ['Issued at', formatDate(key.issuedAt)], ['Issued by', key.issuedBy || '-']] as [label, value] (label)}
				<div class="row">
					<dt class="text-gray-400">{label}</dt>
					<dd class="min-w-0 text-right break-all {label === 'Tag S/N' ? 'font-mono' : ''}">
						{#if label === 'User'}<a
								href={userPath(key.user.id)}
								class="underline-offset-4 hover:underline">{value}</a
							>{:else}{value}{/if}
					</dd>
				</div>
			{/each}
		</dl>
		{#if admin}
			<details class="panel text-sm">
				<summary class="row cursor-pointer text-gray-400 select-none">JWT</summary>
				<textarea
					disabled
					value={key.jwt}
					rows="4"
					class="block w-full resize-none rounded-none border-0 border-t p-4 font-mono text-xs break-all text-gray-600"
				></textarea>
			</details>
		{/if}
	{/if}

	{#if entries.items}
		<div class="flex flex-col gap-2">
			<h2 class="font-bold">Log</h2>
			<div class="panel">
				{#each shownEntries ?? [] as entry, at (entry.id)}
					<!-- svelte-ignore a11y_click_events_have_key_events, a11y_no_static_element_interactions -->
					<div
						in:arrive={{ index: at }}
						onclick={() => showEntry(entry)}
						class="row cursor-pointer hover:bg-brand/10"
					>
						<span class="flex items-center gap-3"
							><span class="lamp {entry.type == EntryType.SUCCESSFUL ? 'lamp-green' : 'lamp-red'}"
							></span>
							<span class="tabular-nums">{formatDateTime(entry.timestamp)}</span></span
						>
						<span class="text-xs text-gray-400"
							>{entry.type == EntryType.SUCCESSFUL ? 'Opened' : (entry.reason ?? 'Denied')}</span
						>
					</div>
					{#if selectedId === entry.id}
						<div class="flex flex-col gap-1 bg-paper-2 px-4 py-3 text-xs text-gray-600">
							<div>Occurred on: {formatDateTime(entry.timestamp)}</div>
							<div>
								Outcome: <span
									class={entry.type == EntryType.SUCCESSFUL ? 'text-green-400' : 'text-red-400'}
									>{entry.type == EntryType.SUCCESSFUL ? 'Success' : 'Failure'}</span
								>
							</div>
							{#if entry.type == EntryType.FAILED}
								<div>Reason: {entry.reason}</div>
							{/if}
							{#if entry.serialNumberFromTag}
								<div>Serial Number from Tag: {entry.serialNumberFromTag}</div>
							{/if}
						</div>
					{/if}
				{:else}
					<span class="block p-8 text-gray-400">No entries yet.</span>
				{/each}
			</div>
			{#if entries.items.length > PREVIEW}
				<button
					onclick={() => (showAllEntries = !showAllEntries)}
					class="self-start text-xs text-brand-text underline"
					>{showAllEntries ? 'Show fewer' : `Show all ${entries.items.length} entries`}</button
				>
			{/if}
		</div>
	{/if}
</div>

{#if renewOpen && key}
	<RenewModal {key} onclose={() => (renewOpen = false)} onrenewed={reload} onwrite={reflash} />
{/if}

{#if writerOpen && key?.serialNumber && key.jwt}
	<WriteCardModal
		token={key.jwt}
		serialNumber={key.serialNumber}
		onclose={() => (writerOpen = false)}
	/>
{/if}
