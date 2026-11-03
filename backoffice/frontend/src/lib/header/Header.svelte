<script lang="ts">
	import Fa from 'svelte-fa';
	import { faArrowRight, faDoorOpen } from '@fortawesome/free-solid-svg-icons';
	import { signOut } from '#lib/auth.ts';
	import type { CurrentUser } from '#lib/types.ts';
	import Avatar from '#lib/ui/Avatar.svelte';
	import ForceOpenModal from './ForceOpenModal.svelte';
	import SettingsModal from './SettingsModal.svelte';

	let { user }: { user: CurrentUser | null } = $props();

	let settingsOpen = $state(false);
	let forceOpenOpen = $state(false);

	const goToHome = () => (window.location.href = '/');
	const createKey = () => (window.location.href = '/items/create');
	const logout = async () => {
		await signOut();
		window.location.href = '/login';
	};
</script>

<header
	id="menu"
	class="sticky top-0 z-20 flex items-center justify-between gap-4 border-b bg-void/70 px-4 py-2.5 backdrop-blur-xl md:px-6 print:hidden"
>
	<a onclick={goToHome} class="flex cursor-pointer items-center gap-2.5">
		<span
			class="flex h-6 w-6 items-center justify-center rounded-md bg-ink font-mono text-[13px] leading-none font-bold text-void"
			aria-hidden="true">d</span
		>
		<span class="text-sm font-medium">doora</span>
		<span class="hidden text-gray-300 sm:inline" aria-hidden="true">/</span>
		<span class="hidden text-sm text-gray-400 sm:inline">backoffice</span>
	</a>
	{#if user}
		<span class="flex items-center gap-1.5 md:gap-2">
			<button type="button" onclick={() => (forceOpenOpen = true)} class="btn"
				><Fa icon={faDoorOpen} />OPEN DOOR</button
			>
			<a onclick={createKey} class="btn btn-primary">+ ISSUE KEY</a>
			<button
				type="button"
				onclick={() => (settingsOpen = true)}
				aria-label="Settings"
				class="ml-1 rounded-full ring-offset-2 ring-offset-void transition-shadow hover:ring-2 hover:ring-gray-300"
				><Avatar email={user.email} size={28} /></button
			>
			<a class="btn border-transparent" onclick={logout} title="Log out"
				><span class="sr-only md:not-sr-only">LOG OUT</span><Fa icon={faArrowRight} /></a
			>
		</span>
	{/if}
</header>

{#if user && forceOpenOpen}
	<ForceOpenModal onclose={() => (forceOpenOpen = false)} />
{/if}

{#if user && settingsOpen}
	<SettingsModal {user} onclose={() => (settingsOpen = false)} />
{/if}
