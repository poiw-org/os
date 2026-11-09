<script lang="ts">
	import { openDoorLink, sendDoorCommand } from '#lib/api.ts';
	import { toast } from '#lib/notify.svelte.ts';

	// Admins only: Header mounts this for them, and the door channel and the command function refuse
	// anyone else. The door decides what runs; output is shown as plain text.
	const OFFLINE_AFTER_MS = 25000;
	const LEASE_RENEW_MS = 10000;
	const MAX_OUTPUT = 20000;

	let dialog = $state<HTMLDialogElement>();
	let screen = $state<HTMLPreElement>();
	let online = $state<boolean | null>(null);
	let open = $state(false);
	let output = $state('');
	let command = $state('');
	let sending = $state(false);
	let link: ReturnType<typeof openDoorLink>;

	$effect(() => {
		const started = Date.now();
		let seen = 0;
		link = openDoorLink({
			door_status: () => {
				seen = Date.now();
				online = true;
			},
			console_output: (payload) => {
				output = (output + String(payload.text ?? '')).slice(-MAX_OUTPUT);
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

	// The door streams only while this keeps renewing, so a closed tab ends the session by itself.
	$effect(() => {
		if (!open) return;
		dialog?.showModal();
		const renew = () => link.send('console_open', { id: crypto.randomUUID(), ts: Date.now() });
		renew();
		const timer = setInterval(renew, LEASE_RENEW_MS);
		return () => clearInterval(timer);
	});

	$effect(() => {
		void output;
		if (screen) screen.scrollTop = screen.scrollHeight;
	});

	const submit = async (event: SubmitEvent) => {
		event.preventDefault();
		const text = command.trim().toLowerCase();
		if (!text) return;
		sending = true;
		try {
			await sendDoorCommand(text);
			command = '';
		} catch (e) {
			toast.error(e instanceof Error ? e.message : 'The command was not sent.');
		}
		sending = false;
	};

	const label = $derived(
		online === null ? 'Door: checking' : online ? 'Door connected' : 'Door offline'
	);
</script>

<button type="button" class="key" aria-haspopup="dialog" onclick={() => (open = true)}>
	<span
		aria-hidden="true"
		class="mr-1 inline-block h-2 w-2 rounded-full {online
			? 'bg-green-500'
			: online === false
				? 'bg-red-500'
				: 'bg-gray-400'}"
	></span>{label}
</button>

{#if open}
	<dialog
		bind:this={dialog}
		onclose={() => (open = false)}
		aria-labelledby="door-console-title"
		class="sheet m-auto w-[calc(100%-2rem)] max-w-2xl overflow-hidden rounded-lg border bg-paper p-0 text-ink backdrop:bg-black/70"
	>
		<div class="flex items-center justify-between border-b px-4 py-2.5">
			<h2 id="door-console-title" class="text-sm">Door console: {label}</h2>
			<button
				type="button"
				onclick={() => dialog?.close()}
				aria-label="Close"
				class="-mr-2 flex h-7 w-7 items-center justify-center rounded-md text-gray-400 hover:bg-paper-2 hover:text-ink"
				>✕</button
			>
		</div>
		<pre
			bind:this={screen}
			role="log"
			aria-label="Door output"
			class="h-80 overflow-auto p-4 font-mono text-xs whitespace-pre-wrap">{output ||
				'Waiting for output from the door…'}</pre>
		<form onsubmit={submit} class="flex gap-2 border-t p-3">
			<input
				bind:value={command}
				aria-label="Command"
				autocomplete="off"
				spellcheck="false"
				placeholder="status, show, reboot, help"
				class="min-w-0 grow rounded-md border bg-paper px-2 py-1 font-mono text-sm"
			/>
			<button type="submit" class="key" disabled={sending || !command.trim()}>Send</button>
		</form>
	</dialog>
{/if}
