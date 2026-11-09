<script lang="ts">
	import { page } from '$app/state';
	import { authorizeReleaseCli } from '#lib/api.ts';
	import { toast } from '#lib/notify.svelte.ts';

	// Consent for `make release`. The answer only ever goes to this computer's loopback address: the
	// port is the one thing the link can choose. The function makes the real decision on who may allow it.
	const params = page.url.searchParams;
	const port = Number(params.get('port'));
	const nonce = params.get('state') ?? '';
	const challenge = params.get('challenge') ?? '';
	const valid =
		Number.isInteger(port) &&
		port >= 1024 &&
		port <= 65535 &&
		/^[\w-]{16,128}$/.test(nonce) &&
		/^[\w-]{43}$/.test(challenge);

	const email = page.data.user.email as string;
	// canUnlock is true for admins and for members with an active key.
	const allowed = page.data.user.canUnlock as boolean;

	let busy = $state(false);

	const back = (answer: Record<string, string>) => {
		const query = new URLSearchParams({ ...answer, state: nonce });
		window.location.href = `http://127.0.0.1:${port}/callback?${query}`;
	};

	const allow = async () => {
		busy = true;
		try {
			back({ code: await authorizeReleaseCli(challenge) });
		} catch (e) {
			toast.error(e instanceof Error ? e.message : 'That did not work.');
			busy = false;
		}
	};
</script>

<svelte:head><title>Allow firmware uploads</title></svelte:head>

<div class="m-auto max-w-md rounded-lg border p-6">
	<h1 class="text-lg font-bold">Allow `make release`?</h1>
	{#if !allowed}
		<p class="mt-3 text-sm text-red-600" role="alert">
			Only admins and members with an active key can upload firmware.
		</p>
	{:else if !valid}
		<p class="mt-3 text-sm text-red-600" role="alert">
			This link is not valid. Run `make release` again.
		</p>
	{:else}
		<p class="mt-3 text-sm">
			The release tool on this computer is asking to upload firmware builds as <strong
				>{email}</strong
			>
			for the next 30 days.
		</p>
		<ul class="mt-3 list-disc pl-5 text-sm">
			<li>
				It can only queue builds. An admin still has to approve each one before the door installs
				it.
			</li>
			<li>It cannot open the door, change keys or use the door console.</li>
		</ul>
		<p class="mt-3 text-sm text-gray-500">Only continue if you just ran `make release` yourself.</p>
		<div class="mt-4 flex justify-end gap-2">
			<button type="button" class="key" disabled={busy} onclick={() => back({ error: 'denied' })}
				>Deny</button
			>
			<button type="button" class="key key-signal" disabled={busy} onclick={allow}>Allow</button>
		</div>
	{/if}
</div>
