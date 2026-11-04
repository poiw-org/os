<script lang="ts">
	import {
		DOOR_ID,
		unlockDoor,
		FORCE_OPEN_STEPS,
		forceOpenDoor,
		type ForceOpenEvent,
		type ForceOpenStep
	} from '#lib/api.ts';
	import { reveal, swapIn, swapOut } from '#lib/motion.ts';
	import { toast } from '#lib/notify.svelte.ts';
	import DoorStage, { type DoorScene } from './DoorStage.svelte';

	// Admins talk to the door directly; members go through the server.
	let { admin, onclose }: { admin: boolean; onclose: () => void } = $props();

	const LABELS: Record<ForceOpenStep, string> = {
		auth: 'Auth',
		join: 'Channel',
		send: 'Command',
		ack: 'Door',
		unlock: 'Unlock',
		log: 'Log'
	};
	// What the user is waiting on while a step is under way.
	const HEADLINES: Record<ForceOpenStep, string> = {
		auth: 'Connecting',
		join: 'Connecting',
		send: 'Sending',
		ack: 'Waiting for the door',
		unlock: 'Opening',
		log: 'Logging'
	};

	type Phase = 'confirm' | 'running' | 'opened' | 'failed';

	let dialog = $state<HTMLDialogElement>();
	let phase = $state<Phase>('confirm');
	let events = $state<ForceOpenEvent[]>([]);
	let error = $state('');

	$effect(() => dialog?.showModal());

	// The latest event of each checkpoint.
	const latest = $derived(
		Object.fromEntries(events.map((e) => [e.step, e])) as Partial<
			Record<ForceOpenStep, ForceOpenEvent>
		>
	);
	const last = $derived(events.at(-1));
	const ackLatency = $derived(
		events.find((e) => e.step === 'ack' && e.state === 'done')?.latencyMs
	);
	// A sixth per checkpoint, half of one while it is under way.
	const progress = $derived(
		phase === 'opened'
			? 1
			: FORCE_OPEN_STEPS.reduce(
					(sum, step) =>
						sum + (latest[step]?.state === 'done' ? 1 : latest[step]?.state === 'active' ? 0.5 : 0),
					0
				) / FORCE_OPEN_STEPS.length
	);

	// The step under way, as the door scene shows it. A failure before the door answered is on the
	// internet side; one after the command went out is the door's.
	const doorScene = $derived<DoorScene>(
		phase === 'confirm'
			? 'idle'
			: phase === 'opened'
				? 'opened'
				: phase === 'failed'
					? last?.step === 'ack' || last?.step === 'unlock'
						? 'failed_door'
						: 'failed_net'
					: !last || last.step === 'auth' || last.step === 'join'
						? 'connecting'
						: last.step === 'send' || last.step === 'ack'
							? 'sending'
							: last.step === 'unlock' && last.state === 'active'
								? 'warning'
								: 'opening'
	);

	const headline = $derived(
		phase === 'confirm'
			? 'Unlock the door?'
			: phase === 'opened'
				? 'Door unlocked'
				: phase === 'failed'
					? 'Could not unlock'
					: last
						? HEADLINES[last.step]
						: 'Connecting'
	);
	const detail = $derived(
		phase === 'confirm'
			? `Sends a one-time unlock to door:${DOOR_ID}. It is logged under your name.`
			: phase === 'failed'
				? error
				: phase === 'opened'
					? latest.log?.state === 'done'
						? 'Recorded under your name'
						: 'It opened, but the entry was not logged'
					: (last?.text ?? '')
	);

	const run = async () => {
		phase = 'running';
		events = [];
		error = '';
		try {
			const { logged } = await (admin ? forceOpenDoor : unlockDoor)((e) => events.push(e));
			phase = 'opened';
			if (!logged) toast.error('The door opened, but the entry could not be logged.');
		} catch (e) {
			error = e instanceof Error ? e.message : 'Could not send the remote unlock command.';
			phase = 'failed';
		}
	};

	// Closing mid-command would hide the outcome of a door that may be opening.
	const close = () => phase !== 'running' && dialog?.close();

	const seconds = (ms: number) => `${(ms / 1000).toFixed(1)} s`;
</script>

<dialog
	bind:this={dialog}
	{onclose}
	oncancel={(e) => phase === 'running' && e.preventDefault()}
	onclick={(e) => e.target === dialog && close()}
	aria-labelledby="force-open-title"
	class="sheet m-auto w-[calc(100%-2rem)] max-w-sm overflow-hidden rounded-lg border bg-paper p-0 text-ink backdrop:bg-black/70"
>
	<div class="flex items-center justify-between border-b px-4 py-2.5">
		<h2 id="force-open-title" class="text-sm">Remote unlock</h2>
		<button
			type="button"
			onclick={close}
			disabled={phase === 'running'}
			aria-label="Close"
			class="-mr-2 flex h-7 w-7 items-center justify-center rounded-md text-gray-400 hover:bg-paper-2 hover:text-ink disabled:opacity-30"
			>✕</button
		>
	</div>

	<div class="flex flex-col items-center px-6 pt-6 pb-6 text-center">
		<!-- The door, its terminal and their cables follow the command step by step. -->
		<div
			class="w-full"
			role="progressbar"
			aria-label="Unlock progress"
			aria-valuemin={0}
			aria-valuemax={100}
			aria-valuenow={Math.round(progress * 100)}
		>
			<DoorStage scene={doorScene} />
		</div>

		<!-- Each line swaps in place as the command moves on. -->
		<div
			class="mt-6 w-full"
			role={phase === 'failed' ? 'alert' : 'status'}
			aria-live={phase === 'failed' ? 'assertive' : 'polite'}
		>
			<div class="grid">
				{#key headline}
					<p class="font-mono text-lg font-bold [grid-area:1/1]" in:swapIn out:swapOut>
						{headline}
					</p>
				{/key}
			</div>
			<div class="mt-1 grid min-h-10">
				{#key detail}
					<p
						class="text-sm text-balance [grid-area:1/1] {phase === 'failed'
							? 'text-red-500'
							: 'text-gray-400'}"
						in:swapIn
						out:swapOut
					>
						{detail}
					</p>
				{/key}
			</div>
		</div>

		{#if phase !== 'confirm'}
			<div class="w-full" in:reveal>
				<dl class="mt-5 grid w-full grid-cols-3 divide-x border-y py-3 text-center">
					<div>
						<dt class="eyebrow">Door reply</dt>
						<dd class="mt-0.5 font-mono tabular-nums">
							{ackLatency === undefined ? '—' : `${ackLatency} ms`}
						</dd>
					</div>
					<div>
						<dt class="eyebrow">Elapsed</dt>
						<dd class="mt-0.5 font-mono tabular-nums">
							{phase === 'running' || !last ? '—' : seconds(last.at)}
						</dd>
					</div>
					<div>
						<dt class="eyebrow">Logged</dt>
						<dd class="mt-0.5 font-mono">
							{latest.log?.state === 'done' ? 'Yes' : latest.log?.state === 'failed' ? 'No' : '—'}
						</dd>
					</div>
				</dl>

				<details class="group w-full border-b text-left">
					<summary
						class="flex cursor-pointer list-none items-center justify-between py-2.5 text-xs text-gray-400 hover:text-ink [&::-webkit-details-marker]:hidden"
					>
						Steps
						<svg
							viewBox="0 0 12 12"
							class="h-3 w-3 transition-transform duration-200 group-open:rotate-90 motion-reduce:transition-none"
							fill="none"
							stroke="currentColor"
							stroke-width="1.5"
							stroke-linecap="round"
							stroke-linejoin="round"
							aria-hidden="true"><path d="m4.5 2.5 3.5 3.5-3.5 3.5" /></svg
						>
					</summary>
					<ol class="pb-3 text-xs">
						{#each FORCE_OPEN_STEPS as step (step)}
							{@const e = latest[step]}
							<li class="flex items-center gap-2 py-1">
								<span
									class="h-1.5 w-1.5 shrink-0 rounded-full {e?.state === 'done'
										? 'bg-green-500'
										: e?.state === 'failed'
											? 'bg-red-500'
											: e?.state === 'active'
												? 'bg-brand-text'
												: 'bg-gray-300'}"
								></span>
								<span class="w-16 shrink-0 font-mono {e ? 'text-ink' : 'text-gray-400'}"
									>{LABELS[step]}</span
								>
								<span class="min-w-0 flex-1 truncate text-gray-400">{e?.text ?? ''}</span>
								{#if e?.latencyMs !== undefined}
									<span class="font-mono text-gray-400 tabular-nums">{e.latencyMs} ms</span>
								{/if}
							</li>
						{/each}
					</ol>
				</details>
			</div>
		{/if}

		<div class="mt-6 grid w-full grid-cols-2 gap-2">
			{#if phase === 'opened'}
				<button type="button" class="btn btn-primary col-span-2" onclick={close}>Done</button>
			{:else}
				<button type="button" class="btn" onclick={close} disabled={phase === 'running'}
					>Cancel</button
				>
				<button type="button" class="btn btn-primary" onclick={run} disabled={phase === 'running'}
					>{phase === 'running'
						? 'Unlocking…'
						: phase === 'failed'
							? 'Try again'
							: 'Unlock'}</button
				>
			{/if}
		</div>
	</div>
</dialog>
