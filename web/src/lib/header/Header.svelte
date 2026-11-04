<script lang="ts">
	import { signOut } from '#lib/auth.ts';
	import type { CurrentUser } from '#lib/types.ts';
	import Avatar from '#lib/ui/Avatar.svelte';
	import { resolve } from '$app/paths';
	import OsLabel from '#lib/ui/OsLabel.svelte';
	import ForceOpenModal from './ForceOpenModal.svelte';
	import MobileMenu from './MobileMenu.svelte';
	import SettingsModal from './SettingsModal.svelte';

	let { user }: { user: CurrentUser | null } = $props();

	let settingsOpen = $state(false);
	let forceOpenOpen = $state(false);
	let menu = $state<MobileMenu>();

	const logout = async () => {
		await signOut();
		window.location.href = '/login';
	};

	// A dialog opened from the phone menu returns focus to "Menu": the row that opened it is gone.
	let fromMenu = false;
	const refocus = () => {
		if (fromMenu) menu?.focus();
		fromMenu = false;
	};
</script>

{#snippet brand()}
	<a href={resolve('/os')} class="label"><OsLabel /></a>
{/snippet}

<header id="menu" class="bar full print:hidden">
	{@render brand()}
	<div class="grow"></div>
	{#if user}
		<span class="only-sm">
			<MobileMenu
				bind:this={menu}
				{user}
				{brand}
				onunlock={() => (fromMenu = forceOpenOpen = true)}
				onsettings={() => (fromMenu = settingsOpen = true)}
				onlogout={logout}
			/>
		</span>
		<span class="only-md">
			{#if user.canUnlock}
				<button type="button" onclick={() => (forceOpenOpen = true)} class="key"
					>Remote unlock</button
				>
			{/if}
			{#if user.isAdmin}
				<a href={resolve('/os/items/create')} class="key key-signal">Issue key</a>
			{/if}
			<button
				type="button"
				onclick={() => (settingsOpen = true)}
				aria-label="Settings"
				class="ml-1 rounded-full hover:opacity-80"><Avatar email={user.email} size={28} /></button
			>
			<button type="button" class="label" onclick={logout}>Log out</button>
		</span>
	{/if}
</header>

{#if user && forceOpenOpen}
	<ForceOpenModal
		admin={user.isAdmin}
		onclose={() => {
			forceOpenOpen = false;
			refocus();
		}}
	/>
{/if}

{#if user && settingsOpen}
	<SettingsModal
		{user}
		onclose={() => {
			settingsOpen = false;
			refocus();
		}}
	/>
{/if}
