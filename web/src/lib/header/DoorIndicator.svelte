<script lang="ts">
	import { openDoorLink } from '#lib/api.ts';

	// Admins only: Header mounts this for them, and the door channel admits nobody else.
	const OFFLINE_AFTER_MS = 25000;

	let { label = $bindable(''), onopen }: { label?: string; onopen: () => void } = $props();

	let online = $state<boolean | null>(null);

	$effect(() => {
		label = online === null ? 'Door: checking' : online ? 'Door connected' : 'Door offline';
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

<button type="button" class="key" aria-haspopup="dialog" onclick={onopen}>
	<span
		aria-hidden="true"
		class="mr-1 inline-block h-2 w-2 rounded-full {online
			? 'bg-green-500'
			: online === false
				? 'bg-red-500'
				: 'bg-gray-400'}"
	></span>{label}
</button>
