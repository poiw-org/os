<script lang="ts">
	import { onMount } from 'svelte';
	import { confirmEmailCode, sendEmailCode, signOut, verifyTotp } from '#lib/auth.ts';
	import AuthShell from '#lib/auth/AuthShell.svelte';
	import Turnstile from '#lib/auth/Turnstile.svelte';

	let { data } = $props();

	const email = $derived(data.user?.needsEmailCode ? data.user.email : '');

	let code = $state('');
	let message = $state('');
	let busy = $state(false);
	let captcha = $state<Turnstile>();

	const send = async () => {
		busy = true;
		message = '';
		try {
			await sendEmailCode(email, await captcha!.take());
		} catch (e) {
			message = e instanceof Error ? e.message : 'Could not send the code.';
		}
		busy = false;
	};

	onMount(() => {
		if (email) send();
	});

	const submit = async (event: SubmitEvent) => {
		event.preventDefault();
		busy = true;
		message = '';
		try {
			await (email ? confirmEmailCode(code.trim()) : verifyTotp(code.trim()));
			window.location.href = '/os';
		} catch (e) {
			message = e instanceof Error ? e.message : 'Could not verify the code.';
			code = '';
			busy = false;
		}
	};

	const leave = async () => {
		await signOut();
		window.location.href = '/login';
	};
</script>

<svelte:head>
	<title>Verify · doora</title>
</svelte:head>

<AuthShell
	title="Enter your code"
	subtitle={email ? `We emailed it to ${email}.` : 'From your authenticator app.'}
>
	<form onsubmit={submit} class="flex flex-col gap-4">
		<input
			required
			inputmode="numeric"
			autocomplete="one-time-code"
			minlength="6"
			maxlength="10"
			aria-label="Verification code"
			placeholder="123456"
			bind:value={code}
			class="px-3.5 py-2.5 font-mono"
		/>
		{#if message}<p role="alert" class="text-red-400">{message}</p>{/if}
		{#if email}<Turnstile bind:this={captcha} />{/if}
		<button type="submit" disabled={busy} class="btn btn-primary py-2.5">
			{busy ? 'One moment…' : 'Verify'}
		</button>
	</form>
	{#if email}
		<button type="button" onclick={send} disabled={busy} class="btn py-2.5">Send a new code</button>
	{/if}
	<button type="button" onclick={leave} class="text-brand-text underline">Sign out</button>
</AuthShell>
