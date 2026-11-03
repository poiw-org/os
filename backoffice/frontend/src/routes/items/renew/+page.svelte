<script lang="ts">
	import { goto } from '$app/navigation';
	import { resolve } from '$app/paths';
	import { page } from '$app/state';
	import { onMount } from 'svelte';
	import Fa from 'svelte-fa';
	import { faHandSparkles } from '@fortawesome/free-solid-svg-icons';
	import { DateInput } from 'date-picker-svelte';
	import { getKey, renewKey } from '#lib/api.ts';
	import { shortId } from '#lib/format.ts';
	import { toast } from '#lib/notify.svelte.ts';
	import Loading from '#lib/loading.svelte';
	import type { Key } from '#lib/types.ts';

	let key = $state.raw<Key>();
	let processing = $state(true);
	let loadError = $state('');

	onMount(async () => {
		const id = page.url.searchParams.get('id');
		try {
			if (id) key = await getKey(id);
			if (!key) loadError = 'Key not found.';
		} catch (e) {
			loadError = e instanceof Error ? e.message : 'Could not load the key.';
		}
		processing = false;
	});

	const submit = async (event: SubmitEvent) => {
		event.preventDefault();
		if (!key) return;
		processing = true;
		try {
			await renewKey(key);
			toast.success('Key renewed.');
			await goto(resolve(`/items?id=${key.id}`));
		} catch (e) {
			toast.error(e instanceof Error ? e.message : 'Could not renew the key.');
			processing = false;
		}
	};
</script>

{#if loadError}
	<span class="flex w-full justify-center px-10 pt-20 text-red-500">{loadError}</span>
{:else if !processing && key}
	<form onsubmit={submit} class="flex flex-col items-start justify-start px-4 py-4 md:px-10">
		<p class="text-xl font-bold">Renew {shortId(key.id)} owned by {key.user.email}</p>
		<span class="mt-10 flex gap-4">
			<span class="flex flex-col gap-2">
				<span>Activate not before</span>
				<DateInput
					format="yyyy-MM-dd"
					min={new Date()}
					max={new Date('2100')}
					bind:value={key.notBefore}
				/>
			</span>
			<span class="flex flex-col gap-2">
				<span>Expires at</span>
				<DateInput
					format="yyyy-MM-dd"
					min={new Date()}
					max={new Date('2100')}
					bind:value={key.expiresAt}
				/>
			</span>
		</span>
		<button type="submit" class="btn btn-primary mt-10"><Fa icon={faHandSparkles} />RENEW</button>
	</form>
{:else}
	<span class="flex pb-20"><Loading /></span>
{/if}
