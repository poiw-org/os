<script lang="ts">
	import { signOut, verifyTotp } from '#lib/auth.ts';
	import AuthShell from '#lib/auth/AuthShell.svelte';

	let code = $state('');
	let message = $state('');
	let busy = $state(false);

	const submit = async (event: SubmitEvent) => {
		event.preventDefault();
		busy = true;
		message = '';
		try {
			await verifyTotp(code.trim());
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

<AuthShell title="Enter your code" subtitle="From your authenticator app.">
	<form onsubmit={submit} class="flex flex-col gap-4">
		<input
			required
			inputmode="numeric"
			autocomplete="one-time-code"
			minlength="6"
			maxlength="6"
			aria-label="Six-digit code"
			placeholder="123456"
			bind:value={code}
			class="px-3.5 py-2.5 font-mono"
		/>
		{#if message}<p role="alert" class="text-red-400">{message}</p>{/if}
		<button type="submit" disabled={busy} class="btn btn-primary py-2.5">
			{busy ? 'One moment…' : 'Verify'}
		</button>
	</form>
	<button type="button" onclick={leave} class="text-brand-text underline">Sign out</button>
</AuthShell>
