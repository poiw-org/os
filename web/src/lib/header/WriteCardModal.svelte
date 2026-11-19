<script lang="ts">
	import {
		DOOR_ID,
		WRITE_CARD_STEPS,
		writeCardViaDoor,
		type WriteCardEvent,
		type WriteCardState,
		type WriteCardStep
	} from '#lib/api.ts';
	import { t } from '#lib/i18n.svelte.ts';
	import TagStage, { type TagScene } from './TagStage.svelte';

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

	let dialog = $state<HTMLDialogElement>();
	let phase = $state<Phase>('confirm');
	let scene = $state<TagScene>('idle');
	let events = $state<WriteCardEvent[]>([]);
	let error = $state('');
	let remaining = $state(0);
	let scanned = $state('');
	let controller: AbortController | undefined;

	$effect(() => dialog?.showModal());

	const stepState = $derived(
		Object.fromEntries(events.map((e) => [e.step, e.state])) as Partial<
			Record<WriteCardStep, WriteCardEvent['state']>
		>
	);
	const tail = $derived(serialNumber.slice(-5).toUpperCase());

	const copyFor = (): Record<TagScene, { title: string; hint: string }> => ({
		idle: scanning
			? {
					title: t('Read a tag with the door'),
					hint: t(
						'The reader at the door reads the serial number of the next tag you hold to it. Stand at the door with the tag in your hand, then start.'
					)
				}
			: {
					title: t('Write this key with the door'),
					hint: t(
						'The reader at the door writes the key to the tag with S/N {serial}. Stand at the door with that tag in your hand, then start.',
						{ serial: serialNumber }
					)
				},
		connecting: {
			title: t('Putting the door in writer mode'),
			hint: t('Reaching the door over its private channel.')
		},
		ready: {
			title: t('Hold the tag to the reader'),
			hint: scanning
				? t('Lay it flat on the reader. The door beeps while it waits.')
				: t('Lay it flat on the reader and keep it there. The door beeps while it waits.')
		},
		wrong_tag: {
			title: t('That is a different tag'),
			hint: t('This key belongs to the tag ending {tail}. Swap it for that one.', { tail })
		},
		writing: {
			title: t('Writing. Keep it still'),
			hint: t('Do not lift the tag until the door plays the happy chord.')
		},
		done: scanning
			? {
					title: t('Tag read'),
					hint: t('Serial number {serial} is now in the form.', { serial: scanned })
				}
			: {
					title: t('Key written'),
					hint: t('The door read it back and it matches. Tap the tag on the door to try it.')
				},
		failed: {
			title: scanning ? t('The tag was not read') : t('The tag was not written'),
			hint: ''
		}
	});

	// While the door waits: the countdown.
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
			tick = setInterval(() => {
				remaining = Math.max(0, Math.ceil((deadline - performance.now()) / 1000));
			}, 1000);
		}
		if (state !== 'ready' && state !== 'wrong_tag') stopTick();
	};

	const run = async () => {
		phase = 'running';
		scene = 'connecting';
		events = [];
		error = '';
		// Kept locally, so a retry that swaps the controller cannot revive this run's result.
		const attempt = new AbortController();
		controller = attempt;
		try {
			const result = await writeCardViaDoor(token, {
				signal: attempt.signal,
				onEvent: (e) => events.push(e),
				onState
			});
			// Closed meanwhile: the result is stale and must not reach the form.
			if (attempt.signal.aborted) return;
			phase = 'done';
			if (scanning) {
				scanned = result?.serial ?? '';
				onscanned?.(scanned);
			} else onwritten?.();
		} catch (e) {
			if (attempt.signal.aborted) return;
			stopTick();
			error = e instanceof Error ? e.message : t('Could not write the tag.');
			phase = 'failed';
			scene = 'failed';
		}
	};

	// Leaving the modal any way (buttons, backdrop, Escape, unmount) takes the door out of
	// writer mode.
	const abort = () => {
		controller?.abort();
		stopTick();
	};
	$effect(() => abort);
	const close = () => {
		abort();
		dialog?.close();
	};

	const copy = $derived(copyFor()[scene]);
	const waiting = $derived(scene === 'ready' || scene === 'wrong_tag');
</script>

<dialog
	bind:this={dialog}
	onclose={() => {
		abort();
		onclose();
	}}
	onclick={(e) => e.target === dialog && close()}
	aria-labelledby="write-card-title"
	class="sheet m-auto w-[calc(100%-2rem)] max-w-xl overflow-hidden rounded-lg border bg-paper p-0 text-ink backdrop:bg-black/70"
>
	<div class="relative p-6">
		<button
			type="button"
			onclick={close}
			aria-label={t('Close')}
			class="absolute top-4 right-4 flex h-8 w-8 items-center justify-center rounded-md text-gray-400 hover:bg-paper-2 hover:text-ink"
			>✕</button
		>
		<h2 id="write-card-title" class="text-base font-bold">
			{scanning ? t('Scan a tag') : t('Write to tag')}
		</h2>
		<p class="mt-1 text-sm text-gray-400">
			{t('Uses the reader on')}
			<span class="font-mono text-gray-500">door:{DOOR_ID}</span>
			{scanning ? t('as a scanner.') : t('as a writer.')}
		</p>

		<!-- Checkpoints -->
		<ol class="mt-6 grid grid-cols-5" aria-label={t('Progress')}>
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
						class="dot relative z-10 flex h-6 w-6 items-center justify-center rounded-full border font-mono text-[11px]
						{state === 'done'
							? 'border-green-500 bg-paper text-green-500'
							: state === 'failed'
								? 'border-red-500 bg-paper text-red-500'
								: state === 'active'
									? 'border-ink bg-paper text-ink'
									: 'bg-paper text-gray-400'}"
					>
						{state === 'done' ? '✓' : state === 'failed' ? '!' : i + 1}
					</span>
					<span class="text-xs {state ? 'text-ink' : 'text-gray-400'}">{t(LABELS[step])}</span>
				</li>
			{/each}
		</ol>

		<TagStage {scene} label={scanning ? '···' : `···${tail}`} />

		<div class="mt-5 min-h-16" aria-live="polite">
			<p class="text-base font-bold">{copy.title}</p>
			<p
				class="mt-1 text-sm {phase === 'failed' ? 'text-red-500' : 'text-gray-400'}"
				role={phase === 'failed' ? 'alert' : undefined}
			>
				{phase === 'failed' ? error : copy.hint}
			</p>
			{#if waiting}
				<p class="mt-2 font-mono text-xs text-gray-400">
					{t('Writer mode ends in {remaining} s', { remaining })}
				</p>
			{/if}
		</div>

		<div class="mt-4 flex justify-end gap-2">
			{#if phase === 'done'}
				<button type="button" class="btn btn-primary" onclick={close}>{t('Done')}</button>
			{:else}
				<button type="button" class="btn" onclick={close}>{t('Cancel')}</button>
				<button type="button" class="btn btn-primary" onclick={run} disabled={phase === 'running'}>
					{phase === 'running'
						? t('Waiting…')
						: phase === 'failed'
							? t('Try again')
							: scanning
								? t('Start scanning')
								: t('Start writer mode')}
				</button>
			{/if}
		</div>
	</div>
</dialog>
