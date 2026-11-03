<script lang="ts">
	import { setNickname, setPassword } from '#lib/auth.ts';
	import type { CurrentUser } from '#lib/types.ts';
	import Avatar from '#lib/ui/Avatar.svelte';

	let { user, onclose }: { user: CurrentUser; onclose: () => void } = $props();

	let dialog = $state<HTMLDialogElement>();
	let nickname = $state(user.nickname);
	let password = $state('');
	let message = $state('');
	let busy = $state(false);

	$effect(() => dialog?.showModal());

	const save = async (event: SubmitEvent) => {
		event.preventDefault();
		busy = true;
		message = '';
		try {
			const name = nickname.trim();
			if (name && name !== user.nickname) await setNickname(user.id, name);
			if (password) await setPassword(password);
			// The header reads the user from the layout load, so reload to pick up the change.
			window.location.reload();
		} catch (e) {
			message = e instanceof Error ? e.message : 'Could not save the settings.';
			busy = false;
		}
	};
</script>

<dialog
	bind:this={dialog}
	{onclose}
	onclick={(e) => e.target === dialog && dialog?.close()}
	aria-labelledby="settings-title"
	class="sheet m-auto w-[calc(100%-2rem)] max-w-md rounded-xl border bg-paper p-0 text-ink shadow-2xl backdrop:bg-black/60 backdrop:backdrop-blur-sm"
>
	<form onsubmit={save} class="flex flex-col gap-6 p-6">
		<span class="flex items-center gap-3"
			><Avatar email={user.email} size={44} />
			<p id="settings-title" class="text-lg font-semibold">SETTINGS</p></span
		>

		<span class="flex flex-col gap-1">
			<span class="text-gray-500">Email</span>
			<span>{user.email}</span>
		</span>

		<label class="flex flex-col gap-1">
			<span class="text-gray-500">Display name</span>
			<input required maxlength="40" bind:value={nickname} class="border p-3" />
		</label>

		<label class="flex flex-col gap-1">
			<span class="text-gray-500">New password (leave empty to keep)</span>
			<input
				type="password"
				minlength="8"
				autocomplete="new-password"
				placeholder="8+ characters"
				bind:value={password}
				class="border p-3"
			/>
		</label>

		{#if message}
			<p class="text-red-500" role="status">{message}</p>
		{/if}

		<span class="flex justify-end gap-3">
			<button type="button" onclick={() => dialog?.close()} class="p-3 text-gray-600">CANCEL</button
			>
			<button type="submit" disabled={busy} class="btn btn-primary">SAVE</button>
		</span>
	</form>
</dialog>
