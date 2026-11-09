<script lang="ts">
	import { openDoorLink, sendDoorCommand } from '#lib/api.ts';
	import { toast } from '#lib/notify.svelte.ts';

	// Admins only: Header mounts this for them, and the door channel and the command function refuse
	// anyone else. The door decides what runs; output is shown as plain text.
	const LEASE_RENEW_MS = 10000;
	const MAX_OUTPUT = 20000;

	let { label, onclose }: { label: string; onclose: () => void } = $props();

	let dialog = $state<HTMLDialogElement>();
	let screen = $state<HTMLPreElement>();
	let output = $state('');
	let command = $state('');
	let sending = $state(false);

	// The door streams only while this keeps renewing, so a closed tab ends the session by itself.
	$effect(() => {
		dialog?.showModal();
		const link = openDoorLink({
			console_output: (payload) => {
				output = (output + String(payload.text ?? '')).slice(-MAX_OUTPUT);
			}
		});
		const renew = () => link.send('console_open', { id: crypto.randomUUID(), ts: Date.now() });
		renew();
		const timer = setInterval(renew, LEASE_RENEW_MS);
		return () => {
			clearInterval(timer);
			link.stop();
		};
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
</script>

<dialog
	bind:this={dialog}
	{onclose}
	onclick={(e) => e.target === dialog && dialog?.close()}
	aria-labelledby="door-console-title"
	class="sheet m-auto w-[calc(100%-2rem)] max-w-3xl overflow-hidden rounded-lg border bg-paper p-0 text-ink backdrop:bg-black/70"
>
	<div class="flex items-center justify-between border-b px-4 py-2.5">
		<h2 id="door-console-title" class="text-sm">Door console · {label}</h2>
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
		class="h-[60vh] overflow-auto bg-black p-4 font-mono text-xs leading-relaxed break-words whitespace-pre-wrap text-gray-100">{output ||
			'Waiting for output from the door…'}</pre>
	<form onsubmit={submit} class="flex items-center gap-2 border-t p-3">
		<span aria-hidden="true" class="font-mono text-sm text-gray-400">&gt;</span>
		<input
			bind:value={command}
			aria-label="Command"
			autocomplete="off"
			spellcheck="false"
			placeholder="status, show, reboot, help"
			class="min-w-0 grow rounded-md border bg-paper px-2 py-1.5 font-mono text-sm text-ink"
		/>
		<button type="submit" class="btn px-4 py-1.5 text-sm" disabled={sending || !command.trim()}
			>Send</button
		>
	</form>
</dialog>
