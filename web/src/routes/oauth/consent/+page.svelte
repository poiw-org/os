<script lang="ts">
	import { onMount } from 'svelte';
	import { page } from '$app/state';
	import type { OAuthAuthorizationDetails } from '@supabase/supabase-js';
	import { t } from '#lib/i18n.svelte.ts';
	import AuthShell from '#lib/auth/AuthShell.svelte';
	import LabRefusal from '#lib/auth/LabRefusal.svelte';
	import { labAccessRefused, signOut } from '#lib/auth.ts';
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
	// The app is for people with an active key and this person has none. The `oidc` function refuses
	// their token anyway; this says so before they try.
	let turnedAway = $state(false);

	const scopes = $derived(details?.scope.split(' ').filter(Boolean) ?? []);
	const destination = $derived(details ? new URL(details.redirect_uri).host : '');

	const go = (url: string) => {
		leaving = true;
		window.location.href = url;
	};

	onMount(async () => {
		if (!authorizationId) {
			error = t('This link is missing its sign-in request. Start again from the app.');
			return;
		}
		const { data, error: failure } =
			await supabase.auth.oauth.getAuthorizationDetails(authorizationId);
		if (failure) error = t('{message}. Start again from the app.', { message: failure.message });
		// Already allowed earlier for these permissions: Supabase answers with where to go straight away.
		else if (!('authorization_id' in data)) go(data.redirect_url);
		else {
			turnedAway = await labAccessRefused(data.client.id, data.user.id);
			details = data;
		}
	});

	const decide = async (allow: boolean) => {
		busy = true;
		const oauth = supabase.auth.oauth;
		const options = { skipBrowserRedirect: true };
		const { data, error: failure } = allow
			? await oauth.approveAuthorization(authorizationId, options)
			: await oauth.denyAuthorization(authorizationId, options);
		if (failure) {
			error = t('{message}. Start again from the app.', { message: failure.message });
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

<svelte:head
	><title
		>{details ? t('Sign in to {app}', { app: details.client.name }) : t('Sign in to an app')}</title
	></svelte:head
>

<AuthShell
	title={details ? t('Sign in to {app}', { app: details.client.name }) : t('Sign in to an app')}
	subtitle={details ? t('as {email}', { email: details.user.email }) : undefined}
>
	{#if error}
		<p class="text-red-400" role="alert">{error}</p>
	{:else if leaving}
		<p role="status" class="text-gray-400">{t('Taking you back to the app…')}</p>
	{:else if !details}
		<p role="status" class="text-gray-400">{t('Checking the request…')}</p>
	{:else}
		<div class="flex flex-col gap-4">
			{#if turnedAway}
				<LabRefusal app={details.client.name} />
			{:else}
				<p>
					{t('{app} wants to use your po/iw account to sign you in. It will be able to:', {
						app: details.client.name
					})}
				</p>
				<ul class="panel">
					{#each scopes as scope (scope)}
						<li class="row">{SCOPES[scope] ? t(SCOPES[scope]) : scope}</li>
					{/each}
				</ul>
				<p class="text-sm text-gray-400">
					{t(
						'It cannot open the door, see your keys or use the OS. Afterwards you go to {destination}. You can disconnect it any time in Settings.',
						{ destination }
					)}
				</p>
			{/if}
			<div class="flex gap-2">
				<button type="button" class="key flex-1" disabled={busy} onclick={() => decide(false)}
					>{turnedAway ? t('Back to the app') : t('Deny')}</button
				>
				{#if !turnedAway}
					<button
						type="button"
						class="key key-signal flex-1"
						disabled={busy}
						onclick={() => decide(true)}>{t('Allow')}</button
					>
				{/if}
			</div>
			<button
				type="button"
				class="self-start text-sm text-gray-400 underline"
				disabled={busy}
				onclick={switchAccount}>{t('Not you? Use another account')}</button
			>
		</div>
	{/if}
</AuthShell>
