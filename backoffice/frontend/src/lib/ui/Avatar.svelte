<script module lang="ts">
	// Gravatar identifies users by the SHA-256 of their lowercased email. Hash once per address.
	// A plain cache, never read reactively.
	// eslint-disable-next-line svelte/prefer-svelte-reactivity
	const hashes = new Map<string, Promise<string | undefined>>();

	function gravatarHash(email: string) {
		let hash = hashes.get(email);
		if (!hash) {
			// crypto.subtle only exists in secure contexts (https, localhost); without it we fall back.
			hash = globalThis.crypto?.subtle
				? crypto.subtle
						.digest('SHA-256', new TextEncoder().encode(email))
						.then((bytes) =>
							Array.from(new Uint8Array(bytes), (b) => b.toString(16).padStart(2, '0')).join('')
						)
				: Promise.resolve(undefined);
			hashes.set(email, hash);
		}
		return hash;
	}

	// Cheap stable string hash for the generated avatar's colours.
	const seed = (text: string) => [...text].reduce((h, c) => (h * 31 + c.charCodeAt(0)) >>> 0, 7);
</script>

<script lang="ts">
	let { email, size = 32 }: { email: string; size?: number } = $props();

	const address = $derived(email.trim().toLowerCase());
	const hue = $derived(seed(address) % 360);
	const initial = $derived(address.charAt(0).toUpperCase() || '?');

	let src = $state<string>();
	let loaded = $state(false);

	// d=404 makes Gravatar answer 404 for unknown addresses, which keeps the generated avatar.
	$effect(() => {
		loaded = false;
		src = undefined;
		let current = true;
		gravatarHash(address).then((hash) => {
			if (current && hash) src = `https://gravatar.com/avatar/${hash}?s=${size * 2}&d=404`;
		});
		return () => (current = false);
	});
</script>

<span
	class="relative inline-flex shrink-0 items-center justify-center overflow-hidden rounded-full font-medium text-white/90 select-none"
	style="width:{size}px;height:{size}px;font-size:{Math.round(
		size * 0.4
	)}px;background:linear-gradient(135deg, oklch(62% 0.12 {hue}), oklch(38% 0.1 {(hue + 50) %
		360}));box-shadow:inset 0 0 0 1px oklch(100% 0 0 / 0.12)"
	aria-hidden="true"
>
	{initial}
	{#if src}
		<img
			{src}
			alt=""
			loading="lazy"
			class="absolute inset-0 h-full w-full object-cover transition-opacity duration-200 {loaded
				? 'opacity-100'
				: 'opacity-0'}"
			onload={() => (loaded = true)}
			onerror={() => (src = undefined)}
		/>
	{/if}
</span>
