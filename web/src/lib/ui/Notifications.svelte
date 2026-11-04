<script lang="ts">
	import { flip } from 'svelte/animate';
	import { fly } from 'svelte/transition';
	import { confirmation, toast, toasts, type ToastTone } from '#lib/notify.svelte.ts';

	const reduced = () =>
		typeof matchMedia === 'function' && matchMedia('(prefers-reduced-motion: reduce)').matches;

	const dots: Record<ToastTone, string> = {
		success: 'lamp-green',
		error: 'lamp-red',
		info: 'lamp-blue'
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
			class="pointer-events-auto flex w-full max-w-sm items-start gap-3 rounded-lg border bg-paper-2 py-3 pr-3 pl-4"
		>
			<span class="lamp mt-[0.4rem] shrink-0 {dots[item.tone]}"></span>
			<p class="min-w-0 flex-1 wrap-break-word">{item.message}</p>
			<button
				type="button"
				onclick={() => toast.dismiss(item.id)}
				class="text-xs text-gray-400 hover:text-ink">Dismiss</button
			>
		</div>
	{/each}
</div>

<dialog
	bind:this={dialog}
	onclose={() => question && answer(false)}
	onclick={(e) => e.target === dialog && answer(false)}
	aria-labelledby="confirm-title"
	class="sheet m-auto w-[calc(100%-2rem)] max-w-sm rounded-lg border bg-paper p-0 text-ink backdrop:bg-black/70"
>
	{#if question}
		<div class="flex flex-col gap-5 p-6">
			<div class="flex flex-col gap-2">
				<p id="confirm-title" class="text-base font-bold">{question.title}</p>
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
