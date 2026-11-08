<script lang="ts">
	import { onMount } from 'svelte';
	import { beginTotp, confirmTotp } from '#lib/auth.ts';

	let { ondone }: { ondone: () => void } = $props();

	let enrollment = $state<Awaited<ReturnType<typeof beginTotp>>>();
	let code = $state('');
	let error = $state('');
	let busy = $state(false);

	const failure = (e: unknown) => (e instanceof Error ? e.message : 'Something went wrong.');

	onMount(() => {
		beginTotp().then(
			(started) => (enrollment = started),
			(e) => (error = failure(e))
		);
	});

	const submit = async (event: SubmitEvent) => {
		event.preventDefault();
		if (!enrollment) return;
		busy = true;
		error = '';
		try {
			await confirmTotp(enrollment.factorId, code.trim());
			ondone();
		} catch (e) {
			error = failure(e);
			code = '';
		}
		busy = false;
	};
</script>

{#if enrollment}
	<form onsubmit={submit} class="flex flex-col gap-3">
		<img src={enrollment.qrCode} alt="QR code" width="176" height="176" class="bg-white p-2" />
		<p class="text-xs break-all text-gray-400">
			Scan it, or enter this secret: <code class="select-all">{enrollment.secret}</code>
		</p>
		<span class="flex gap-2">
			<input
				required
				inputmode="numeric"
				autocomplete="one-time-code"
				minlength="6"
				maxlength="6"
				aria-label="Six-digit code"
				placeholder="123456"
				bind:value={code}
				class="slot min-w-0 flex-1 px-3.5 py-2.5 font-mono"
			/>
			<button type="submit" disabled={busy} class="btn btn-primary px-4">Verify</button>
		</span>
	</form>
{/if}
{#if error}<p role="alert" class="text-red-400">{error}</p>{/if}
