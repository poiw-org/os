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
			{#if someoneElses}
				<section class="mt-4 rounded-md border-2 !border-red-500 bg-red-500/10 p-4" role="alert">
					<ScaredPerson />
					<h2 class="mt-3 text-lg font-bold text-red-600">
						You do not know what is in this binary.
					</h2>
					<p class="mt-2 text-sm">
						{uploader ?? 'Someone else'} uploaded it, not you. Once approved it runs on the door and can
						do anything the door can do, including letting anyone in. A label and a hash cannot tell you
						what the code does.
					</p>
					<p class="mt-2 text-sm font-bold">
						You may want to check the code yourself before approving. If you cannot vouch for it,
						reject it.
					</p>
					<label class="mt-3 block text-sm">
						To approve anyway, type <strong class="font-mono">{ACKNOWLEDGEMENT}</strong>
						<input
							bind:value={typed}
							class="mt-1 w-full px-3 py-2 font-mono"
							autocomplete="off"
							autocapitalize="none"
							autocorrect="off"
							spellcheck="false"
						/>
					</label>
				</section>
			{/if}
			<div class="mt-3 flex gap-2">
				<button
					type="button"
					class="key"
					disabled={busy}
					onclick={() => act(() => rejectFirmware(id), 'Rejected.')}>Reject</button
				>
				<button
					type="button"
					class="key key-signal"
					disabled={busy || (someoneElses && typed !== ACKNOWLEDGEMENT)}
					onclick={approve}>Approve</button
				>
			</div>
		{/if}
	</div>
{/if}
