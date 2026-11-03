<script lang="ts">
	import '../app.css';
	import Header from '#lib/header/Header.svelte';
	import Notifications from '#lib/ui/Notifications.svelte';
	import type { LayoutProps } from './$types';

	let { children, data }: LayoutProps = $props();
</script>

<div class="flex min-h-screen w-full flex-col bg-paper/90">
	<Header user={data.user} />
	<main class="mx-auto w-full max-w-[1500px] flex-1">
		{@render children()}
	</main>
	<p id="footer" class="border-t p-4 text-center font-mono text-xs text-gray-400 print:hidden">
		Open Source/FOSS project made by po/iw. Find the source code in our
		<a href="https://github.com/poiw-org" target="_blank" rel="noreferrer" class="font-bold"
			>Github page</a
		>.
	</p>
</div>
<Notifications />
