<script lang="ts">
	import type { Snippet } from 'svelte';
	import OsLabel from '#lib/ui/OsLabel.svelte';
	import { swapIn, swapOut } from '#lib/motion.ts';
	import Dither from './Dither.svelte';
	import mascot from './mascot.txt?raw';

	let { title, subtitle, children }: { title: string; subtitle?: string; children: Snippet } =
		$props();
</script>

<div class="console">
	<div class="fixed inset-0">
		<Dither
			waveColor="var(--color-brand-text)"
			backgroundColor="var(--color-backing)"
			ditherMode="bayer"
			colorNum={4}
			pixelSize={3}
			waveSpeed={0.04}
			waveFrequency={2.4}
			waveAmplitude={0.35}
			brightness={-0.12}
			mouseRadius={0.35}
			art={mascot}		/>
	</div>
	<header class="bar full fixed inset-x-0 top-0 z-10">
		<a href="/" class="label"><OsLabel /></a>
		<div class="grow"></div>
	</header>
	<div class="relative flex min-h-screen flex-col px-6 pt-20 pb-8 sm:px-12 lg:w-1/2">
		<main class="flex flex-1 items-center justify-center py-10 lg:justify-start lg:pl-[4vw]">
			<div class="panel rise w-full max-w-sm p-8">
				<div class="stagger flex flex-col gap-6" style="--d: 80ms">
					<header class="grid">
						{#key title}
							<div class="flex flex-col gap-3 [grid-area:1/1]" in:swapIn out:swapOut>
								<h1 class="text-xl font-bold">{title}</h1>
								{#if subtitle}<p class="text-gray-400">{subtitle}</p>{/if}
							</div>
						{/key}
					</header>
					{@render children()}
				</div>
			</div>
		</main>

		<p class="self-start bg-backing px-3 py-2 text-xs text-gray-400">
			Open source by po/iw ·
			<a href="https://github.com/poiw-org" target="_blank" rel="noreferrer" class="hover:text-ink"
				>GitHub</a
			>
		</p>
	</div>
</div>
