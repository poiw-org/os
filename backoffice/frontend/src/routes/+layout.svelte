<script lang="ts">
	import '../app.css';
	import { page } from '$app/state';
	import Header from '#lib/header/Header.svelte';
	import Notifications from '#lib/ui/Notifications.svelte';
	import type { LayoutProps } from './$types';

	let { children, data }: LayoutProps = $props();

	// The auth screens bring their own full-bleed layout.
	const bare = $derived(['/login', '/reset-password'].includes(page.url.pathname));
</script>

{#if bare}
	{@render children()}
{:else}
	<div class="flex min-h-screen w-full flex-col bg-paper/90">
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
