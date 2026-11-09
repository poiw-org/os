<script lang="ts">
	import { resolve } from '$app/paths';
	import { onMount } from 'svelte';
	import {
		passkeyAutofillAllowed,
		passkeyAutofillSupported,
		passkeysSupported,
		sendPasswordReset,
		signIn,
		signInWithPasskey
	} from '#lib/auth.ts';
	import AuthShell from '#lib/auth/AuthShell.svelte';
	import QuickUnlock from '#lib/auth/QuickUnlock.svelte';
	import { arrive, reveal as unfold } from '#lib/motion.ts';

	type Step = 'email' | 'password' | 'reset';
	const copy: Record<Step, { title: string; subtitle: string; action: string }> = {
		email: { title: 'Sign in', subtitle: '', action: 'Continue' },
		password: { title: 'Sign in', subtitle: '', action: 'Sign in' },
		reset: {
			title: 'Reset password',
			subtitle: 'We will email you a link to choose a new one.',
			action: 'Send reset link'
		}
	};

	let step = $state<Step>('email');
	let email = $state('');
	let password = $state('');
	let reveal = $state(false);
	let message = $state('');
	let failed = $state(false);
	let busy = $state(false);
	let canUsePasskey = $state(false);
	const emailLocked = $derived(step === 'password');

	// Lets the browser or password manager offer a passkey from the email field. It waits in the
	// background until one is picked, and a modal passkey prompt replaces it.
	const offerAutofill = async () => {
		if (!(await passkeyAutofillSupported())) return;
		try {
			await signInWithPasskey('conditional');
			enter();
		} catch (e) {
			if (e instanceof Error && e.name !== 'AbortError') report(e.message);
		}
	};

	onMount(() => {
		canUsePasskey = passkeysSupported();
		if (new URLSearchParams(window.location.search).has('password'))
			report('Sign in with your password to finish. Email links can’t be used on their own.');
		offerAutofill();
	});

	const switchTo = (next: Step) => {
		step = next;
		message = '';
		password = '';
	};

	const report = (text: string, error = true) => {
		message = text;
		failed = error;
	};

	const enter = () => {
		window.location.href = '/os';
	};

	const run = async (action: () => Promise<void>) => {
		message = '';
		busy = true;
		try {
			await action();
		} catch (e) {
			report(e instanceof Error ? e.message : 'Something went wrong.');
		}
		busy = false;
	};

	const usePasskey = () =>
		run(async () => {
			await signInWithPasskey();
			enter();
		});

	const submit = (event: SubmitEvent) => {
		event.preventDefault();
		return run(async () => {
			if (step === 'email') switchTo('password');
			else if (step === 'password') {
				await signIn(email, password);
				enter();
			} else if (step === 'reset') {
				await sendPasswordReset(email);
				report('If an account exists for this email, a reset link is on its way.', false);
			}
		});
	};
</script>

<svelte:head>
	<title>{copy[step].title} · doora</title>
</svelte:head>

<AuthShell title={copy[step].title} subtitle={copy[step].subtitle}>
	{#if step === 'email' && canUsePasskey}
		<QuickUnlock />
	{/if}

	<form onsubmit={submit} class="flex flex-col gap-4">
		<div class="flex flex-col gap-1.5">
			<span class="flex items-baseline justify-between">
				<label for="email" class="eyebrow">Email</label>
				{#if emailLocked}
					<button
						type="button"
						onclick={() => switchTo('email')}
						disabled={busy}
						class="text-xs text-brand-text underline">Change</button
					>
				{/if}
			</span>
			<input
				id="email"
				type="email"
				required
				readonly={emailLocked}
				autocomplete={passkeyAutofillAllowed() ? 'username webauthn' : 'username'}
				placeholder="you@example.com"
				bind:value={email}
				class="px-3.5 py-2.5"
			/>
		</div>

		{#if step === 'password'}
			<div class="flex flex-col gap-1.5" transition:unfold>
				<span class="flex items-baseline justify-between">
					<label for="password" class="eyebrow">Password</label>
					<button
						type="button"
						onclick={() => switchTo('reset')}
						class="text-xs text-brand-text underline">Forgot password?</button
					>
				</span>
				<span class="relative flex">
					<input
						id="password"
						type={reveal ? 'text' : 'password'}
						required
						autocomplete="current-password"
						placeholder="••••••••"
						bind:value={password}
						class="w-full py-2.5 pr-16 pl-3.5"
					/>
					<button
						type="button"
						onclick={() => (reveal = !reveal)}
						aria-pressed={reveal}
						aria-label={reveal ? 'Hide password' : 'Show password'}
						class="absolute inset-y-0 right-0 px-3.5 text-xs text-gray-400 hover:text-ink"
						>{reveal ? 'Hide' : 'Show'}</button
					>
				</span>
			</div>
		{/if}

		{#if message}
			<p role="status" in:arrive class={failed ? 'text-red-400' : 'text-green-700'}>{message}</p>
		{/if}

		<button type="submit" disabled={busy} class="btn btn-primary mt-1 py-2.5">
			{busy ? 'One moment…' : copy[step].action}
		</button>
	</form>

	{#if step !== 'reset' && canUsePasskey}
		<button type="button" onclick={usePasskey} disabled={busy} class="btn py-2.5"
			>Sign in with a passkey</button
		>
	{/if}

	{#if step === 'reset'}
		<p class="text-gray-400">
			<button type="button" onclick={() => switchTo('email')} class="text-brand-text underline"
				>Back to sign in</button
			>
		</p>
	{:else}
		<p class="text-gray-400">
			No account? <a href={resolve('/onboarding')} class="text-brand-text underline">Register</a>
		</p>
	{/if}
</AuthShell>
