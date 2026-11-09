<script lang="ts">
	import { page } from '$app/state';
	import {
		approveFirmware,
		getFirmwareRelease,
		getFirmwareUploader,
		rejectFirmware,
		toFirmwareRelease
	} from '#lib/api.ts';
	import { formatDate } from '#lib/format.ts';
	import { liveRows } from '#lib/live.svelte.ts';
	import Loading from '#lib/loading.svelte';
	import { ask, toast } from '#lib/notify.svelte.ts';
	import ScaredPerson from '#lib/ScaredPerson.svelte';
	import Breadcrumb from '#lib/ui/Breadcrumb.svelte';

	// Opened by `make release`. The hash in the link comes from the terminal that uploaded the build,
	// and an approval only goes through for exactly that hash on exactly this release. Members land
	// here after uploading; they cannot read releases, so they are only told to ask an admin.
	const id = page.params.id as string;
	const expected = page.url.searchParams.get('sha256') ?? '';
	const isAdmin = page.data.user.isAdmin as boolean;
	const ACKNOWLEDGEMENT = 'this is dangerous';

	const rows = isAdmin
		? liveRows({
				table: 'firmware_releases',
				filter: `id=eq.${id}`,
				load: () => getFirmwareRelease(id),
				map: toFirmwareRelease
			})
		: null;
	const release = $derived(rows?.items?.find((r) => r.id === id));
	const matches = $derived(release?.sha256 === expected);

	// Unknown uploaders count too: a deleted account's build is still not the approver's own.
	const someoneElses = $derived(release?.uploadedBy !== page.data.user.id);

	let busy = $state(false);
	let typed = $state('');
	let uploader = $state<string | null>(null);

	if (isAdmin) getFirmwareUploader(id).then((email) => (uploader = email));

	const act = async (run: () => Promise<void>, done: string) => {
		busy = true;
		try {
			await run();
			toast.success(done);
		} catch (e) {
			toast.error(e instanceof Error ? e.message : 'That did not work.');
		}
		busy = false;
	};

	const approve = async () => {
		// Typing the warning is the confirmation for someone else's build.
		const yes =
			someoneElses ||
			(await ask({
				title: 'Install this firmware?',
				message: `This code will run on the door. Only approve it if you know what changed and the uploader confirms this hash: ${expected}`,
				confirmLabel: 'Approve'
			}));
		if (yes)
			await act(
				() => approveFirmware(id, expected, someoneElses ? typed : undefined),
				'Approved. The door will fetch it now.'
			);
	};
</script>

{#snippet fact(label: string, value: string)}
	<div class="row flex-col items-start gap-1 sm:flex-row sm:items-baseline sm:gap-6">
		<dt class="eyebrow sm:w-28 sm:shrink-0">{label}</dt>
		<dd class="min-w-0 flex-1">{value}</dd>
	</div>
{/snippet}

<svelte:head><title>Approve firmware</title></svelte:head>

<div class="stagger flex max-w-2xl flex-col gap-6 px-4 py-6 md:px-8 md:py-10">
	{#if isAdmin}
		<Breadcrumb crumbs={[{ label: 'Firmware', href: '/os/firmware' }, { label: 'Approve' }]} />
	{/if}
	<h1 class="text-2xl">Approve firmware</h1>

	{#if !isAdmin}
		<div class="panel flex flex-col gap-3 p-4">
			<p role="status">
				Your build is uploaded. Contact an admin to approve it; the door will not install it until
				they do. Approval has to happen within 24 hours.
			</p>
			<div class="flex flex-col gap-1 border-t pt-3">
				<span class="eyebrow">Send them this link</span>
				<p class="font-mono text-xs break-all select-all">{page.url.href}</p>
			</div>
		</div>
	{:else if !rows?.items}
		<Loading />
	{:else if !release}
		<p role="alert">This release does not exist.</p>
	{:else}
		<dl class="panel">
			{@render fact('Version', release.version)}
			{@render fact('Uploaded by', uploader ?? 'unknown')}
			{@render fact(
				'Build',
				`${Math.round(release.size / 1024)} KB, ${formatDate(new Date(release.createdAt))}`
			)}
			<div class="row flex-col items-start gap-1 sm:flex-row sm:items-baseline sm:gap-6">
				<dt class="eyebrow sm:w-28 sm:shrink-0">SHA-256</dt>
				<dd class="flex min-w-0 flex-1 flex-col gap-1.5">
					<span class="font-mono text-xs break-all select-all">{release.sha256}</span>
					<span class="text-xs text-gray-400">
						Compare this hash with the one printed in the uploader's terminal. If someone else
						uploaded it, confirm the hash with them directly.
					</span>
				</dd>
			</div>
		</dl>

		{#if release.version.endsWith('-dirty')}
			<p class="text-red-400" role="alert">
				This label says the build has uncommitted changes, so its code may not be in the repository.
			</p>
		{/if}

		{#if !matches}
			<p class="text-red-400" role="alert">
				This link does not carry the hash of this release, so it cannot be approved. Use the link
				opened by <code class="font-mono">make release</code>.
			</p>
		{:else if release.status !== 'pending'}
			<p role="status">No longer waiting for approval ({release.doorState ?? release.status}).</p>
		{:else}
			{#if someoneElses}
				<section
					class="flex flex-col gap-3 border-2 !border-red-500 bg-red-500/10 p-4 sm:p-5"
					role="alert"
				>
					<ScaredPerson />
					<h2 class="text-xl text-red-400">You do not know what is in this binary.</h2>
					<p>
						{uploader ?? 'Someone else'} uploaded it, not you. Once approved it runs on the door and can
						do anything the door can do, including letting anyone in. A label and a hash cannot tell you
						what the code does.
					</p>
					<p class="font-bold">
						You may want to check the code yourself before approving. If you cannot vouch for it,
						reject it.
					</p>
					<label class="flex flex-col gap-1.5 border-t !border-red-500/40 pt-3">
						<span>To approve anyway, type <strong class="font-mono">{ACKNOWLEDGEMENT}</strong></span
						>
						<input
							bind:value={typed}
							class="w-full px-3.5 py-2.5 font-mono"
							autocomplete="off"
							autocapitalize="none"
							autocorrect="off"
							spellcheck="false"
						/>
					</label>
				</section>
			{/if}
			<div class="flex justify-end gap-2">
				<button
					type="button"
					class="key flex-1 sm:flex-none"
					disabled={busy}
					onclick={() => act(() => rejectFirmware(id), 'Rejected.')}>Reject</button
				>
				<button
					type="button"
					class="key key-signal flex-1 sm:flex-none"
					disabled={busy || (someoneElses && typed !== ACKNOWLEDGEMENT)}
					onclick={approve}>Approve</button
				>
			</div>
		{/if}
	{/if}
</div>
