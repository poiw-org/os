<script lang="ts">
	import { resolve } from '$app/paths';
	import { onMount } from 'svelte';
	import { hasSession, setPassword } from '#lib/auth.ts';
	import Loading from '#lib/loading.svelte';

	// The email link signs the user in; the form then sets the new password for that session.
	let ready = $state(false);
	let signedIn = $state(false);
	let password = $state('');
	let message = $state('');
	let busy = $state(false);

	onMount(async () => {
		signedIn = await hasSession();
		ready = true;
	});

	const submit = async (event: SubmitEvent) => {
		event.preventDefault();
		busy = true;
		message = '';
		try {
			await setPassword(password);
			window.location.href = '/';
		} catch (e) {
			message = e instanceof Error ? e.message : 'Could not set the password.';
			busy = false;
		}
	};
</script>

<svelte:head>
	<title>Reset password</title>
</svelte:head>

{#if !ready}
	<span class="flex pb-20"><Loading /></span>
{:else if !signedIn}
	<span class="flex flex-col items-start gap-6 px-4 py-10 md:px-10">
		<span>This reset link is invalid or has expired.</span>
		<a href={resolve('/login')} class="text-gray-600 underline">Request a new one</a>
	</span>
{:else}
	<form onsubmit={submit} class="flex flex-col items-start gap-6 px-4 py-10 md:px-10">
		<p class="text-xl font-semibold tracking-tight">SET A NEW PASSWORD</p>
		<input
			type="password"
			required
			minlength="8"
			autocomplete="new-password"
			placeholder="New password (8+ characters)"
			bind:value={password}
			class="w-full max-w-md border p-4"
		/>
		{#if message}
			<p class="text-red-500" role="status">{message}</p>
		{/if}
		<button type="submit" disabled={busy} class="btn btn-primary">SAVE PASSWORD</button>
	</form>
{/if}
