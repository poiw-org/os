<script lang="ts">
	import { resolve } from '$app/paths';
	import { onMount } from 'svelte';
	import { hasSession, setPassword } from '#lib/auth.ts';
	import AuthShell from '#lib/auth/AuthShell.svelte';
	import Loading from '#lib/loading.svelte';
	import { PASSWORD_HINT, passwordIsStrong } from '#lib/password.ts';

	// The email link signs the user in; the form then sets the new password for that session.
	let ready = $state(false);
	let signedIn = $state(false);
	let password = $state('');
	let confirm = $state('');
	let message = $state('');
	let busy = $state(false);

	onMount(async () => {
		signedIn = await hasSession();
		ready = true;
	});

	const submit = async (event: SubmitEvent) => {
		event.preventDefault();
		if (password !== confirm) {
			message = 'The passwords do not match.';
			return;
		}
		if (!passwordIsStrong(password)) {
			message = PASSWORD_HINT;
			return;
		}
		busy = true;
		message = '';
		try {
			await setPassword(password);
			window.location.href = '/os';
		} catch (e) {
			message = e instanceof Error ? e.message : 'Could not set the password.';
			busy = false;
		}
	};
</script>

<svelte:head>
	<title>Reset password · doora</title>
</svelte:head>

{#if !ready}
	<span class="flex min-h-screen items-center justify-center"><Loading /></span>
{:else if !signedIn}
	<AuthShell title="Link expired" subtitle="This reset link is invalid or has already been used.">
		<a href={resolve('/login')} class="btn btn-primary py-2.5">Request a new link</a>
	</AuthShell>
{:else}
	<AuthShell title="New password">
		<form onsubmit={submit} class="flex flex-col gap-4">
			<label class="flex flex-col gap-1.5">
				<span class="eyebrow">New password</span>
				<input
					type="password"
					required
					minlength="12"
					autocomplete="new-password"
					placeholder="12+ characters"
					bind:value={password}
					class="px-3.5 py-2.5"
				/>
			</label>
			<label class="flex flex-col gap-1.5">
				<span class="eyebrow">Confirm password</span>
				<input
					type="password"
					required
					autocomplete="new-password"
					bind:value={confirm}
					class="px-3.5 py-2.5"
				/>
			</label>
			{#if message}
				<p role="status" class="text-red-400">{message}</p>
			{/if}
			<button type="submit" disabled={busy} class="btn btn-primary mt-1 py-2.5"
				>{busy ? 'Saving…' : 'Save password'}</button
			>
		</form>
	</AuthShell>
{/if}
