<script lang="ts">
	import { onMount } from 'svelte';
	import { page } from '$app/state';
	import type { OAuthAuthorizationDetails } from '@supabase/supabase-js';
	import AuthShell from '#lib/auth/AuthShell.svelte';
	import { signOut } from '#lib/auth.ts';
	import { supabase } from '#lib/supabase.ts';

	// Supabase's OAuth server sends people here (the Site URL plus the Authorization Path) when an app
	// such as Cloudflare Access asks to sign them in. The layout has already made sure they are fully
	// signed in; Supabase checks the client, its redirect URI and PKCE before and after this page.
	const authorizationId = page.url.searchParams.get('authorization_id') ?? '';

	// What each scope lets the app see, in the words people read on the screen.
	const SCOPES: Record<string, string> = {
		openid: 'Confirm that it is you',
		email: 'See your email address',
		profile: 'See your name and picture',
		phone: 'See your phone number',
		offline_access: 'Stay signed in to it while you are away'
	};

	let details = $state<OAuthAuthorizationDetails>();
	let error = $state('');
	let leaving = $state(false);
	let busy = $state(false);

	const scopes = $derived(details?.scope.split(' ').filter(Boolean) ?? []);
	const destination = $derived(details ? new URL(details.redirect_uri).host : '');

	const go = (url: string) => {
		leaving = true;
		window.location.href = url;
	};

	onMount(async () => {
		if (!authorizationId) {
			error = 'This link is missing its sign-in request. Start again from the app.';
			return;
		}
		const { data, error: failure } =
			await supabase.auth.oauth.getAuthorizationDetails(authorizationId);
		if (failure) error = `${failure.message}. Start again from the app.`;
		// Already allowed earlier for these permissions: Supabase answers with where to go straight away.
		else if (!('authorization_id' in data)) go(data.redirect_url);
		else details = data;
	});

	const decide = async (allow: boolean) => {
		busy = true;
		const oauth = supabase.auth.oauth;
		const options = { skipBrowserRedirect: true };
		const { data, error: failure } = allow
			? await oauth.approveAuthorization(authorizationId, options)
			: await oauth.denyAuthorization(authorizationId, options);
		if (failure) {
			error = `${failure.message}. Start again from the app.`;
			busy = false;
		} else go(data.redirect_url);
	};

	// Another account: sign out and come back here, which goes through the login first.
	const switchAccount = async () => {
		busy = true;
		await signOut();
		window.location.href = page.url.pathname + page.url.search;
	};
</script>

<svelte:head><title>Sign in to {details?.client.name ?? 'an app'}</title></svelte:head>

<AuthShell
	title={details ? `Sign in to ${details.client.name}` : 'Sign in to an app'}
	subtitle={details ? `as ${details.user.email}` : undefined}
>
	{#if error}
		<p class="text-red-400" role="alert">{error}</p>
	{:else if leaving}
		<p role="status" class="text-gray-400">Taking you back to the app…</p>
	{:else if !details}
		<p role="status" class="text-gray-400">Checking the request…</p>
	{:else}
		<div class="flex flex-col gap-4">
			<p>
				<strong>{details.client.name}</strong> wants to use your po/iw account to sign you in. It will
				be able to:
			</p>
			<ul class="panel">
				{#each scopes as scope (scope)}
					<li class="row">{SCOPES[scope] ?? scope}</li>
				{/each}
			</ul>
			<p class="text-sm text-gray-400">
				It cannot open the door, see your keys or use the OS. Afterwards you go to
				<span class="font-mono">{destination}</span>. You can disconnect it any time in Settings.
			</p>
			<div class="flex gap-2">
				<button type="button" class="key flex-1" disabled={busy} onclick={() => decide(false)}
					>Deny</button
				>
				<button
					type="button"
					class="key key-signal flex-1"
					disabled={busy}
					onclick={() => decide(true)}>Allow</button
				>
			</div>
			<button
				type="button"
				class="self-start text-sm text-gray-400 underline"
				disabled={busy}
				onclick={switchAccount}>Not you? Use another account</button
			>
		</div>
	{/if}
</AuthShell>
