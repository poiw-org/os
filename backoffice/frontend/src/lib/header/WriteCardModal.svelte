<script lang="ts">
	import Fa from 'svelte-fa';
	import { faMicrochip } from '@fortawesome/free-solid-svg-icons';
	import {
		DOOR_ID,
		WRITE_CARD_STEPS,
		writeCardViaDoor,
		type WriteCardEvent,
		type WriteCardState,
		type WriteCardStep
	} from '#lib/api.ts';

	let {
		token,
		serialNumber = '',
		onclose,
		onwritten,
		onscanned
	}: {
		/** The key to write. Leave out to only read a tag's serial number. */
		token?: string;
		serialNumber?: string;
		onclose: () => void;
		onwritten?: () => void;
		onscanned?: (serial: string) => void;
	} = $props();
	const scanning = !token;

	const LABELS: Record<WriteCardStep, string> = {
		auth: 'Auth',
		join: 'Channel',
		door: 'Door',
		tag: 'Tag',
		write: scanning ? 'Read' : 'Write'
	};

	type Phase = 'confirm' | 'running' | 'done' | 'failed';
	type Scene = 'idle' | 'connecting' | WriteCardState;

	let dialog = $state<HTMLDialogElement>();
	let phase = $state<Phase>('confirm');
	let scene = $state<Scene>('idle');
	let events = $state<WriteCardEvent[]>([]);
	let error = $state('');
	let remaining = $state(0);
	let scanned = $state('');
	// Alternates while the door waits, so the tag glides toward the reader and back.
	let near = $state(false);
	let controller: AbortController | undefined;

	$effect(() => dialog?.showModal());

	const stepState = $derived(
		Object.fromEntries(events.map((e) => [e.step, e.state])) as Partial<
			Record<WriteCardStep, WriteCardEvent['state']>
		>
	);
	const tail = $derived(serialNumber.slice(-5).toUpperCase());

	const copyFor = (): Record<Scene, { title: string; hint: string }> => ({
		idle: scanning
			? {
					title: 'Read a tag with the door',
					hint: 'The reader at the door reads the serial number of the next tag you hold to it. Stand at the door with the tag in your hand, then start.'
				}
			: {
					title: 'Write this key with the door',
					hint: `The reader at the door writes the key to the tag with S/N ${serialNumber}. Stand at the door with that tag in your hand, then start.`
				},
		connecting: {
			title: 'Putting the door in writer mode',
			hint: 'Reaching the door over its private channel.'
		},
		ready: {
			title: 'Hold the tag to the reader',
			hint: scanning
				? 'Lay it flat on the reader. The door beeps while it waits.'
				: 'Lay it flat on the reader and keep it there. The door beeps while it waits.'
		},
		wrong_tag: {
			title: 'That is a different tag',
			hint: `This key belongs to the tag ending ${tail}. Swap it for that one.`
		},
		writing: {
			title: 'Writing. Keep it still',
			hint: 'Do not lift the tag until the door plays the happy chord.'
		},
		done: scanning
			? { title: 'Tag read', hint: `Serial number ${scanned} is now in the form.` }
			: {
					title: 'Key written',
					hint: 'The door read it back and it matches. Tap the tag on the door to try it.'
				},
		failed: { title: scanning ? 'The tag was not read' : 'The tag was not written', hint: '' }
	});

	// While the door waits: the countdown, and the tag gliding toward the reader and back.
	let tick: ReturnType<typeof setInterval> | undefined;
	const stopTick = () => {
		clearInterval(tick);
		tick = undefined;
	};
	const onState = (state: WriteCardState, timeoutMs: number) => {
		scene = state;
		if (state === 'ready' && !tick) {
			const deadline = performance.now() + timeoutMs;
			remaining = Math.ceil(timeoutMs / 1000);
			near = true;
			tick = setInterval(() => {
				remaining = Math.max(0, Math.ceil((deadline - performance.now()) / 1000));
				near = !near;
			}, 1300);
		}
		if (state !== 'ready' && state !== 'wrong_tag') stopTick();
	};

	const run = async () => {
		phase = 'running';
		scene = 'connecting';
		events = [];
		error = '';
		near = false;
		controller = new AbortController();
		try {
			const result = await writeCardViaDoor(token, {
				signal: controller.signal,
				onEvent: (e) => events.push(e),
				onState
			});
			phase = 'done';
			if (scanning) {
				scanned = result?.serial ?? '';
				onscanned?.(scanned);
			} else onwritten?.();
		} catch (e) {
			stopTick();
			error = e instanceof Error ? e.message : 'Could not write the tag.';
			phase = 'failed';
			scene = 'failed';
		}
	};

	// Closing while the door waits takes it out of writer mode.
	const close = () => {
		controller?.abort();
		stopTick();
		dialog?.close();
	};

	const copy = $derived(copyFor()[scene]);
	const waiting = $derived(scene === 'ready' || scene === 'wrong_tag');
	const cardPose = $derived(
		scene === 'writing' || scene === 'done'
			? 'docked'
			: waiting
				? near
					? 'near'
					: 'far'
				: scene === 'connecting'
					? 'far'
					: 'away'
	);
	const waves = $derived(
		scene === 'writing' ? 'fast' : waiting ? 'slow' : scene === 'done' ? 'done' : 'off'
	);
</script>

<dialog
	bind:this={dialog}
	{onclose}
	onclick={(e) => e.target === dialog && close()}
	aria-labelledby="write-card-title"
	class="sheet m-auto w-[calc(100%-2rem)] max-w-xl overflow-hidden rounded-2xl border bg-paper p-0 text-ink shadow-2xl backdrop:bg-black/60 backdrop:backdrop-blur-sm"
>
	<div class="relative p-6">
		<button
			type="button"
			onclick={close}
			aria-label="Close"
			class="absolute top-4 right-4 flex h-8 w-8 items-center justify-center rounded-full text-gray-400 hover:bg-paper-2 hover:text-ink"
			>✕</button
		>
		<h2 id="write-card-title" class="text-lg font-semibold tracking-tight">
			{scanning ? 'Scan a tag' : 'Write to tag'}
		</h2>
		<p class="mt-1 text-sm text-gray-400">
			Uses the reader on <span class="font-mono text-gray-500">door:{DOOR_ID}</span> as a {scanning
				? 'scanner'
				: 'writer'}.
		</p>

		<!-- Checkpoints -->
		<ol class="mt-6 grid grid-cols-5" aria-label="Progress">
			{#each WRITE_CARD_STEPS as step, i (step)}
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

		<!-- The reader and the tag -->
		<div
			class="stage relative mt-6 h-60 overflow-hidden rounded-xl border bg-paper-2"
			data-scene={scene}
			data-waves={waves}
			aria-hidden="true"
		>
			<div class="waves">
				<span class="wave" style:--i="0"></span>
				<span class="wave" style:--i="1"></span>
				<span class="wave" style:--i="2"></span>
			</div>

			<div class="card" data-pose={cardPose}>
				<div class="card-face">
					<svg
						viewBox="0 0 24 24"
						class="h-6 w-6"
						fill="none"
						stroke="currentColor"
						stroke-width="1.8"
						stroke-linecap="round"
					>
						<path d="M8.5 8.5a5 5 0 0 1 0 7" />
						<path d="M12 6a8.5 8.5 0 0 1 0 12" />
						<path d="M15.5 3.5a12 12 0 0 1 0 17" />
					</svg>
					<span class="chip"></span>
					<span class="mt-auto font-mono text-[10px] tracking-wider opacity-70"
						>{scanning ? '···' : `···${tail}`}</span
					>
					<span class="sheen"></span>
				</div>
				<span class="badge" data-show={scene === 'done'}>✓</span>
			</div>

			<div class="pad">
				<span class="led"></span>
				<Fa icon={faMicrochip} class="text-gray-400" />
				<span class="font-mono text-[10px] tracking-widest text-gray-400">PN532</span>
			</div>
		</div>

		<div class="mt-5 min-h-16" aria-live="polite">
			<p class="text-base font-medium tracking-tight">{copy.title}</p>
			<p
				class="mt-1 text-sm {phase === 'failed' ? 'text-red-500' : 'text-gray-400'}"
				role={phase === 'failed' ? 'alert' : undefined}
			>
				{phase === 'failed' ? error : copy.hint}
			</p>
			{#if waiting}
				<p class="mt-2 font-mono text-xs text-gray-400">Writer mode ends in {remaining} s</p>
			{/if}
		</div>

		<div class="mt-4 flex justify-end gap-2">
			{#if phase === 'done'}
				<button type="button" class="btn btn-primary" onclick={close}>DONE</button>
			{:else}
				<button type="button" class="btn" onclick={close}>CANCEL</button>
				<button type="button" class="btn btn-primary" onclick={run} disabled={phase === 'running'}>
					{phase === 'running'
						? 'WAITING…'
						: phase === 'failed'
							? 'TRY AGAIN'
							: scanning
								? 'START SCANNING'
								: 'START WRITER MODE'}
				</button>
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

	.stage {
		--spring: cubic-bezier(0.34, 1.3, 0.64, 1);
		--tone: #9ca3af;
	}
	.stage[data-scene='ready'],
	.stage[data-scene='wrong_tag'] {
		--tone: currentColor;
	}
	.stage[data-scene='writing'] {
		--tone: #eab308;
	}
	.stage[data-scene='done'] {
		--tone: #22c55e;
	}
	.stage[data-scene='failed'] {
		--tone: #ef4444;
	}

	/* The reader pad, with a status light. */
	.pad {
		position: absolute;
		bottom: 28px;
		left: 50%;
		display: flex;
		width: 176px;
		height: 44px;
		translate: -50% 0;
		align-items: center;
		justify-content: center;
		gap: 10px;
		border-radius: 16px;
		border: 1px solid var(--tone);
		background: var(--color-paper, #fff);
		box-shadow: 0 8px 24px -12px rgb(0 0 0 / 0.35);
		transition: border-color 250ms ease;
	}
	.led {
		position: absolute;
		top: 8px;
		right: 10px;
		height: 6px;
		width: 6px;
		border-radius: 9999px;
		background: var(--tone);
		transition: background 250ms ease;
	}
	.stage[data-scene='ready'] .led,
	.stage[data-scene='wrong_tag'] .led {
		animation: blink 1.2s ease-in-out infinite;
	}
	.stage[data-scene='writing'] .led {
		animation: blink 0.3s steps(2) infinite;
	}
	@keyframes blink {
		50% {
			opacity: 0.25;
		}
	}

	/* Contactless waves rising from the reader. */
	.waves {
		position: absolute;
		left: 50%;
		top: 168px;
		height: 0;
		width: 0;
	}
	.wave {
		position: absolute;
		left: -60px;
		top: -60px;
		height: 120px;
		width: 120px;
		border-radius: 9999px;
		border: 2px solid var(--tone);
		clip-path: inset(0 0 50% 0);
		opacity: 0;
	}
	.stage[data-waves='slow'] .wave {
		animation: wave 2.4s cubic-bezier(0.2, 0.6, 0.3, 1) infinite;
		animation-delay: calc(var(--i) * 0.5s);
	}
	.stage[data-waves='fast'] .wave {
		animation: wave 0.75s cubic-bezier(0.2, 0.6, 0.3, 1) infinite;
		animation-delay: calc(var(--i) * 0.16s);
	}
	.stage[data-waves='done'] .wave {
		animation: wave 1.1s cubic-bezier(0.2, 0.6, 0.3, 1) 1 both;
		animation-delay: calc(var(--i) * 0.12s);
	}
	@keyframes wave {
		from {
			opacity: 0.75;
			transform: scale(0.45);
		}
		to {
			opacity: 0;
			transform: scale(2.1);
		}
	}

	/* The tag. Its pose is a state, so it re-targets from wherever it is when the state changes. */
	.card {
		position: absolute;
		left: 50%;
		top: 84px;
		width: 128px;
		height: 80px;
		margin-left: -64px;
		transition:
			transform 900ms var(--spring),
			opacity 400ms ease;
		will-change: transform;
	}
	.card[data-pose='away'] {
		transform: translateY(-96px) rotate(-10deg);
		opacity: 0.35;
	}
	.card[data-pose='far'] {
		transform: translateY(-62px) rotate(-7deg);
		opacity: 0.9;
	}
	.card[data-pose='near'] {
		transform: translateY(-18px) rotate(-2deg);
		opacity: 1;
	}
	.card[data-pose='docked'] {
		transform: translateY(-2px) rotate(0deg);
		opacity: 1;
	}
	.stage[data-scene='wrong_tag'] .card-face {
		animation: shake 450ms ease-in-out 1;
	}
	@keyframes shake {
		20% {
			translate: -8px 0;
		}
		40% {
			translate: 7px 0;
		}
		60% {
			translate: -5px 0;
		}
		80% {
			translate: 3px 0;
		}
	}
	.card-face {
		position: relative;
		display: flex;
		height: 100%;
		flex-direction: column;
		overflow: hidden;
		border-radius: 12px;
		border: 1px solid var(--tone);
		padding: 10px 12px;
		background: var(--color-paper, #fff);
		color: var(--tone);
		box-shadow: 0 14px 28px -14px rgb(0 0 0 / 0.45);
		transition:
			border-color 250ms ease,
			color 250ms ease;
	}
	.chip {
		position: absolute;
		right: 12px;
		top: 12px;
		height: 18px;
		width: 24px;
		border-radius: 4px;
		border: 1px solid currentColor;
		opacity: 0.6;
	}
	.sheen {
		position: absolute;
		inset: 0;
		background: linear-gradient(
			100deg,
			transparent 30%,
			rgb(234 179 8 / 0.35) 50%,
			transparent 70%
		);
		transform: translateX(-100%);
	}
	.stage[data-scene='writing'] .sheen {
		animation: sweep 0.9s linear infinite;
	}
	@keyframes sweep {
		to {
			transform: translateX(100%);
		}
	}
	.badge {
		position: absolute;
		left: 50%;
		top: 50%;
		display: flex;
		height: 36px;
		width: 36px;
		margin: -18px 0 0 -18px;
		align-items: center;
		justify-content: center;
		border-radius: 9999px;
		background: #22c55e;
		color: white;
		font-weight: 600;
		scale: 0;
		transition: scale 450ms cubic-bezier(0.34, 1.56, 0.64, 1);
	}
	.badge[data-show='true'] {
		scale: 1;
	}

	/* Reduced motion: states still change, but as a cross-fade instead of travel and pulses. */
	@media (prefers-reduced-motion: reduce) {
		.dot.active::after,
		.wave,
		.led,
		.sheen,
		.card-face {
			animation: none !important;
		}
		.stage:not([data-waves='off']) .wave {
			opacity: 0.35;
			transform: scale(1.2);
		}
		.card {
			transition: opacity 250ms ease;
		}
		.card[data-pose] {
			transform: translateY(-14px);
		}
		.badge {
			transition: none;
		}
	}
</style>
