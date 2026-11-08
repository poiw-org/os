<script lang="ts">
	import { resolve } from '$app/paths';
	import { onMount } from 'svelte';
	import {
		passkeysSupported,
		sendPasswordReset,
		signIn,
		signInWithPasskey
	} from '#lib/auth.ts';
	import AuthShell from '#lib/auth/AuthShell.svelte';
	import QuickUnlock from '#lib/auth/QuickUnlock.svelte';
	import Turnstile from '#lib/auth/Turnstile.svelte';
	import { arrive, reveal as unfold } from '#lib/motion.ts';

	type Mode = 'signin' | 'reset';
	const copy: Record<Mode, { title: string; subtitle: string; action: string }> = {
		signin: {
			title: 'Sign in',
			subtitle: '',
			action: 'Sign in'
		},
		reset: {
			title: 'Reset password',
			subtitle: 'We will email you a link to choose a new one.',
			action: 'Send reset link'
		}
	};

	let mode = $state<Mode>('signin');
	let email = $state('');
	let password = $state('');
	let reveal = $state(false);
	let message = $state('');
	let failed = $state(false);
	let busy = $state(false);
	let canUsePasskey = $state(false);
	let captcha = $state<Turnstile>();
	const token = () => captcha!.take();

	onMount(() => (canUsePasskey = passkeysSupported()));

	const switchTo = (next: Mode) => {
		mode = next;
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

	const passkey = async () => {
		busy = true;
		message = '';
		try {
			await signInWithPasskey(await token());
			enter();
		} catch (e) {
			// Closing the browser's passkey prompt is not an error worth showing.
			if (!(e instanceof Error && e.name === 'NotAllowedError'))
				report(e instanceof Error ? e.message : 'Could not sign in with a passkey.');
		}
		busy = false;
	};

	const submit = async (event: SubmitEvent) => {
		event.preventDefault();
		message = '';
		busy = true;
		try {
			if (mode === 'signin') {
				await signIn(email, password, await token());
				enter();
			} else {
				await sendPasswordReset(email, await token());
				report('If an account exists for this email, a reset link is on its way.', false);
			}
		} catch (e) {
			report(e instanceof Error ? e.message : 'Something went wrong.');
		}
		busy = false;
	};
</script>

<svelte:head>
	<title>{copy[mode].title} · doora</title>
</svelte:head>

<AuthShell title={copy[mode].title} subtitle={copy[mode].subtitle}>
	{#if mode === 'signin' && canUsePasskey}
		<QuickUnlock captcha={token} />
	{/if}

	<form onsubmit={submit} class="flex flex-col gap-4">
		<label class="flex flex-col gap-1.5">
			<span class="eyebrow">Email</span>
			<input
				type="email"
				required
				autocomplete="username"
				placeholder="you@example.com"
				bind:value={email}
				class="px-3.5 py-2.5"
			/>
		</label>

		{#if mode !== 'reset'}
			<div class="flex flex-col gap-1.5" transition:unfold>
				<span class="flex items-baseline justify-between">
					<label for="password" class="eyebrow">Password</label>
					{#if mode === 'signin'}
						<button
							type="button"
							onclick={() => switchTo('reset')}
							class="text-xs text-brand-text underline">Forgot password?</button
						>
					{/if}
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

		<button type="submit" disabled={busy} class="btn btn-primary mt-1 py-2.5">
			{busy ? 'One moment…' : copy[mode].action}
		</button>
	</form>

	{#if mode === 'signin' && canUsePasskey}
		<button type="button" onclick={passkey} disabled={busy} class="btn py-2.5"
			>Sign in with a passkey</button
		>
	{/if}

	{#if mode === 'signin'}
		<p class="text-gray-400">
			No account? <a href={resolve('/onboarding')} class="text-brand-text underline">Register</a>
		</p>
	{:else}
		<p class="text-gray-400">
			<button type="button" onclick={() => switchTo('signin')} class="text-brand-text underline"
				>Back to sign in</button
			>
		</p>
	{/if}
</AuthShell>
