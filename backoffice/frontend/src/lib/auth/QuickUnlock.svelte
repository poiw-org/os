<script lang="ts">
	import Fa from 'svelte-fa';
	import { faDoorClosed, faDoorOpen } from '@fortawesome/free-solid-svg-icons';
	import { unlockDoor, type ForceOpenEvent } from '#lib/api.ts';
	import { signInWithPasskey, signOut } from '#lib/auth.ts';

	type Phase = 'idle' | 'passkey' | 'door' | 'unlocking' | 'done' | 'failed';

	const RING = 2 * Math.PI * 26;
	const COPY: Record<Phase, [string, string]> = {
		idle: ['Quick unlock', 'Open the door with a passkey'],
		passkey: ['Waiting for your passkey', 'Follow the prompt on your device'],
		door: ['Contacting the door', 'This takes a moment'],
		unlocking: ['Unlocking', ''],
		done: ['Door unlocked', 'You can come in'],
		failed: ['Could not unlock', '']
	};

	let phase = $state<Phase>('idle');
	let error = $state('');
	let secondsLeft = $state(0);
	let totalSeconds = $state(0);
	let resetTimer: ReturnType<typeof setTimeout> | undefined;

	const busy = $derived(phase === 'passkey' || phase === 'door' || phase === 'unlocking');
	// The ring fills as the door's warning tone counts down; other phases show a sweeping arc.
	const progress = $derived(
		phase === 'done'
			? 1
			: phase === 'unlocking' && totalSeconds
				? 1 - secondsLeft / totalSeconds
				: 0
	);

	const follow = (event: ForceOpenEvent) => {
		if (event.step === 'send' && event.state === 'active') phase = 'door';
		if (event.step === 'unlock' && event.state === 'active') {
			phase = 'unlocking';
			const left = Number(event.text.match(/(\d+) s/)?.[1]);
			if (left) {
				totalSeconds ||= left;
				secondsLeft = left;
			}
		}
	};

	const settle = (next: Phase, after: number) => {
		phase = next;
		clearTimeout(resetTimer);
		resetTimer = setTimeout(() => (phase = 'idle'), after);
	};

	// Asks for a passkey, then unlocks. The session is dropped right after: this is a shortcut at
	// the door, not a sign-in.
	const run = async () => {
		if (busy) return;
		clearTimeout(resetTimer);
		phase = 'passkey';
		error = '';
		totalSeconds = secondsLeft = 0;
		try {
			await signInWithPasskey();
			await unlockDoor(follow);
			settle('done', 3500);
		} catch (e) {
			// Closing the browser's passkey prompt is not worth an error.
			if (e instanceof Error && e.name === 'NotAllowedError') phase = 'idle';
			else {
				error = e instanceof Error ? e.message : 'Something went wrong.';
				settle('failed', 5000);
			}
		}
		await signOut();
	};

	const title = $derived(COPY[phase][0]);
	const detail = $derived(
		phase === 'failed'
			? error
			: phase === 'unlocking'
				? secondsLeft
					? `Unlocking in ${secondsLeft} s`
					: 'Unlocking now'
				: COPY[phase][1]
	);
</script>

<button
	type="button"
	onclick={run}
	disabled={busy}
	aria-busy={busy}
	class="qu group relative flex w-full items-center gap-4 overflow-hidden rounded-2xl border bg-paper-2 p-4 text-left shadow-[0_1px_0_0_rgb(255_255_255/0.04)_inset] transition-[transform,border-color,background-color] duration-200 ease-out active:scale-[0.985] enabled:hover:border-gray-300 {phase ===
	'done'
		? 'border-green-500/50'
		: phase === 'failed'
			? 'qu-shake border-red-500/50'
			: ''}"
>
	<!-- Soft glow that follows the state -->
	<span
		aria-hidden="true"
		class="pointer-events-none absolute -top-10 -left-10 h-32 w-32 rounded-full blur-3xl transition-opacity duration-500 {phase ===
		'failed'
			? 'bg-red-500/20 opacity-100'
			: phase === 'idle'
				? 'bg-green-500/10 opacity-60 group-hover:opacity-100'
				: 'bg-green-500/20 opacity-100'}"
	></span>

	<span class="relative flex h-14 w-14 shrink-0 items-center justify-center">
		<svg viewBox="0 0 60 60" class="absolute inset-0 -rotate-90" aria-hidden="true">
			<circle cx="30" cy="30" r="26" fill="none" stroke="var(--color-gray-200)" stroke-width="3" />
			<circle
				cx="30"
				cy="30"
				r="26"
				fill="none"
				stroke-width="3"
				stroke-linecap="round"
				class="qu-ring {phase === 'passkey' || phase === 'door' ? 'qu-sweep' : ''} {phase ===
				'failed'
					? 'stroke-red-500'
					: 'stroke-green-500'}"
				stroke-dasharray={phase === 'passkey' || phase === 'door' ? `${RING * 0.28} ${RING}` : RING}
				stroke-dashoffset={RING * (1 - progress)}
				opacity={phase === 'idle' || phase === 'failed' ? (phase === 'failed' ? 1 : 0) : 1}
			/>
		</svg>
		<span
			class="relative text-lg transition-colors duration-300 {phase === 'done'
				? 'text-green-500'
				: phase === 'failed'
					? 'text-red-500'
					: 'text-ink'}"
		>
			{#if phase === 'done' || phase === 'unlocking'}
				<span class="qu-pop block"><Fa icon={faDoorOpen} /></span>
			{:else}
				<span class="block"><Fa icon={faDoorClosed} /></span>
			{/if}
		</span>
	</span>

	<span class="relative flex min-w-0 flex-1 flex-col gap-0.5" aria-live="polite">
		<span class="font-medium tracking-tight">{title}</span>
		<span class="text-sm text-gray-400">{detail}</span>
	</span>

	{#if phase === 'idle'}
		<span
			aria-hidden="true"
			class="relative text-gray-400 transition-transform duration-200 ease-out group-hover:translate-x-0.5 group-hover:text-ink"
			>→</span
		>
	{/if}
</button>

<style>
	.qu-ring {
		transition:
			stroke-dashoffset 1s linear,
			opacity 200ms ease;
		transform-origin: center;
	}
	.qu-sweep {
		animation: qu-spin 1.1s linear infinite;
	}
	.qu-pop {
		animation: qu-pop 380ms cubic-bezier(0.34, 1.56, 0.64, 1);
	}
	.qu-shake {
		animation: qu-shake 420ms cubic-bezier(0.36, 0.07, 0.19, 0.97);
	}
	@keyframes qu-spin {
		to {
			transform: rotate(360deg);
		}
	}
	@keyframes qu-pop {
		from {
			transform: scale(0.6);
			opacity: 0;
		}
	}
	@keyframes qu-shake {
		20%,
		60% {
			transform: translateX(-5px);
		}
		40%,
		80% {
			transform: translateX(5px);
		}
	}
	@media (prefers-reduced-motion: reduce) {
		.qu-sweep,
		.qu-pop,
		.qu-shake {
			animation: none;
		}
		.qu-ring {
			transition: none;
		}
	}
</style>
