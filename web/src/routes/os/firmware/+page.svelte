<script lang="ts">
	import {
		approveFirmware,
		listFirmwareReleases,
		rejectFirmware,
		toFirmwareRelease,
		type FirmwareRelease
	} from '#lib/api.ts';
	import { formatDate } from '#lib/format.ts';
	import { liveRows } from '#lib/live.svelte.ts';
	import Loading from '#lib/loading.svelte';
	import { ask, toast } from '#lib/notify.svelte.ts';

	const releases = liveRows({
		table: 'firmware_releases',
		load: listFirmwareReleases,
		map: toFirmwareRelease,
		sort: (a, b) => b.createdAt.localeCompare(a.createdAt),
		limit: 20
	});

	let busy = $state(false);

	const DOOR_STATES: Record<string, string> = {
		downloading: 'Door is downloading',
		rebooting: 'Door is restarting into it',
		installed: 'Installed',
		rolled_back: 'Failed its trial; the door went back to the previous firmware',
		failed: 'Door could not install it'
	};

	const describe = (r: FirmwareRelease) => {
		if (r.status === 'pending') return 'Waiting for approval';
		if (r.status === 'rejected') return 'Rejected';
		return r.doorState ? DOOR_STATES[r.doorState] : 'Approved; waiting for the door';
	};

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

	const approve = async (r: FirmwareRelease) => {
		const yes = await ask({
			title: 'Install this firmware?',
			message: `The door will download ${r.version} and restart into it when idle.`,
			confirmLabel: 'Approve'
		});
		if (yes) await act(() => approveFirmware(r.id), 'Approved. The door will fetch it now.');
	};
</script>

<svelte:head><title>Firmware</title></svelte:head>

<h1 class="mb-4 text-lg font-bold">Firmware</h1>

{#if !releases.items}
	<Loading />
{:else if releases.items.length === 0}
	<p class="text-gray-500">No firmware uploaded yet. Publish a build with `make release`.</p>
{:else}
	<ul class="flex flex-col gap-3">
		{#each releases.items as r (r.id)}
			<li class="rounded-lg border p-4">
				<p class="font-mono font-bold">{r.version}</p>
				<p class="text-sm break-all text-gray-500">{r.sha256}</p>
				<p class="mt-1 text-sm text-gray-500">
					{Math.round(r.size / 1024)} KB, uploaded {formatDate(new Date(r.createdAt))}
				</p>
				<p class="mt-2 text-sm" role="status">{describe(r)}</p>
				{#if r.doorError}<p class="mt-1 text-sm text-red-600">{r.doorError}</p>{/if}
				{#if r.status === 'pending'}
					<div class="mt-3 flex gap-2">
						<button
							type="button"
							class="key"
							disabled={busy}
							onclick={() => act(() => rejectFirmware(r.id), 'Rejected.')}>Reject</button
						>
						<button type="button" class="key key-signal" disabled={busy} onclick={() => approve(r)}
							>Approve</button
						>
					</div>
				{/if}
			</li>
		{/each}
	</ul>
{/if}
