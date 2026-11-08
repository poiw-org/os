<script lang="ts">
	import { resolve } from '$app/paths';
	import { onMount } from 'svelte';
	import {
		hasPasskey,
		passkeysSupported,
		sendPasswordReset,
		sendSignInLink,
		signIn,
		signInWithPasskey
	} from '#lib/auth.ts';
	import AuthShell from '#lib/auth/AuthShell.svelte';
	import QuickUnlock from '#lib/auth/QuickUnlock.svelte';
	import Turnstile from '#lib/auth/Turnstile.svelte';
	import { arrive, reveal as unfold } from '#lib/motion.ts';

	type Step = 'email' | 'password' | 'passkey' | 'sent' | 'reset';
	const copy: Record<Step, { title: string; subtitle: string; action: string }> = {
		email: { title: 'Sign in', subtitle: '', action: 'Continue' },
		password: { title: 'Sign in', subtitle: '', action: 'Sign in' },
		passkey: {
			title: 'Use your passkey',
			subtitle: 'Your device will ask you to confirm it is you.',
			action: 'Continue with passkey'
		},
		sent: { title: 'Check your email', subtitle: '', action: '' },
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
	let captcha = $state<Turnstile>();
	const token = () => captcha!.take();
	const emailLocked = $derived(step === 'password' || step === 'passkey' || step === 'sent');

	onMount(() => {
		canUsePasskey = passkeysSupported();
		// An expired or already used email link comes back here with the reason.
		const linkError = new URLSearchParams(window.location.hash.slice(1)).get('error_description');
		if (linkError) report(`${linkError}. Sign in to get a new one.`);
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

	const emailLink = async () => {
		await sendSignInLink(email, await token());
		switchTo('sent');
		report(
			`We sent a sign-in link to ${email}. Unlocking the door will still need your passkey or authenticator.`,
			false
		);
	};

	const lookUp = async () => {
		if (!(await hasPasskey(email, await token()))) switchTo('password');
		else if (canUsePasskey) switchTo('passkey');
		else await emailLink();
	};

	// A refused or unavailable passkey reaches us as NotAllowedError: fall back to the email link.
	const usePasskey = async () => {
		try {
			await signInWithPasskey(await token());
			enter();
		} catch (e) {
			if (e instanceof Error && e.name === 'NotAllowedError') await emailLink();
			else throw e;
		}
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

	const submit = (event: SubmitEvent) => {
		event.preventDefault();
		return run(async () => {
			if (step === 'email') await lookUp();
			else if (step === 'passkey') await usePasskey();
			else if (step === 'password') {
				await signIn(email, password, await token());
				enter();
			} else if (step === 'reset') {
				await sendPasswordReset(email, await token());
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
		<QuickUnlock captcha={token} />
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
				autocomplete="username"
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

		<Turnstile bind:this={captcha} />

		{#if copy[step].action}
			<button type="submit" disabled={busy} class="btn btn-primary mt-1 py-2.5">
				{busy ? 'One moment…' : copy[step].action}
			</button>
		{/if}
	</form>

	{#if step === 'passkey'}
		<button type="button" onclick={() => run(emailLink)} disabled={busy} class="btn py-2.5"
			>Email me a sign-in link instead</button
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
