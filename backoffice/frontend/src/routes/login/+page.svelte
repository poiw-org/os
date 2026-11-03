<script lang="ts">
	import { sendPasswordReset, signIn, signOut, getCurrentUser } from '#lib/auth.ts';

	let email = $state('');
	let password = $state('');
	let message = $state('');
	let failed = $state(false);
	let busy = $state(false);

	const submit = async (event: SubmitEvent) => {
		event.preventDefault();
		busy = true;
		message = '';
		try {
			await signIn(email, password);
			if (await getCurrentUser()) {
				window.location.href = '/';
				return;
			}
			// A valid Supabase account that is not in the admins table.
			await signOut();
			failed = true;
			message = 'This account does not have access to the backoffice.';
		} catch (e) {
			failed = true;
			message = e instanceof Error ? e.message : 'Could not sign in.';
		}
		busy = false;
	};

	const forgotPassword = async () => {
		if (!email) {
			failed = true;
			message = 'Enter your email first.';
			return;
		}
		busy = true;
		try {
			await sendPasswordReset(email);
			failed = false;
			message = 'If an account exists for this email, a password reset link is on its way.';
		} catch (e) {
			failed = true;
			message = e instanceof Error ? e.message : 'Could not send the reset link.';
		}
		busy = false;
	};
</script>

<svelte:head>
	<title>Sign in</title>
</svelte:head>

<form onsubmit={submit} class="flex flex-col items-start gap-6 px-4 py-10 md:px-10">
	<p class="text-xl font-semibold tracking-tight">SIGN IN</p>
	<input
		type="email"
		required
		autocomplete="username"
		placeholder="Email"
		bind:value={email}
		class="w-full max-w-md border p-4"
	/>
	<input
		type="password"
		autocomplete="current-password"
		placeholder="Password"
		bind:value={password}
		class="w-full max-w-md border p-4"
	/>
	{#if message}
		<p class={failed ? 'text-red-500' : 'text-green-700'} role="status">{message}</p>
	{/if}
	<span class="flex items-center gap-6">
		<button type="submit" disabled={busy || !password} class="btn btn-primary">SIGN IN</button>
		<button type="button" onclick={forgotPassword} disabled={busy} class="text-gray-600 underline"
			>Forgot password?</button
		>
	</span>
</form>
