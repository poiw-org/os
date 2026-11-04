<script lang="ts">
	import '../app.css';
	import '#lib/console.css';
	import { page } from '$app/state';
	import { onNavigate } from '$app/navigation';
	import { reduced } from '#lib/motion.ts';
	import Header from '#lib/header/Header.svelte';
	import Notifications from '#lib/ui/Notifications.svelte';
	import type { LayoutProps } from './$types';
	import { onMount } from 'svelte';
	import { startFaviconGalaxy } from '#lib/favicon-galaxy.ts';

	let { children, data }: LayoutProps = $props();

	onMount(startFaviconGalaxy);

	// Leaving a page fades it out while the next one rises in on its own (.rise/.stagger). Same-page
	// navigations (a ?id= change) and reduced motion skip it.
	onNavigate((nav) => {
		if (!document.startViewTransition || reduced()) return;
		if (nav.from?.url.pathname === nav.to?.url.pathname) return;
		return new Promise((resolve) => {
			document.startViewTransition(async () => {
				resolve();
				await nav.complete;
			});
		});
	});

	// The auth screens bring their own full-bleed layout.
	const bare = $derived(
		['/', '/login', '/reset-password', '/onboarding'].includes(page.url.pathname)
	);
</script>

{#if bare}
	{@render children()}
{:else}
	<div class="console flex min-h-screen w-full flex-col">
		<Header user={data.user} />
		<main class="mx-auto w-full max-w-[1500px] flex-1">
			{@render children()}
		</main>
		<footer id="footer" class="border-t text-xs text-gray-400 print:hidden">
			<p class="mx-auto w-full max-w-[1500px] px-4 py-4 md:px-8">
				Open Source/FOSS project made by po/iw. Find the source code in our
				<a href="https://github.com/poiw-org" target="_blank" rel="noreferrer" class="font-bold"
					>Github page</a
				>.
			</p>
		</footer>
	</div>
{/if}
<Notifications />
