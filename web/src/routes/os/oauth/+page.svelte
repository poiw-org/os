<script lang="ts">
	import { onMount } from 'svelte';
	import type { OAuthClient, OAuthClientTokenEndpointAuthMethod } from '@supabase/supabase-js';
	import {
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

	// What an app that signs people in through doora (Cloudflare Access, for one) is configured with.
	const auth = `${config.supabaseUrl}/auth/v1`;
	const endpoints: [string, string][] = [
		['Issuer', auth],
		['Auth URL', `${auth}/oauth/authorize`],
		['Token URL', `${auth}/oauth/token`],
		['Certs (JWKS) URL', `${auth}/.well-known/jwks.json`],
		['Userinfo URL', `${auth}/oauth/userinfo`],
		['OIDC discovery', `${auth}/.well-known/openid-configuration`]
	];

	const METHODS: [OAuthClientTokenEndpointAuthMethod, string][] = [
		['client_secret_basic', 'Confidential: secret in the Authorization header'],
		['client_secret_post', 'Confidential: secret in the request body'],
		['none', 'Public: no secret, PKCE only (phone and browser apps)']
	];
	const methodLabel = (m: string) => METHODS.find(([value]) => value === m)?.[1] ?? m;

	let clients = $state<OAuthClient[]>();
	let busy = $state(false);
	// The form both adds an app and edits one; `editing` is the app's id when editing.
	let formOpen = $state(false);
	let editing = $state<string>();
	let name = $state('');
	let redirectUris = $state('');
	let method = $state<OAuthClientTokenEndpointAuthMethod>('client_secret_basic');
	// Supabase returns a secret only when it is made, so it is shown once, here.
	let secret = $state<{ name: string; id: string; value: string }>();

	const refresh = async () => {
		try {
			clients = await listOAuthClients();
		} catch (e) {
			clients = [];
			toast.error(e instanceof Error ? e.message : 'Could not load the apps.');
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
			toast.error(e instanceof Error ? e.message : 'That did not work.');
		}
		busy = false;
	};

	const openForm = (client?: OAuthClient) => {
		formOpen = true;
		editing = client?.client_id;
		name = client?.client_name ?? '';
		redirectUris = client?.redirect_uris.join('\n') ?? '';
		method = client?.token_endpoint_auth_method ?? 'client_secret_basic';
	};

	const save = (event: SubmitEvent) => {
		event.preventDefault();
		const fields = {
			name: name.trim(),
			redirectUris: redirectUris.split(/\s+/).filter(Boolean)
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
			editing ? 'Saved.' : 'App added.'
		);
	};

	const newSecret = async (client: OAuthClient) => {
		const accepted = await ask({
			title: 'Make a new secret?',
			message: `${client.client_name} stops working until you give it the new secret.`,
			confirmLabel: 'New secret',
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
			title: `Delete ${client.client_name}?`,
			message:
				'Nobody can sign in to it with doora any more, and everyone signed in is signed out.',
			confirmLabel: 'Delete',
			tone: 'danger'
		});
		if (accepted) run(() => deleteOAuthClient(client.client_id), `${client.client_name} deleted.`);
	};
</script>

<svelte:head><title>Apps</title></svelte:head>

<div class="stagger flex max-w-4xl flex-col gap-6 px-4 py-6 md:px-8 md:py-10">
	<div class="flex flex-wrap items-center justify-between gap-3">
		<h1 class="text-2xl">Apps</h1>
		{#if !formOpen}
			<button type="button" class="btn btn-primary" onclick={() => openForm()}>+ Add an app</button>
		{/if}
	</div>
	<p class="text-gray-400">
		Apps that sign people in with their doora account. Each person sees a consent screen the first
		time.
	</p>

	{#if secret}
		<div class="panel flex flex-col gap-3 border-amber-500/50 p-4" role="status">
			<p class="font-bold">Secret for {secret.name}</p>
			<p class="text-sm text-gray-400">Copy it now. It is not shown again.</p>
			<dl class="grid gap-1 font-mono text-sm break-all">
				<dt class="eyebrow">Client ID</dt>
				<dd class="select-all">{secret.id}</dd>
				<dt class="eyebrow">Client secret</dt>
				<dd class="select-all">{secret.value}</dd>
			</dl>
			<button type="button" class="btn self-start" onclick={() => (secret = undefined)}
				>I copied it</button
			>
		</div>
	{/if}

	{#if formOpen}
		<form onsubmit={save} class="panel grid gap-4 p-4">
			<h2 class="font-bold">{editing ? 'Edit app' : 'New app'}</h2>
			<label class="flex flex-col gap-1.5">
				<span class="eyebrow">Name people see on the consent screen</span>
				<input required maxlength="100" bind:value={name} class="px-3 py-2" />
			</label>
			<label class="flex flex-col gap-1.5">
				<span class="eyebrow">Redirect URIs, one per line (matched exactly)</span>
				<textarea
					required
					rows="3"
					bind:value={redirectUris}
					placeholder="https://your-team.cloudflareaccess.com/cdn-cgi/access/callback"
					class="px-3 py-2 font-mono text-sm"></textarea>
			</label>
			{#if !editing}
				<label class="flex flex-col gap-1.5">
					<span class="eyebrow">Client type</span>
					<select bind:value={method} class="px-3 py-2">
						{#each METHODS as [value, label] (value)}
							<option {value}>{label}</option>
						{/each}
					</select>
				</label>
			{/if}
			<div class="flex justify-end gap-2">
				<button type="button" class="btn" disabled={busy} onclick={() => (formOpen = false)}
					>Cancel</button
				>
				<button type="submit" class="btn btn-primary" disabled={busy}
					>{editing ? 'Save' : 'Add app'}</button
				>
			</div>
		</form>
	{/if}

	{#if !clients}
		<Loading />
	{:else if clients.length === 0}
		<p class="panel px-4 py-8 text-center text-gray-400">No apps yet.</p>
	{:else}
		<ul class="panel">
			{#each clients as client (client.client_id)}
				<li class="row flex-col items-stretch gap-3 py-4 md:flex-row md:items-start">
					<div class="flex min-w-0 flex-1 flex-col gap-1">
						<p class="font-bold">{client.client_name}</p>
						<p class="truncate font-mono text-xs text-gray-400 select-all">{client.client_id}</p>
						<p class="text-sm text-gray-400">
							{methodLabel(client.token_endpoint_auth_method)} · added {formatDate(
								new Date(client.created_at)
							)}
						</p>
						<ul class="font-mono text-xs break-all">
							{#each client.redirect_uris as uri (uri)}<li>{uri}</li>{/each}
						</ul>
					</div>
					<div class="flex shrink-0 flex-wrap gap-2">
						<button type="button" class="btn" disabled={busy} onclick={() => openForm(client)}
							>Edit</button
						>
						{#if client.client_type === 'confidential'}
							<button type="button" class="btn" disabled={busy} onclick={() => newSecret(client)}
								>New secret</button
							>
						{/if}
						<button
							type="button"
							class="btn tone-red"
							disabled={busy}
							onclick={() => remove(client)}>Delete</button
						>
					</div>
				</li>
			{/each}
		</ul>
	{/if}

	<section class="flex flex-col gap-2">
		<h2 class="font-bold">Endpoints</h2>
		<dl class="panel grid gap-x-6 gap-y-2 p-4 text-sm md:grid-cols-[max-content_1fr]">
			{#each endpoints as [label, url] (label)}
				<dt class="eyebrow">{label}</dt>
				<dd class="font-mono break-all select-all">{url}</dd>
			{/each}
		</dl>
		<p class="text-sm text-gray-400">
			Apps must use PKCE. Scopes: <span class="font-mono">openid email profile</span>.
		</p>
	</section>
</div>
