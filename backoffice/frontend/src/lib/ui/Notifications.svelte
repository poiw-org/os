<script lang="ts">
	import { flip } from 'svelte/animate';
	import { fly } from 'svelte/transition';
	import Fa from 'svelte-fa';
	import {
		faCheck,
		faExclamationCircle,
		faInfoCircle,
		faTimes
	} from '@fortawesome/free-solid-svg-icons';
	import { confirmation, toast, toasts, type ToastTone } from '#lib/notify.svelte.ts';

	const reduced = () =>
		typeof matchMedia === 'function' && matchMedia('(prefers-reduced-motion: reduce)').matches;

	const icons = { success: faCheck, error: faExclamationCircle, info: faInfoCircle };
	const tones: Record<ToastTone, string> = {
		success: 'text-green-700',
		error: 'text-red-500',
		info: 'text-gray-600'
	};

	let dialog = $state<HTMLDialogElement>();
	const question = $derived(confirmation.current);

	$effect(() => {
		if (question && dialog && !dialog.open) dialog.showModal();
	});

	// Every way out of the dialog (buttons, Escape, backdrop) ends up here.
	const answer = (accepted: boolean) => {
		question?.resolve(accepted);
		confirmation.current = undefined;
		dialog?.close();
	};
</script>

<div
	class="pointer-events-none fixed inset-x-0 bottom-0 z-50 flex flex-col items-center gap-2 p-4 sm:items-end"
	aria-live="polite"
>
	{#each toasts as item (item.id)}
		<div
			role={item.tone === 'error' ? 'alert' : 'status'}
			in:fly={{ y: reduced() ? 0 : 16, duration: reduced() ? 0 : 220 }}
			out:fly={{ y: reduced() ? 0 : 8, duration: reduced() ? 0 : 160 }}
			animate:flip={{ duration: reduced() ? 0 : 200 }}
			class="pointer-events-auto flex w-full max-w-sm items-start gap-3 rounded-xl border bg-paper-2 py-3 pr-2 pl-4 shadow-2xl shadow-black/40"
		>
			<span class="mt-0.5 {tones[item.tone]}"><Fa icon={icons[item.tone]} /></span>
			<p class="min-w-0 flex-1 wrap-break-word">{item.message}</p>
			<button
				type="button"
				onclick={() => toast.dismiss(item.id)}
				aria-label="Dismiss"
				class="rounded-md p-1.5 text-gray-400 transition-colors hover:text-ink"
				><Fa icon={faTimes} /></button
			>
		</div>
	{/each}
</div>

<dialog
	bind:this={dialog}
	onclose={() => question && answer(false)}
	onclick={(e) => e.target === dialog && answer(false)}
	aria-labelledby="confirm-title"
	class="sheet m-auto w-[calc(100%-2rem)] max-w-sm rounded-xl border bg-paper p-0 text-ink shadow-2xl backdrop:bg-black/60 backdrop:backdrop-blur-sm"
>
	{#if question}
		<div class="flex flex-col gap-5 p-6">
			<div class="flex flex-col gap-2">
				<p id="confirm-title" class="text-lg font-semibold tracking-tight">{question.title}</p>
				{#if question.message}<p class="text-gray-600">{question.message}</p>{/if}
			</div>
			<span class="flex justify-end gap-2">
				<button type="button" onclick={() => answer(false)} class="btn"
					>{question.cancelLabel ?? 'Cancel'}</button
				>
				<!-- svelte-ignore a11y_autofocus -->
				<button
					type="button"
					autofocus
					onclick={() => answer(true)}
					class="btn {question.tone === 'danger' ? 'tone-red' : 'btn-primary'}"
					>{question.confirmLabel ?? 'Confirm'}</button
				>
			</span>
		</div>
	{/if}
</dialog>
