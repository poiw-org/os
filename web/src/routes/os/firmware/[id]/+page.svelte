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

	// Opened by `make release`. The hash in the link comes from the terminal that uploaded the build,
	// and an approval only goes through for exactly that hash on exactly this release. Members land
	// here after uploading; they cannot read releases, so they are only told to ask an admin.
	const id = page.params.id as string;
	const expected = page.url.searchParams.get('sha256') ?? '';
	const isAdmin = page.data.user.isAdmin as boolean;

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

	let busy = $state(false);
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
		const yes = await ask({
			title: 'Install this firmware?',
			message: `This code will run on the door. Only approve it if you know what changed and the uploader confirms this hash: ${expected}`,
			confirmLabel: 'Approve'
		});
		if (yes)
			await act(() => approveFirmware(id, expected), 'Approved. The door will fetch it now.');
	};
</script>

<svelte:head><title>Approve firmware</title></svelte:head>

<h1 class="mb-4 text-lg font-bold">Approve firmware</h1>

{#if !isAdmin}
	<div class="rounded-lg border p-4">
		<p class="text-sm" role="status">
			Your build is uploaded. Contact an admin to approve it; the door will not install it until
			they do. Approval has to happen within 24 hours.
		</p>
		<p class="mt-3 text-sm">Send them this link:</p>
		<p class="mt-1 font-mono text-sm break-all">{page.url.href}</p>
	</div>
{:else if !rows?.items}
	<Loading />
{:else if !release}
	<p role="alert">This release does not exist.</p>
{:else}
	<div class="rounded-lg border p-4">
		<p class="font-mono font-bold">{release.version}</p>
		<p class="mt-2 text-sm">Uploaded by <strong>{uploader ?? 'unknown'}</strong></p>
		{#if release.version.endsWith('-dirty')}
			<p class="mt-2 text-sm text-red-600" role="alert">
				This label says the build has uncommitted changes, so its code may not be in the repository.
			</p>
		{/if}
		<p class="mt-2 text-sm">
			Compare this hash with the one printed in the uploader's terminal. If someone else uploaded
			it, confirm the hash with them directly:
		</p>
		<p class="mt-1 font-mono text-sm break-all">{release.sha256}</p>
		<p class="mt-2 text-sm text-gray-500">
			{Math.round(release.size / 1024)} KB, uploaded {formatDate(new Date(release.createdAt))}
		</p>
		{#if !matches}
			<p class="mt-3 text-sm text-red-600" role="alert">
				This link does not carry the hash of this release, so it cannot be approved. Use the link
				opened by `make release`.
			</p>
		{:else if release.status !== 'pending'}
			<p class="mt-3 text-sm" role="status">
				No longer waiting for approval ({release.doorState ?? release.status}).
			</p>
		{:else}
			<div class="mt-3 flex gap-2">
				<button
					type="button"
					class="key"
					disabled={busy}
					onclick={() => act(() => rejectFirmware(id), 'Rejected.')}>Reject</button
				>
				<button type="button" class="key key-signal" disabled={busy} onclick={approve}
					>Approve</button
				>
			</div>
		{/if}
	</div>
{/if}
