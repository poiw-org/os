<script lang="ts">
	import { watchBluetoothRequests, type BluetoothRequest } from '#lib/api.ts';
	import { toast } from '#lib/notify.svelte.ts';

	// Asks every signed-in admin about a device connecting to the door's Bluetooth console. The door
	// keeps the clock and the decision; this only shows its request and passes on the answer.
	type Shown = BluetoothRequest & { expiresAt: number };

	let dialog = $state<HTMLDialogElement>();
	let request = $state<Shown | null>(null);
	let decided = $state<'approve' | 'deny' | null>(null);
	let now = $state(Date.now());
	let watcher: ReturnType<typeof watchBluetoothRequests>;

	const secondsLeft = $derived(
		request ? Math.max(0, Math.ceil((request.expiresAt - now) / 1000)) : 0
	);

	$effect(() => {
		watcher = watchBluetoothRequests({
			request: (r) => {
				if (r.id !== request?.id) decided = null;
				request = { ...r, expiresAt: Date.now() + r.leftMs };
				now = Date.now();
			},
			ended: (id, outcome) => {
				if (id !== request?.id) return;
				if (decided && outcome === 'approved') toast.success('Bluetooth connection approved.');
				if (decided && outcome === 'denied') toast.info('Bluetooth connection denied.');
				request = null;
			}
		});
		return () => watcher.stop();
	});

	$effect(() => {
		if (!request) return;
		const tick = setInterval(() => {
			now = Date.now();
			// The door's result can be lost with the connection; the window is over either way.
			if (request && now > request.expiresAt + 2000) request = null;
		}, 250);
		return () => clearInterval(tick);
	});

	$effect(() => {
		if (request && dialog && !dialog.open) dialog.showModal();
	});

	const decide = (approve: boolean) => {
		if (!request) return;
		decided = approve ? 'approve' : 'deny';
		watcher.decide(request.id, approve);
	};
</script>

{#if request}
	<dialog
		bind:this={dialog}
		oncancel={(e) => e.preventDefault()}
		aria-labelledby="bluetooth-title"
		class="sheet m-auto w-[calc(100%-2rem)] max-w-sm overflow-hidden rounded-lg border bg-paper p-0 text-ink backdrop:bg-black/70"
	>
		<div class="border-b px-4 py-2.5">
			<h2 id="bluetooth-title" class="text-sm">Bluetooth connection</h2>
		</div>
		<div class="flex flex-col gap-4 px-6 py-6 text-center">
			<p>
				A device wants to use the door's console. Approving gives it full access to the console for
				24 hours.
			</p>
			<p class="font-mono text-lg font-bold">{request.address}</p>
			<p class="text-sm" role="timer" aria-live="off">
				Dropped in {secondsLeft} s unless you approve it.
			</p>
			<div class="flex justify-center gap-2">
				<button type="button" class="key" disabled={decided !== null} onclick={() => decide(false)}
					>Deny</button
				>
				<button
					type="button"
					class="key key-signal"
					disabled={decided !== null}
					onclick={() => decide(true)}>Approve</button
				>
			</div>
		</div>
	</dialog>
{/if}
