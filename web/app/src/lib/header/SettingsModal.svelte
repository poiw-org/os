<script lang="ts">
	import {
		deletePasskey,
		listPasskeys,
		passkeysSupported,
		registerPasskey,
		setEmail,
		setNickname,
		setPassword,
		signOut
	} from '#lib/auth.ts';
	import { timeAgo, withName } from '#lib/format.ts';
	import type { CurrentUser } from '#lib/types.ts';
	import Avatar from '#lib/ui/Avatar.svelte';

	let { user, onclose }: { user: CurrentUser; onclose: () => void } = $props();

	type Tab = 'profile' | 'security' | 'sessions';
	const tabs: [Tab, string][] = [
		['profile', 'Profile'],
		['security', 'Security'],
		['sessions', 'Sessions']
	];

	let dialog = $state<HTMLDialogElement>();
	let tab = $state<Tab>('profile');
	let nickname = $state(user.nickname);
	let email = $state(user.email);
	let password = $state('');
	let confirm = $state('');
	let message = $state('');
	let failed = $state(false);
	let busy = $state(false);

	$effect(() => dialog?.showModal());

	const select = (next: Tab) => {
		tab = next;
		message = '';
		if (next === 'security' && passkeysSupported()) refreshPasskeys();
	};

	let passkeys = $state<Awaited<ReturnType<typeof listPasskeys>>>([]);
	const refreshPasskeys = () =>
		listPasskeys().then(
			(list) => (passkeys = list),
			() => (passkeys = [])
		);

	const addPasskey = () =>
		run(async () => {
			await registerPasskey();
			await refreshPasskeys();
		}, 'Passkey added. You can now sign in with it.');

	const removePasskey = (id: string) =>
		run(async () => {
			await deletePasskey(id);
			await refreshPasskeys();
		}, 'Passkey removed.');

	// Runs one account change and reports how it went.
	const run = async (action: () => Promise<unknown>, done: string) => {
		busy = true;
		message = '';
		try {
			await action();
			failed = false;
			message = done;
		} catch (e) {
			failed = true;
			message = e instanceof Error ? e.message : 'Something went wrong.';
		}
		busy = false;
	};

	const saveName = (event: SubmitEvent) => {
		event.preventDefault();
		run(async () => {
			await setNickname(user.id, nickname.trim());
			// The header reads the user from the layout load, so reload to pick up the change.
			window.location.reload();
		}, 'Saved.');
	};

	const saveEmail = (event: SubmitEvent) => {
		event.preventDefault();
		const next = email.trim();
		run(
			() => setEmail(next),
			`Confirmation sent to ${next}. The change applies once you open the link.`
		);
	};

	const savePassword = (event: SubmitEvent) => {
		event.preventDefault();
		if (password !== confirm) {
			failed = true;
			message = 'The passwords do not match.';
			return;
		}
		run(async () => {
			await setPassword(password);
			password = confirm = '';
		}, 'Password updated.');
	};

	const leave = (everywhere: boolean) =>
		run(async () => {
			await signOut(everywhere);
			window.location.href = '/login';
		}, 'Signing out…');

	const nameChanged = $derived(nickname.trim() !== '' && nickname.trim() !== user.nickname);
	const emailChanged = $derived(email.trim() !== '' && email.trim() !== user.email);
</script>

{#snippet heading(label: string, hint?: string)}
	<span class="flex items-baseline justify-between gap-3">
		<span class="eyebrow">{label}</span>
		{#if hint}<span class="text-xs text-gray-400">{hint}</span>{/if}
	</span>
{/snippet}

<dialog
	bind:this={dialog}
	{onclose}
	onclick={(e) => e.target === dialog && dialog?.close()}
	aria-labelledby="settings-title"
	class="sheet m-auto w-[calc(100%-2rem)] max-w-lg overflow-hidden rounded-lg border bg-paper p-0 text-ink backdrop:bg-black/70"
>
	<header class="relative flex items-center gap-4 border-b px-6 pt-6 pb-5">
		<Avatar email={user.email} size={56} />
		<span class="flex min-w-0 flex-col pr-8">
			<h2 id="settings-title" class="truncate text-base font-bold">
				{user.nickname}
			</h2>
			<span class="truncate text-gray-400">{withName(user)}</span>
		</span>
		<button
			type="button"
			onclick={() => dialog?.close()}
			aria-label="Close"
			class="absolute top-4 right-4 flex h-8 w-8 items-center justify-center rounded-md text-gray-400 hover:bg-paper-2 hover:text-ink"
			>✕</button
		>
	</header>

	<div role="tablist" class="flex gap-1 border-b px-4">
		{#each tabs as [value, label] (value)}
			<button
				type="button"
				role="tab"
				aria-selected={tab === value}
				onclick={() => select(value)}
				class="-mb-px border-b-2 px-2.5 py-3 text-sm font-medium {tab === value
					? 'border-ink text-ink'
					: 'border-transparent text-gray-400 hover:text-ink'}">{label}</button
			>
		{/each}
	</div>

	<div class="flex min-h-[300px] flex-col gap-6 p-6">
		{#if tab === 'profile'}
			<form onsubmit={saveName} class="flex flex-col gap-1.5">
				<label for="settings-name">{@render heading('Display name', 'Shown in key logs')}</label>
				<span class="flex gap-2">
					<input
						id="settings-name"
						required
						maxlength="40"
						bind:value={nickname}
						class="min-w-0 flex-1 px-3.5 py-2.5"
					/>
					<button type="submit" disabled={busy || !nameChanged} class="btn btn-primary px-4"
						>Save</button
					>
				</span>
			</form>

			<div class="flex items-center gap-4 rounded-lg border p-4">
				<Avatar email={user.email} size={40} />
				<p class="flex-1 text-gray-400">
					Your avatar comes from Gravatar, with a generated one as fallback.
				</p>
				<a href="https://gravatar.com/profile" target="_blank" rel="noreferrer" class="btn"
					>Change</a
				>
			</div>

			<dl class="grid grid-cols-2 gap-4 border-t pt-5">
				<div class="flex flex-col gap-1">
					<dt class="eyebrow">Member since</dt>
					<dd>{user.createdAt.toLocaleDateString('el-GR', { dateStyle: 'medium' })}</dd>
				</div>
				<div class="flex flex-col gap-1">
					<dt class="eyebrow">Last sign-in</dt>
					<dd>{user.lastSignInAt ? timeAgo(user.lastSignInAt) : '—'}</dd>
				</div>
				<div class="col-span-2 flex flex-col gap-1">
					<dt class="eyebrow">User ID</dt>
					<dd class="font-mono text-xs text-gray-600 select-all">{user.id}</dd>
				</div>
			</dl>
		{:else if tab === 'security'}
			<form onsubmit={saveEmail} class="flex flex-col gap-1.5">
				<label for="settings-email">{@render heading('Email', 'Needs confirmation')}</label>
				<span class="flex gap-2">
					<input
						id="settings-email"
						type="email"
						required
						autocomplete="email"
						bind:value={email}
						class="min-w-0 flex-1 px-3.5 py-2.5"
					/>
					<button type="submit" disabled={busy || !emailChanged} class="btn btn-primary px-4"
						>Update</button
					>
				</span>
				{#if user.pendingEmail}
					<span class="text-xs text-yellow-500"
						>Waiting for confirmation of {user.pendingEmail}.</span
					>
				{/if}
			</form>

			<form onsubmit={savePassword} class="flex flex-col gap-3 border-t pt-5">
				{@render heading('New password', '8+ characters')}
				<input
					type="password"
					required
					minlength="8"
					autocomplete="new-password"
					placeholder="New password"
					aria-label="New password"
					bind:value={password}
					class="px-3.5 py-2.5"
				/>
				<input
					type="password"
					required
					autocomplete="new-password"
					placeholder="Confirm new password"
					aria-label="Confirm new password"
					bind:value={confirm}
					class="px-3.5 py-2.5"
				/>
				<button
					type="submit"
					disabled={busy || !password}
					class="btn btn-primary self-end px-4 py-2">Change password</button
				>
			</form>

			{#if passkeysSupported()}
				<section class="flex flex-col gap-3 border-t pt-5">
					{@render heading('Passkeys', 'Face ID, Touch ID or a security key')}
					{#if passkeys.length}
						<ul class="panel">
							{#each passkeys as key (key.id)}
								<li class="row">
									<span class="flex min-w-0 flex-col">
										<span class="truncate font-medium">{key.friendly_name || 'Passkey'}</span>
										<span class="text-xs text-gray-400"
											>Added {timeAgo(new Date(key.created_at))}{key.last_used_at
												? ` · used ${timeAgo(new Date(key.last_used_at))}`
												: ''}</span
										>
									</span>
									<button
										type="button"
										disabled={busy}
										onclick={() => removePasskey(key.id)}
										class="btn tone-red shrink-0">Remove</button
									>
								</li>
							{/each}
						</ul>
					{/if}
					<button type="button" disabled={busy} onclick={addPasskey} class="btn self-start"
						>+ Add a passkey</button
					>
				</section>
			{/if}
		{:else}
			<div class="panel">
				<div class="row py-4">
					<span class="flex flex-col">
						<span class="font-medium">This device</span>
						<span class="text-gray-400">Sign out of the operating system here.</span>
					</span>
					<button type="button" disabled={busy} onclick={() => leave(false)} class="btn"
						>Sign out</button
					>
				</div>
				<div class="row py-4">
					<span class="flex flex-col">
						<span class="font-medium">All devices</span>
						<span class="text-gray-400">Revoke every session, including this one.</span>
					</span>
					<button
						type="button"
						disabled={busy}
						onclick={() => leave(true)}
						class="btn tone-red shrink-0">Sign out all</button
					>
				</div>
			</div>
		{/if}

		{#if message}
			<p
				role="status"
				class="rounded-lg border px-3.5 py-2.5 {failed
					? 'border-red-500/40 text-red-400'
					: 'border-green-700/40 text-green-700'}"
			>
				{message}
			</p>
		{/if}
	</div>
</dialog>
