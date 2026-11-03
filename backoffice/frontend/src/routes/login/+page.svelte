<script lang="ts">
	import { onMount } from 'svelte';
	import {
		passkeysSupported,
		sendPasswordReset,
		signIn,
		signInWithPasskey,
		signUp
	} from '#lib/auth.ts';
	import AuthShell from '#lib/auth/AuthShell.svelte';
	import QuickUnlock from '#lib/auth/QuickUnlock.svelte';

	type Mode = 'signin' | 'register' | 'reset';
	const copy: Record<Mode, { title: string; subtitle: string; action: string }> = {
		signin: {
			title: 'Welcome back',
			subtitle: 'Sign in to manage keys and doors.',
			action: 'Sign in'
		},
		register: {
			title: 'Create an account',
			subtitle: 'Admins issue your keys once your account exists.',
			action: 'Create account'
		},
		reset: {
			title: 'Reset your password',
			subtitle: 'We will email you a link to choose a new one.',
			action: 'Send reset link'
		}
	};

	let mode = $state<Mode>('signin');
	let email = $state('');
	let password = $state('');
	let confirm = $state('');
	let reveal = $state(false);
	let message = $state('');
	let failed = $state(false);
	let busy = $state(false);
	let canUsePasskey = $state(false);

	onMount(() => (canUsePasskey = passkeysSupported()));

	const switchTo = (next: Mode) => {
		mode = next;
		message = '';
		password = '';
		confirm = '';
	};

	const report = (text: string, error = true) => {
		message = text;
		failed = error;
	};

	const enter = () => (window.location.href = '/');

	const passkey = async () => {
		busy = true;
		message = '';
		try {
			await signInWithPasskey();
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
		if (mode === 'register' && password !== confirm) return report('The passwords do not match.');
		busy = true;
		try {
			if (mode === 'signin') {
				await signIn(email, password);
				enter();
			} else if (mode === 'register') {
				if (await signUp(email, password)) enter();
				else report('Check your inbox to confirm your email, then sign in.', false);
			} else {
				await sendPasswordReset(email);
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
		<QuickUnlock />
		<div class="flex items-center gap-3 text-xs text-gray-400">
			<span class="h-px flex-1 bg-gray-200"></span>or sign in<span class="h-px flex-1 bg-gray-200"
			></span>
		</div>
	{/if}

	{#if mode !== 'reset'}
		<div role="tablist" class="grid grid-cols-2 gap-1 rounded-lg border bg-paper p-1">
			{#each [['signin', 'Sign in'], ['register', 'Register']] as const as [value, label] (value)}
				<button
					type="button"
					role="tab"
					aria-selected={mode === value}
					onclick={() => switchTo(value)}
					class="rounded-md py-1.5 text-sm font-medium transition-colors {mode === value
						? 'bg-paper-2 text-ink shadow-[inset_0_0_0_1px_var(--color-gray-200)]'
						: 'text-gray-400 hover:text-ink'}">{label}</button
				>
			{/each}
		</div>
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
			<div class="flex flex-col gap-1.5">
				<span class="flex items-baseline justify-between">
					<label for="password" class="eyebrow">Password</label>
					{#if mode === 'signin'}
						<button
							type="button"
							onclick={() => switchTo('reset')}
							class="text-xs text-gray-400 hover:text-ink">Forgot password?</button
						>
					{/if}
				</span>
				<span class="relative flex">
					<input
						id="password"
						type={reveal ? 'text' : 'password'}
						required
						minlength={mode === 'register' ? 8 : undefined}
						autocomplete={mode === 'register' ? 'new-password' : 'current-password'}
						placeholder={mode === 'register' ? '8+ characters' : '••••••••'}
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

		{#if mode === 'register'}
			<label class="flex flex-col gap-1.5">
				<span class="eyebrow">Confirm password</span>
				<input
					type={reveal ? 'text' : 'password'}
					required
					autocomplete="new-password"
					bind:value={confirm}
					class="px-3.5 py-2.5"
				/>
			</label>
		{/if}

		{#if message}
			<p
				role="status"
				class="rounded-lg border px-3.5 py-2.5 {failed
					? 'border-red-500/30 bg-red-500/5 text-red-400'
					: 'border-green-700/30 bg-green-700/5 text-green-700'}"
			>
				{message}
			</p>
		{/if}

		<button type="submit" disabled={busy} class="btn btn-primary mt-2 py-2.5 text-[0.8125rem]">
			{busy ? 'One moment…' : copy[mode].action}
		</button>
	</form>

	{#if mode === 'signin' && canUsePasskey}
		<div class="flex items-center gap-3 text-xs text-gray-400">
			<span class="h-px flex-1 bg-gray-200"></span>or<span class="h-px flex-1 bg-gray-200"></span>
		</div>
		<button type="button" onclick={passkey} disabled={busy} class="btn py-2.5 text-[0.8125rem]">
			<svg viewBox="0 0 24 24" class="h-4 w-4" fill="none" stroke="currentColor" stroke-width="1.8"
				><circle cx="9" cy="8" r="4" /><path d="M2 21a7 7 0 0 1 11-5.7" /><circle
					cx="18"
					cy="13"
					r="2.5"
				/><path d="M18 15.5V21l1.5-1.5L18 18" /></svg
			>
			Sign in with a passkey
		</button>
	{/if}

	{#if mode === 'reset'}
		<button
			type="button"
			onclick={() => switchTo('signin')}
			class="self-start text-sm text-gray-400 hover:text-ink">← Back to sign in</button
		>
	{/if}
</AuthShell>
