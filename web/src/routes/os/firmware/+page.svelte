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

	const DOOR_STATES: Record<string, { text: string; lamp: string }> = {
		downloading: { text: 'Door is downloading', lamp: 'lamp-signal' },
		rebooting: { text: 'Door is restarting into it', lamp: 'lamp-signal' },
		installed: { text: 'Installed', lamp: 'lamp-green' },
		rolled_back: {
			text: 'Failed its trial; the door went back to the previous firmware',
			lamp: 'lamp-red'
		},
		failed: { text: 'Door could not install it', lamp: 'lamp-red' }
	};

	const describe = (r: FirmwareRelease) => {
		if (r.status === 'pending')
			return { text: 'Waiting for approval from the link make release opens', lamp: 'lamp-amber' };
		if (r.status === 'rejected') return { text: 'Rejected', lamp: 'lamp-gray lamp-off' };
		return r.doorState
			? DOOR_STATES[r.doorState]
			: { text: 'Approved; waiting for the door', lamp: 'lamp-blue' };
	};
</script>

<svelte:head><title>Firmware</title></svelte:head>

<div class="stagger flex max-w-4xl flex-col gap-6 px-4 py-6 md:px-8 md:py-10">
	<h1 class="text-2xl">Firmware</h1>

	{#if !releases.items}
		<Loading />
	{:else if releases.items.length === 0}
		<p class="panel px-4 py-8 text-center text-gray-400">
			No firmware uploaded yet. Publish a build with <code class="font-mono">make release</code>.
		</p>
	{:else}
		<ul class="panel">
			{#each releases.items as r (r.id)}
				{@const state = describe(r)}
				<li class="row flex-col items-stretch gap-2 py-4 md:flex-row md:items-start md:gap-8">
					<div class="min-w-0 flex-1">
						<p class="flex items-center gap-2.5">
							<span class="lamp shrink-0 {state.lamp}" aria-hidden="true"></span>
							<span class="truncate font-mono font-bold">{r.version}</span>
						</p>
						<p class="mt-1 truncate font-mono text-xs text-gray-400 select-all" title={r.sha256}>
							{r.sha256}
						</p>
					</div>
					<div class="flex flex-col gap-0.5 md:w-72 md:shrink-0 md:text-right">
						<p role="status">{state.text}</p>
						<p class="text-xs text-gray-400">
							{Math.round(r.size / 1024)} KB · {formatDate(new Date(r.createdAt))}
						</p>
						{#if r.doorError}<p class="text-xs break-words text-red-400">{r.doorError}</p>{/if}
					</div>
				</li>
			{/each}
		</ul>
	{/if}
</div>
