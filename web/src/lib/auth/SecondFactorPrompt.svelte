<script lang="ts">
	import { resolve } from '$app/paths';
	import { onMount } from 'svelte';
	import { secondFactorPromptDismissed, dismissSecondFactorPrompt } from '#lib/auth.ts';
	import { dateLocale, t } from '#lib/i18n.svelte.ts';
	import type { CurrentUser } from '#lib/types.ts';

	let { user }: { user: CurrentUser } = $props();

	let dialog = $state<HTMLDialogElement>();

	onMount(() => {
		if (secondFactorPromptDismissed(user)) return;
		// Recorded on showing, so however it is left it waits for the next sign-in.
		dismissSecondFactorPrompt(user);
		dialog?.showModal();
	});

	const day = (date: Date) => date.toLocaleDateString(dateLocale(), { dateStyle: 'long' });
</script>

<dialog
	bind:this={dialog}
	aria-labelledby="second-factor-title"
	class="sheet m-auto w-[calc(100%-2rem)] max-w-md rounded-lg border bg-paper p-6 text-ink backdrop:bg-black/70"
>
	<h2 id="second-factor-title" class="text-base font-bold">{t('Protect your key with 2FA')}</h2>
	<p class="mt-2 text-gray-400">
		{t('Register a passkey or an authenticator app to keep your door access.')}
		{#if user.keysSuspendAt}
			{t('Without one, your keys will be suspended on {date}.', { date: day(user.keysSuspendAt) })}
		{:else}
			{t('Without one, your keys will be suspended after 14 days.')}
		{/if}
	</p>
	<div class="mt-5 flex justify-end gap-2">
		<button type="button" class="btn" onclick={() => dialog?.close()}>{t('Later')}</button>
		<a href={resolve('/security')} class="btn btn-primary">{t('Set up now')}</a>
	</div>
</dialog>
