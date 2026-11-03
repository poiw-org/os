<script lang="ts">
	import Fa from 'svelte-fa';
	import { faDoorOpen, faDoorClosed } from '@fortawesome/free-solid-svg-icons';
	import { goto } from '$app/navigation';
	import { resolve } from '$app/paths';
	import { forceOpenDoor } from '#lib/api.ts';
	import { toast } from '#lib/notify.svelte.ts';

	const forceUnlock = async () => {
		try {
			await forceOpenDoor();
		} catch (e) {
			toast.error(e instanceof Error ? e.message : 'Could not send the force unlock command.');
			return;
		}

		toast.success('Force unlock command sent.');
		await goto(resolve('/'));
	};

	const cancel = () => goto(resolve('/'));
</script>

<span class="flex flex-col p-4">
	<span>Are you sure you want to send a force unlock command?</span>
	<span class="flex items-center gap-4">
		<button onclick={forceUnlock} class="btn mt-10"><Fa icon={faDoorOpen} />YES</button>
		<button onclick={cancel} class="btn tone-red mt-10"><Fa icon={faDoorClosed} />CANCEL</button>
	</span>
</span>
