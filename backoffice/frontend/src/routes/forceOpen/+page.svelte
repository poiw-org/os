<script lang="ts">
	import Fa from 'svelte-fa';
	import { faDoorOpen, faDoorClosed } from '@fortawesome/free-solid-svg-icons';
	import { goto } from '$app/navigation';
	import { resolve } from '$app/paths';
	import { forceOpenDoor } from '#lib/api.ts';
	import { toast } from '#lib/notify.svelte.ts';

	let sending = $state(false);

	const forceUnlock = async () => {
		sending = true;
		try {
			await forceOpenDoor();
		} catch (e) {
			toast.error(e instanceof Error ? e.message : 'Could not send the force unlock command.');
			sending = false;
			return;
		}

		toast.success('The door unlocked.');
		await goto(resolve('/'));
	};

	const cancel = () => goto(resolve('/'));
</script>

<span class="flex flex-col p-4">
	<span>Are you sure you want to send a force unlock command?</span>
	<span class="flex items-center gap-4">
		<button onclick={forceUnlock} disabled={sending} class="btn mt-10"
			><Fa icon={faDoorOpen} />{sending ? 'WAITING FOR THE DOOR…' : 'YES'}</button
		>
		<button onclick={cancel} disabled={sending} class="btn tone-red mt-10"
			><Fa icon={faDoorClosed} />CANCEL</button
		>
	</span>
</span>
