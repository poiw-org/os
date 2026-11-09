<script lang="ts">
	import { listFirmwareReleases, toFirmwareRelease, type FirmwareRelease } from '#lib/api.ts';
	import { formatDate } from '#lib/format.ts';
	import { liveRows } from '#lib/live.svelte.ts';
	import Loading from '#lib/loading.svelte';

	const releases = liveRows({
		table: 'firmware_releases',
		load: listFirmwareReleases,
		map: toFirmwareRelease,
		sort: (a, b) => b.createdAt.localeCompare(a.createdAt),
		limit: 20
	});

	const DOOR_STATES: Record<string, string> = {
		downloading: 'Door is downloading',
		rebooting: 'Door is restarting into it',
		installed: 'Installed',
		rolled_back: 'Failed its trial; the door went back to the previous firmware',
		failed: 'Door could not install it'
	};

	const describe = (r: FirmwareRelease) => {
		if (r.status === 'pending') return 'Waiting for approval, from the link `make release` opens';
		if (r.status === 'rejected') return 'Rejected';
		return r.doorState ? DOOR_STATES[r.doorState] : 'Approved; waiting for the door';
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
			</li>
		{/each}
	</ul>
{/if}
