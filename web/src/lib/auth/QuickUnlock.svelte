<script lang="ts">
	import { unlockDoor, type ForceOpenEvent } from '#lib/api.ts';
	import { signInWithPasskey, signOut } from '#lib/auth.ts';

	type Phase = 'idle' | 'passkey' | 'door' | 'unlocking' | 'done' | 'failed';

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
	// The bar fills as the door's warning tone counts down.
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
			// A closed prompt and a failed passkey are both NotAllowedError, so show the browser's reason.
			error = e instanceof Error ? e.message : 'Something went wrong.';
			settle('failed', 5000);
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
	class="relative flex w-full items-center gap-3 overflow-hidden rounded-lg border bg-paper-2 px-4 py-3 text-left enabled:hover:border-gray-300 {phase ===
	'failed'
		? 'border-red-500/60'
		: phase === 'done'
			? 'border-green-500/60'
			: ''}"
>
	<svg
		viewBox="0 0 24 24"
		class="h-5 w-5 shrink-0 {phase === 'failed'
			? 'text-red-500'
			: phase === 'done'
				? 'text-green-500'
				: 'text-brand-text'}"
		fill="none"
		stroke="currentColor"
		stroke-width="1.8"
		stroke-linecap="round"
		stroke-linejoin="round"
		aria-hidden="true"
	>
		<circle cx="8" cy="15" r="4" />
		<path d="M11 12 21 2m-4 4 3 3m-6 0 2 2" />
	</svg>
	<span class="flex min-w-0 flex-col gap-0.5" aria-live="polite">
		<span class="font-mono font-bold">{title}</span>
		<span class="text-gray-400">{detail}</span>
	</span>
	<span
		class="absolute inset-x-0 bottom-0 h-0.5 origin-left bg-green-500 transition-transform duration-1000 ease-linear motion-reduce:transition-none"
		style:transform="scaleX({progress})"
		aria-hidden="true"
	></span>
</button>
