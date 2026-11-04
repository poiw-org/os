<script lang="ts">
	import { signOut } from '#lib/auth.ts';
	import type { CurrentUser } from '#lib/types.ts';
	import Avatar from '#lib/ui/Avatar.svelte';
	import Brand from '#lib/ui/Brand.svelte';
	import ForceOpenModal from './ForceOpenModal.svelte';
	import MobileMenu from './MobileMenu.svelte';
	import SettingsModal from './SettingsModal.svelte';

	let { user }: { user: CurrentUser | null } = $props();

	let settingsOpen = $state(false);
	let forceOpenOpen = $state(false);
	let menu = $state<MobileMenu>();

	const goToHome = () => (window.location.href = '/');
	const createKey = () => (window.location.href = '/items/create');
	const logout = async () => {
		await signOut();
		window.location.href = '/login';
	};

	// A dialog opened from the phone menu returns focus to "Menu": the row that opened it is gone.
	const refocus = () => {
		if (!document.activeElement || document.activeElement === document.body) menu?.focus();
	};
</script>

{#snippet brand()}
	<a onclick={goToHome} class="flex cursor-pointer items-center gap-2.5">
		<Brand size={28} />
		<span class="font-mono text-sm font-bold">doora</span>
		<span class="hidden text-gray-300 sm:inline" aria-hidden="true">/</span>
		<span class="hidden text-sm text-gray-400 sm:inline">backoffice</span>
	</a>
{/snippet}

<header id="menu" class="sticky top-0 z-20 border-b bg-void print:hidden">
	<div
		class="mx-auto flex w-full max-w-[1500px] items-center justify-between gap-4 px-4 py-2.5 md:px-8"
	>
		{@render brand()}
		{#if user}
			<span class="md:hidden">
				<MobileMenu
					bind:this={menu}
					{user}
					{brand}
					onunlock={() => (forceOpenOpen = true)}
					onsettings={() => (settingsOpen = true)}
					onlogout={logout}
				/>
			</span>
			<span class="hidden items-center gap-1.5 md:flex md:gap-2">
				{#if user.canUnlock}
					<button type="button" onclick={() => (forceOpenOpen = true)} class="btn"
						>Remote unlock</button
					>
				{/if}
				{#if user.isAdmin}<a onclick={createKey} class="btn btn-primary">Issue key</a>{/if}
				<button
					type="button"
					onclick={() => (settingsOpen = true)}
					aria-label="Settings"
					class="ml-1 rounded-full hover:opacity-80"><Avatar email={user.email} size={28} /></button
				>
				<button type="button" class="btn border-transparent" onclick={logout}>Log out</button>
			</span>
		{/if}
	</div>
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
