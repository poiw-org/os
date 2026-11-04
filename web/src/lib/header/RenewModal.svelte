<script lang="ts">
	import { untrack } from 'svelte';
	import { DateInput } from 'date-picker-svelte';
	import { renewKey } from '#lib/api.ts';
	import { shortId, withName } from '#lib/format.ts';
	import { nfcSupported } from '#lib/nfc.ts';
	import { toast } from '#lib/notify.svelte.ts';
	import type { Key } from '#lib/types.ts';

	let {
		key,
		onclose,
		onrenewed,
		onwrite
	}: {
		key: Key;
		onclose: () => void;
		/** The key was renewed: reload it so its new token is at hand. */
		onrenewed: () => Promise<void>;
		/** Start writing the renewed token to the tag, at the door or with this device's reader. */
		onwrite: (via: 'door' | 'nfc') => void;
	} = $props();

	let dialog = $state<HTMLDialogElement>();
	let notBefore = $state(untrack(() => key.notBefore));
	let expiresAt = $state(untrack(() => key.expiresAt));
	let busy = $state(false);
	let renewed = $state(false);

	$effect(() => dialog?.showModal());

	const submit = async (event: SubmitEvent) => {
		event.preventDefault();
		if (expiresAt <= notBefore) {
			toast.error('The key must expire after it activates.');
			return;
		}
		busy = true;
		try {
			await renewKey({ id: key.id, notBefore, expiresAt });
			await onrenewed();
			toast.success('Key renewed.');
			renewed = true;
		} catch (e) {
			toast.error(e instanceof Error ? e.message : 'Could not renew the key.');
		}
		busy = false;
	};

	const write = (via: 'door' | 'nfc') => {
		dialog?.close();
		onwrite(via);
	};
</script>

<dialog
	bind:this={dialog}
	{onclose}
	onclick={(e) => e.target === dialog && dialog?.close()}
	aria-labelledby="renew-title"
	class="sheet m-auto w-[calc(100%-2rem)] max-w-md overflow-visible rounded-lg border bg-paper p-0 text-ink backdrop:bg-black/70"
>
	<div class="relative flex flex-col gap-5 p-6">
		<button
			type="button"
			onclick={() => dialog?.close()}
			aria-label="Close"
			class="absolute top-4 right-4 flex h-8 w-8 items-center justify-center rounded-md text-gray-400 hover:bg-paper-2 hover:text-ink"
			>✕</button
		>
		<header class="pr-8">
			<h2 id="renew-title" class="text-base font-bold">Renew {shortId(key.id)}</h2>
			<p class="mt-1 text-sm text-gray-400">{withName(key.user)}</p>
		</header>

		{#if renewed}
			{#if key.serialNumber}
				<p role="status" class="rounded-md border border-yellow-500/50 bg-yellow-500/10 p-3 text-sm">
					<strong>Reflash the tag.</strong> The tag still carries the old token, with the old dates.
					The door only honours the new dates once the renewed key is written to the tag again.
				</p>
				<div class="flex flex-wrap justify-end gap-2">
					<button type="button" onclick={() => dialog?.close()} class="btn">Later</button>
					{#if nfcSupported}
						<button type="button" onclick={() => write('nfc')} class="btn">Write to tag</button>
					{/if}
					<button type="button" onclick={() => write('door')} class="btn btn-primary"
						>Write at door</button
					>
				</div>
			{:else}
				<p role="status" class="text-sm text-gray-600">
					Renewed. This is a virtual key, so there is no tag to reflash.
				</p>
				<div class="flex justify-end">
					<button type="button" onclick={() => dialog?.close()} class="btn btn-primary"
						>Done</button
					>
				</div>
			{/if}
		{:else}
			<form onsubmit={submit} class="flex flex-col gap-5">
				<div class="flex flex-wrap gap-4">
					<div class="flex flex-col gap-2">
						<span class="eyebrow">Activate not before</span>
						<DateInput
							format="yyyy-MM-dd"
							min={new Date()}
							max={new Date('2100')}
							bind:value={notBefore}
						/>
					</div>
					<div class="flex flex-col gap-2">
						<span class="eyebrow">Expires at</span>
						<DateInput
							format="yyyy-MM-dd"
							min={new Date()}
							max={new Date('2100')}
							bind:value={expiresAt}
						/>
					</div>
				</div>
				{#if key.serialNumber}
					<p class="text-sm text-gray-400">
						The key is bound to a tag. After renewing, write it to the tag again so the tag carries
						the new dates.
					</p>
				{/if}
				<div class="flex justify-end gap-2">
					<button type="button" onclick={() => dialog?.close()} class="btn">Cancel</button>
					<button type="submit" disabled={busy} class="btn btn-primary"
						>{busy ? 'Renewing…' : 'Renew'}</button
					>
				</div>
			</form>
		{/if}
	</div>
</dialog>
