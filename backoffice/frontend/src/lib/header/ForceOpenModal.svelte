<script lang="ts">
	import Fa from 'svelte-fa';
	import { faDoorOpen } from '@fortawesome/free-solid-svg-icons';
	import {
		DOOR_ID,
		unlockDoor,
		FORCE_OPEN_STEPS,
		forceOpenDoor,
		type ForceOpenEvent,
		type ForceOpenStep
	} from '#lib/api.ts';
	import { toast } from '#lib/notify.svelte.ts';

	// Admins talk to the door directly; members go through the server.
	let { admin, onclose }: { admin: boolean; onclose: () => void } = $props();

	const LABELS: Record<ForceOpenStep, string> = {
		auth: 'Auth',
		join: 'Channel',
		send: 'Command',
		ack: 'Door',
		unlock: 'Unlock',
		log: 'Logged'
	};

	type Phase = 'confirm' | 'running' | 'opened' | 'failed';

	let dialog = $state<HTMLDialogElement>();
	let terminal = $state<HTMLOListElement>();
	let phase = $state<Phase>('confirm');
	let events = $state<ForceOpenEvent[]>([]);
	let error = $state('');

	$effect(() => dialog?.showModal());

	// The latest state of each checkpoint.
	const stepState = $derived(
		Object.fromEntries(events.map((e) => [e.step, e.state])) as Partial<
			Record<ForceOpenStep, ForceOpenEvent['state']>
		>
	);
	const ackLatency = $derived(
		events.find((e) => e.step === 'ack' && e.state === 'done')?.latencyMs
	);

	const push = (event: ForceOpenEvent) => {
		events.push(event);
		queueMicrotask(() => terminal?.lastElementChild?.scrollIntoView({ block: 'nearest' }));
	};

	const run = async () => {
		phase = 'running';
		events = [];
		error = '';
		try {
			const { logged } = await (admin ? forceOpenDoor : unlockDoor)(push);
			phase = 'opened';
			if (!logged) toast.error('The door opened, but the entry could not be logged.');
		} catch (e) {
			error = e instanceof Error ? e.message : 'Could not send the remote unlock command.';
			phase = 'failed';
		}
	};

	// Closing mid-command would hide the outcome of a door that may be opening.
	const close = () => phase !== 'running' && dialog?.close();

	const pad = (ms: number) => `+${ms}ms`.padStart(8, ' ');
</script>

<dialog
	bind:this={dialog}
	{onclose}
	oncancel={(e) => phase === 'running' && e.preventDefault()}
	onclick={(e) => e.target === dialog && close()}
	aria-labelledby="force-open-title"
	class="sheet m-auto w-[calc(100%-2rem)] max-w-xl overflow-hidden rounded-2xl border bg-paper p-0 text-ink shadow-2xl backdrop:bg-black/60 backdrop:backdrop-blur-sm"
>
	<div class="relative p-6">
		<button
			type="button"
			onclick={close}
			disabled={phase === 'running'}
			aria-label="Close"
			class="absolute top-4 right-4 flex h-8 w-8 items-center justify-center rounded-full text-gray-400 hover:bg-paper-2 hover:text-ink disabled:opacity-30"
			>✕</button
		>
		<h2 id="force-open-title" class="text-lg font-semibold tracking-tight">Remote unlock</h2>
		<p class="mt-1 text-sm text-gray-400">
			Sends a one-time unlock to <span class="font-mono text-gray-500">door:{DOOR_ID}</span>. It is
			logged under your name as a remote unlock.
		</p>

		<!-- Checkpoints of the command choreography -->
		<ol class="mt-6 grid grid-cols-6" aria-label="Progress">
			{#each FORCE_OPEN_STEPS as step, i (step)}
				{@const state = stepState[step]}
				<li
					class="relative flex flex-col items-center gap-2 text-center"
					aria-current={state === 'active' ? 'step' : undefined}
				>
					{#if i > 0}
						<span class="absolute top-3 right-1/2 h-px w-full bg-gray-200" aria-hidden="true">
							<span
								class="block h-full origin-left transition-transform duration-300 ease-out motion-reduce:transition-none {state ===
								'failed'
									? 'bg-red-500'
									: 'bg-green-500'}"
								style:transform="scaleX({state ? 1 : 0})"
							></span>
						</span>
					{/if}
					<span
						class="dot relative z-10 flex h-6 w-6 items-center justify-center rounded-full border font-mono text-[11px] transition-colors duration-200
						{state === 'done'
							? 'border-green-500 bg-paper text-green-500'
							: state === 'failed'
								? 'border-red-500 bg-paper text-red-500'
								: state === 'active'
									? 'active border-ink bg-paper text-ink'
									: 'bg-paper text-gray-400'}"
					>
						{state === 'done' ? '✓' : state === 'failed' ? '!' : i + 1}
					</span>
					<span class="text-xs {state ? 'text-ink' : 'text-gray-400'}">{LABELS[step]}</span>
				</li>
			{/each}
		</ol>

		<!-- Transcript of the exchanges -->
		<div class="mt-6 overflow-hidden rounded-xl border bg-void text-[12px] leading-5 text-gray-400">
			<div class="flex items-center justify-between border-b border-gray-200 px-3 py-1.5">
				<span class="font-mono text-[11px] text-gray-500">realtime · door:{DOOR_ID}</span>
				{#if ackLatency !== undefined}
					<span class="font-mono text-[11px] text-green-400">door rtt {ackLatency} ms</span>
				{/if}
			</div>
			<ol
				bind:this={terminal}
				class="h-48 overflow-y-auto px-3 py-2 font-mono"
				role="log"
				aria-live="polite"
			>
				{#each events as e, i (i)}
					<li
						class="flex gap-2 whitespace-pre {e.state === 'failed'
							? 'text-red-400'
							: e.state === 'done'
								? 'text-ink'
								: 'text-gray-400'}"
					>
						<span class="text-gray-300">{pad(e.at)}</span>
						<span
							class={e.dir === '>'
								? 'text-blue-500'
								: e.dir === '<'
									? 'text-green-400'
									: 'text-gray-300'}>{e.dir}</span
						>
						<span class="min-w-0 flex-1 truncate">{e.text}</span>
						{#if e.latencyMs !== undefined}<span class="text-yellow-500">{e.latencyMs} ms</span
							>{/if}
					</li>
				{:else}
					<li class="text-gray-400">Idle. Nothing has been sent.</li>
				{/each}
				{#if phase === 'running'}<li class="cursor text-gray-500" aria-hidden="true">▍</li>{/if}
			</ol>
		</div>

		{#if phase === 'failed'}
			<p role="alert" class="mt-4 text-sm text-red-500">{error}</p>
		{:else if phase === 'opened'}
			<p role="status" class="mt-4 text-sm text-green-600">The door unlocked.</p>
		{/if}

		<div class="mt-6 flex justify-end gap-2">
			{#if phase === 'opened'}
				<button type="button" class="btn btn-primary" onclick={close}>DONE</button>
			{:else}
				<button type="button" class="btn" onclick={close} disabled={phase === 'running'}
					>CANCEL</button
				>
				<button type="button" class="btn btn-primary" onclick={run} disabled={phase === 'running'}
					><Fa icon={faDoorOpen} />{phase === 'running'
						? 'UNLOCKING…'
						: phase === 'failed'
							? 'TRY AGAIN'
							: 'REMOTE UNLOCK'}</button
				>
			{/if}
		</div>
	</div>
</dialog>

<style>
	.dot.active::after {
		content: '';
		position: absolute;
		inset: -4px;
		border-radius: 9999px;
		border: 1px solid currentColor;
		animation: ping 1.1s cubic-bezier(0, 0, 0.2, 1) infinite;
	}
	@keyframes ping {
		from {
			opacity: 0.6;
			transform: scale(0.85);
		}
		to {
			opacity: 0;
			transform: scale(1.5);
		}
	}
	.cursor {
		animation: blink 1s steps(2) infinite;
	}
	@keyframes blink {
		to {
			opacity: 0;
		}
	}
	@media (prefers-reduced-motion: reduce) {
		.dot.active::after,
		.cursor {
			animation: none;
		}
	}
</style>
