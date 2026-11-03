<script lang="ts">
	import { resolve } from '$app/paths';
	import type { Snippet } from 'svelte';
	import Brand from '#lib/ui/Brand.svelte';

	let { title, subtitle, children }: { title: string; subtitle?: string; children: Snippet } =
		$props();
</script>

<div class="flex min-h-screen flex-col px-6 py-8 sm:px-12">
	<a href={resolve('/')} class="flex items-center gap-2.5 self-start">
		<Brand size={28} />
		<span class="font-mono font-bold">doora</span>
		<span class="text-gray-400">backoffice</span>
	</a>

	<main class="flex w-full max-w-sm flex-1 flex-col gap-6 pt-[12vh] pb-12 sm:ml-[8vw]">
		<header class="flex flex-col gap-2">
			<h1 class="text-xl font-bold">{title}</h1>
			{#if subtitle}<p class="text-gray-400">{subtitle}</p>{/if}
		</header>
		{@render children()}
	</main>

	<p class="text-xs text-gray-400">
		Open source by po/iw ·
		<a href="https://github.com/poiw-org" target="_blank" rel="noreferrer" class="hover:text-ink"
			>GitHub</a
		>
	</p>
</div>
