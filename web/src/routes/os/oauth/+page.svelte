<script lang="ts">
	import { onMount } from 'svelte';
	import type { OAuthClient, OAuthClientTokenEndpointAuthMethod } from '@supabase/supabase-js';
	import {
		type App,
		createOAuthClient,
		deleteOAuthClient,
		listOAuthClients,
		regenerateOAuthSecret,
		updateOAuthClient
	} from '#lib/api.ts';
	import { config } from '#lib/config.ts';
	import { formatDate } from '#lib/format.ts';
	import Loading from '#lib/loading.svelte';
	import { ask, toast } from '#lib/notify.svelte.ts';
	import { t } from '#lib/i18n.svelte.ts';

	// What an app that signs people in through doora (Cloudflare Access, for one) is configured with.
	// Token and Certs go through the `oidc` function, whose ID tokens add the user's `roles`.
	const auth = `${config.supabaseUrl}/auth/v1`;
	const oidc = `${config.supabaseUrl}/functions/v1/oidc`;
	const endpoints: [string, string][] = [
		['Issuer', oidc],
		['Auth URL', `${auth}/oauth/authorize`],
		['Token URL', `${oidc}/token`],
		['Certs (JWKS) URL', `${oidc}/jwks`]
	];

	const METHODS: [OAuthClientTokenEndpointAuthMethod, string][] = [
		['client_secret_basic', 'Confidential: secret in the Authorization header'],
		['client_secret_post', 'Confidential: secret in the request body'],
		['none', 'Public: no secret, PKCE only (phone and browser apps)']
	];
	const methodLabel = (m: string) => t(METHODS.find(([value]) => value === m)?.[1] ?? m);

	let clients = $state<App[]>();
	let busy = $state(false);
	// The form both adds an app and edits one; `editing` is the app's id when editing.
	let formOpen = $state(false);
	let editing = $state<string>();
	let name = $state('');
	let redirectUris = $state('');
	let method = $state<OAuthClientTokenEndpointAuthMethod>('client_secret_basic');
	let labOnly = $state(false);
	// Supabase returns a secret only when it is made, so it is shown once, here.
	let secret = $state<{ name: string; id: string; value: string }>();

	const refresh = async () => {
		try {
			clients = await listOAuthClients();
		} catch (e) {
			clients = [];
			toast.error(e instanceof Error ? e.message : t('Could not load the apps.'));
		}
	};
	onMount(refresh);

	const run = async (action: () => Promise<unknown>, done?: string) => {
		busy = true;
		try {
			await action();
			if (done) toast.success(done);
			await refresh();
		} catch (e) {
			toast.error(e instanceof Error ? e.message : t('That did not work.'));
		}
		busy = false;
	};

	const openForm = (client?: App) => {
		formOpen = true;
		editing = client?.client_id;
		name = client?.client_name ?? '';
		redirectUris = client?.redirect_uris.join('\n') ?? '';
		method = client?.token_endpoint_auth_method ?? 'client_secret_basic';
		labOnly = client?.lab_access_only ?? false;
	};

	const save = (event: SubmitEvent) => {
		event.preventDefault();
		const fields = {
			name: name.trim(),
			redirectUris: redirectUris.split(/\s+/).filter(Boolean),
			labOnly
		};
		run(
			async () => {
				if (editing) await updateOAuthClient(editing, fields);
				else {
					const client = await createOAuthClient({ ...fields, authMethod: method });
					if (client.client_secret)
						secret = {
							name: client.client_name,
							id: client.client_id,
							value: client.client_secret
						};
				}
				formOpen = false;
			},
			editing ? t('Saved.') : t('App added.')
		);
	};

	const newSecret = async (client: OAuthClient) => {
		const accepted = await ask({
			title: t('Make a new secret?'),
			message: t('{name} stops working until you give it the new secret.', {
				name: client.client_name
			}),
			confirmLabel: t('New secret'),
			tone: 'danger'
		});
		if (!accepted) return;
		run(async () => {
			const updated = await regenerateOAuthSecret(client.client_id);
			if (updated.client_secret)
				secret = { name: updated.client_name, id: updated.client_id, value: updated.client_secret };
		});
	};

	const remove = async (client: OAuthClient) => {
		const accepted = await ask({
			title: t('Delete {name}?', { name: client.client_name }),
			message: t(
				'Nobody can sign in to it with doora any more, and everyone signed in is signed out.'
			),
			confirmLabel: t('Delete'),
			tone: 'danger'
		});
		if (accepted)
			run(
				() => deleteOAuthClient(client.client_id),
				t('{name} deleted.', { name: client.client_name })
			);
	};
</script>

<svelte:head><title>{t('Apps')}</title></svelte:head>

<div class="stagger flex max-w-4xl flex-col gap-6 px-4 py-6 md:px-8 md:py-10">
	<div class="flex flex-wrap items-center justify-between gap-3">
		<h1 class="text-2xl">{t('Apps')}</h1>
		{#if !formOpen}
			<button type="button" class="btn btn-primary" onclick={() => openForm()}
				>{t('+ Add an app')}</button
			>
		{/if}
	</div>
	<p class="text-gray-400">
		{t(
			'Apps that sign people in with their po/iw account. Each person sees a consent screen the first time.'
		)}
	</p>

	{#if secret}
		<div class="panel flex flex-col gap-3 border-amber-500/50 p-4" role="status">
			<p class="font-bold">{t('Secret for {name}', { name: secret.name })}</p>
			<p class="text-sm text-gray-400">{t('Copy it now. It is not shown again.')}</p>
			<dl class="grid gap-1 font-mono text-sm break-all">
				<dt class="eyebrow">{t('Client ID')}</dt>
				<dd class="select-all">{secret.id}</dd>
				<dt class="eyebrow">{t('Client secret')}</dt>
				<dd class="select-all">{secret.value}</dd>
			</dl>
			<button type="button" class="btn self-start" onclick={() => (secret = undefined)}
				>{t('I copied it')}</button
			>
		</div>
	{/if}

	{#if formOpen}
		<form onsubmit={save} class="panel grid gap-4 p-4">
			<h2 class="font-bold">{editing ? t('Edit app') : t('New app')}</h2>
			<label class="flex flex-col gap-1.5">
				<span class="eyebrow">{t('Name people see on the consent screen')}</span>
				<input required maxlength="100" bind:value={name} class="px-3 py-2" />
			</label>
			<label class="flex flex-col gap-1.5">
				<span class="eyebrow">{t('Redirect URIs, one per line (matched exactly)')}</span>
				<textarea
					required
					rows="3"
					bind:value={redirectUris}
					placeholder="https://your-team.cloudflareaccess.com/cdn-cgi/access/callback"
					class="px-3 py-2 font-mono text-sm"></textarea>
			</label>
			{#if !editing}
				<label class="flex flex-col gap-1.5">
					<span class="eyebrow">{t('Client type')}</span>
					<select bind:value={method} class="px-3 py-2">
						{#each METHODS as [value, label] (value)}
							<option {value}>{t(label)}</option>
						{/each}
					</select>
				</label>
			{/if}
			<label class="flex items-start gap-2">
				<input type="checkbox" bind:checked={labOnly} class="mt-1" />
				<span>
					{t('Only people with an active key')}
					<span class="block text-sm text-gray-400"
						>{t('Everyone else is turned away, also when their key later stops working.')}</span
					>
				</span>
			</label>
			<div class="flex justify-end gap-2">
				<button type="button" class="btn" disabled={busy} onclick={() => (formOpen = false)}
					>{t('Cancel')}</button
				>
				<button type="submit" class="btn btn-primary" disabled={busy}
					>{editing ? t('Save') : t('Add app')}</button
				>
			</div>
		</form>
	{/if}

	{#if !clients}
		<Loading />
	{:else if clients.length === 0}
		<p class="panel px-4 py-8 text-center text-gray-400">{t('No apps yet.')}</p>
	{:else}
		<ul class="panel">
			{#each clients as client (client.client_id)}
				<li class="row flex-col items-stretch gap-3 py-4 md:flex-row md:items-start">
					<div class="flex min-w-0 flex-1 flex-col gap-1">
						<p class="font-bold">{client.client_name}</p>
						<p class="truncate font-mono text-xs text-gray-400 select-all">{client.client_id}</p>
						<p class="text-sm text-gray-400">
							{methodLabel(client.token_endpoint_auth_method)} · {t('added {date}', {
								date: formatDate(new Date(client.created_at))
							})}
						</p>
						<p class="text-sm">
							{client.lab_access_only
								? t('Only people with an active key')
								: t('Everyone with an account')}
						</p>
						<ul class="font-mono text-xs break-all">
							{#each client.redirect_uris as uri (uri)}<li>{uri}</li>{/each}
						</ul>
					</div>
					<div class="flex shrink-0 flex-wrap gap-2">
						<button type="button" class="btn" disabled={busy} onclick={() => openForm(client)}
							>{t('Edit')}</button
						>
						{#if client.client_type === 'confidential'}
							<button type="button" class="btn" disabled={busy} onclick={() => newSecret(client)}
								>{t('New secret')}</button
							>
						{/if}
						<button
							type="button"
							class="btn tone-red"
							disabled={busy}
							onclick={() => remove(client)}>{t('Delete')}</button
						>
					</div>
				</li>
			{/each}
		</ul>
	{/if}

	<section class="flex flex-col gap-2">
		<h2 class="font-bold">{t('Endpoints')}</h2>
		<dl class="panel grid gap-x-6 gap-y-2 p-4 text-sm md:grid-cols-[max-content_1fr]">
			{#each endpoints as [label, url] (label)}
				<dt class="eyebrow">{t(label)}</dt>
				<dd class="font-mono break-all select-all">{url}</dd>
			{/each}
		</dl>
		<p class="text-sm text-gray-400">
			{t('Apps must use PKCE. Scopes:')} <span class="font-mono">openid email profile</span>.
			{t("The ID token's")} <span class="font-mono">roles</span>
			{t('claim lists')}
			<span class="font-mono">admin</span>
			{t('and')} <span class="font-mono">lab-access</span> ({t('an active key')}).
		</p>
	</section>
</div>
