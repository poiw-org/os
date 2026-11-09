<script lang="ts">
	import { openDoorLink } from '#lib/api.ts';
	import Dropdown from '#lib/ui/Dropdown.svelte';

	// Admins only: Header mounts this for them, and the door channel admits nobody else.
	const OFFLINE_AFTER_MS = 25000;

	let { label = $bindable(''), onopen }: { label?: string; onopen: () => void } = $props();

	let online = $state<boolean | null>(null);

	const status = $derived(online === null ? 'Checking' : online ? 'Connected' : 'Offline');
	const lamp = $derived(online ? 'lamp-green' : online === false ? 'lamp-red' : 'lamp-gray');

	$effect(() => {
		label = `Door ${status.toLowerCase()}`;
	});

	$effect(() => {
		const started = Date.now();
		let seen = 0;
		const link = openDoorLink({
			door_status: () => {
				seen = Date.now();
				online = true;
			}
		});
		const tick = setInterval(() => {
			if (Date.now() - (seen || started) > OFFLINE_AFTER_MS) online = false;
		}, 2000);
		return () => {
			clearInterval(tick);
			link.stop();
		};
	});
</script>

<Dropdown trigger="key" align="bar">
	{#snippet label()}
		<span aria-hidden="true" class="lamp {lamp}"></span>Devices
	{/snippet}
	{#snippet children(close)}
		<div class="flex items-center gap-3 px-4 py-3">
			<span aria-hidden="true" class="lamp shrink-0 {lamp}"></span>
			<span class="flex min-w-0 flex-1 flex-col">
				<span class="font-mono font-bold">Door</span>
				<span class="text-xs text-gray-400" role="status">{status}</span>
			</span>
			<button
				type="button"
				class="btn btn-primary"
				aria-haspopup="dialog"
				onclick={() => {
					close();
					onopen();
				}}>Connect</button
			>
		</div>
	{/snippet}
</Dropdown>
