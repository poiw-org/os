<script lang="ts">
	import type { CurrentUser } from '#lib/types.ts';
	import Avatar from '#lib/ui/Avatar.svelte';
	import { resolve } from '$app/paths';
	import OsLabel from '#lib/ui/OsLabel.svelte';
	import BluetoothApproval from './BluetoothApproval.svelte';
	import DoorConsole from './DoorConsole.svelte';
	import DevicesMenu from './DevicesMenu.svelte';
	import ForceOpenModal from './ForceOpenModal.svelte';
	import MobileMenu from './MobileMenu.svelte';
	import SettingsModal from './SettingsModal.svelte';
	import IssueKeyMenu from '#lib/ui/IssueKeyMenu.svelte';
	import LocaleSwitch from '#lib/ui/LocaleSwitch.svelte';
	import { t } from '#lib/i18n.svelte.ts';

	let { user }: { user: CurrentUser | null } = $props();

	let settingsOpen = $state(false);
	let consoleOpen = $state(false);
	let doorLabel = $state('');
	let forceOpenOpen = $state(false);
	let menu = $state<MobileMenu>();

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

<header id="menu" class="bar full app-bar print:hidden">
	{@render brand()}
	{#if user?.isAdmin}
		<a href={resolve('/os/firmware')} class="label only-md">{t('Firmware')}</a>
		<a href={resolve('/os/oauth')} class="label only-md">{t('Apps')}</a>
	{/if}
	{#if user}
		<a href={resolve('/os/keys')} class="label only-md">{t('My keys')}</a>
	{/if}
	<div class="grow"></div>
	<LocaleSwitch />
	{#if user?.isAdmin}
		<span class="key-cell">
			<DevicesMenu bind:label={doorLabel} onopen={() => (consoleOpen = true)} />
		</span>
	{/if}
	{#if user}
		<span class="only-sm">
			<MobileMenu
				bind:this={menu}
				{user}
				{brand}
				onunlock={() => (fromMenu = forceOpenOpen = true)}
				onsettings={() => (fromMenu = settingsOpen = true)}
			/>
		</span>
		<span class="only-md">
			{#if user.canUnlock}
				<button type="button" onclick={() => (forceOpenOpen = true)} class="key"
					>{t('Remote unlock')}</button
				>
			{/if}
			{#if user.isAdmin}
				<IssueKeyMenu trigger="key key-signal" align="right" />
			{/if}
			<button
				type="button"
				onclick={() => (settingsOpen = true)}
				aria-label={t('Settings')}
				class="ml-1 rounded-full hover:opacity-80"><Avatar email={user.email} size={28} /></button
			>
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

{#if user?.isAdmin}
	<BluetoothApproval />
{/if}

{#if user?.isAdmin && consoleOpen}
	<DoorConsole label={doorLabel} onclose={() => (consoleOpen = false)} />
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
