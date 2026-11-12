<script lang="ts">
	import { onMount } from 'svelte';
	import { goto } from '$app/navigation';
	import { resolve } from '$app/paths';

	// A phone tapped a door's reader, which reads as a tag holding this page's URL. Where the app's
	// own tag handling did not open it (no app, or iOS), hand over to the app with its unlock link;
	// if nothing takes it, this is the website, so go to the home page.
	onMount(() => {
		location.href = 'doora://unlock';
		const fallback = setTimeout(() => {
			if (document.visibilityState === 'visible') goto(resolve('/'), { replaceState: true });
		}, 1500);
		return () => clearTimeout(fallback);
	});
</script>

<svelte:head><title>Doora</title></svelte:head>

<p>Opening Doora…</p>
