<script lang="ts">
	import { resolve } from '$app/paths';
	import type { Snippet } from 'svelte';
	import AuthArt from './AuthArt.svelte';

	let { title, subtitle, children }: { title: string; subtitle?: string; children: Snippet } =
		$props();
</script>

<div class="grid min-h-screen bg-void lg:grid-cols-[minmax(440px,1fr)_1.15fr]">
	<section class="flex flex-col px-6 py-8 sm:px-12">
		<a href={resolve('/')} class="flex items-center gap-2.5 self-start">
			<span
				class="flex h-6 w-6 items-center justify-center rounded-md bg-ink font-mono text-[13px] leading-none font-bold text-void"
				aria-hidden="true">d</span
			>
			<span class="text-sm font-medium">doora</span>
			<span class="text-gray-300" aria-hidden="true">/</span>
			<span class="text-sm text-gray-400">backoffice</span>
		</a>

		<div class="mx-auto flex w-full max-w-sm flex-1 flex-col justify-center gap-8 py-12">
			<header class="flex flex-col gap-2">
				<h1 class="text-2xl font-semibold tracking-tight">{title}</h1>
				{#if subtitle}<p class="text-gray-400">{subtitle}</p>{/if}
			</header>
			{@render children()}
		</div>

		<p class="font-mono text-xs text-gray-400">
			Open source by po/iw ·
			<a href="https://github.com/poiw-org" target="_blank" rel="noreferrer" class="hover:text-ink"
				>GitHub</a
			>
		</p>
	</section>

	<aside class="sticky top-0 hidden h-screen border-l lg:block"><AuthArt /></aside>
</div>
