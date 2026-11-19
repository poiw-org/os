<script lang="ts">
	import { resolve } from '$app/paths';
	import { onMount } from 'svelte';
	import {
		getCurrentUser,
		labAccessRefused,
		passkeysSupported,
		pendingReturn,
		sendPasswordReset,
		signIn,
		signInWithPasskey,
		signOut,
		takeReturn
	} from '#lib/auth.ts';
	import AuthShell from '#lib/auth/AuthShell.svelte';
	import LabRefusal from '#lib/auth/LabRefusal.svelte';
	import QuickUnlock from '#lib/auth/QuickUnlock.svelte';
	import { arrive, reveal as unfold } from '#lib/motion.ts';
	import { supabase } from '#lib/supabase.ts';

	type Mode = 'signin' | 'reset';
	const copy: Record<Mode, { title: string; subtitle: string; action: string }> = {
		signin: { title: 'Sign in', subtitle: '', action: 'Sign in' },
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
	// The app they came for is for lab access only and they have none: the name of that app.
	let refusedApp = $state('');
	// Another app (Cloudflare Access, say) sent them here to sign in with their po/iw account, not
	// to open the OS: say so, and leave out the door's quick unlock.
	const forApp = !!pendingReturn();
	const title = $derived(
		refusedApp
			? 'Welcome to po/iw'
			: mode === 'signin' && forApp
				? 'Sign in to continue'
				: copy[mode].title
	);
	const subtitle = $derived(
		refusedApp
			? ''
			: mode === 'signin' && forApp
				? 'An app wants you to sign in with your po/iw account. Next you choose whether to allow it.'
				: copy[mode].subtitle
	);

	onMount(() => {
		canUsePasskey = passkeysSupported();
		if (new URLSearchParams(window.location.search).has('password'))
			report('Sign in with your password to finish. Email links can’t be used on their own.');
	});

	const switchTo = (next: Mode) => {
		mode = next;
		message = '';
		password = '';
	};

	const report = (text: string, error = true) => {
		message = text;
		failed = error;
	};

	// An app that is for lab-access only turns away someone without it here, not after the code,
	// onboarding and consent steps. Asking Supabase about the request hands the user straight on to
	// the app if they allowed it before, so only ask once nothing else stands in the way (the layout
	// guards the consent page with the same steps). Otherwise the consent screen turns them away.
	const refuseForApp = async () => {
		const id = new URLSearchParams((pendingReturn() ?? '').split('?')[1]).get('authorization_id');
		const user = id ? await getCurrentUser() : null;
		if (!id || !user?.onboarded) return false;
		if (user.needsCode || user.needsEmailCode || user.secondFactorEnforced) return false;
		const { data } = await supabase.auth.oauth.getAuthorizationDetails(id);
		if (!data) return false;
		// Allowed before: Supabase has already approved it, so carry on to the app.
		if (!('authorization_id' in data)) {
			takeReturn();
			window.location.href = data.redirect_url;
			return true;
		}
		if (!(await labAccessRefused(data.client.id, data.user.id))) return false;
		takeReturn();
		await signOut();
		password = '';
		refusedApp = data.client.name;
		return true;
	};

	const enter = async () => {
		if (await refuseForApp()) return;
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

	const passkey = () =>
		run(async () => {
			await signInWithPasskey();
			await enter();
		});

	const submit = (event: SubmitEvent) => {
		event.preventDefault();
		return run(async () => {
			if (mode === 'signin') {
				await signIn(email, password);
				await enter();
			} else {
				await sendPasswordReset(email);
				report('If an account exists for this email, a reset link is on its way.', false);
			}
		});
	};
</script>

<svelte:head>
	<title>{title} · doora</title>
</svelte:head>

<AuthShell {title} {subtitle}>
	{#if refusedApp}
		<LabRefusal app={refusedApp} />
		<button type="button" onclick={() => (refusedApp = '')} class="text-sm text-gray-400 underline"
			>Not you? Use another account</button
		>
	{:else}
		{#if mode === 'signin' && canUsePasskey && !forApp}
			<QuickUnlock />
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

			{#if mode === 'signin'}
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
	{/if}
</AuthShell>
