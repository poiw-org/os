<script lang="ts">
	import { resolve } from '$app/paths';

	// Wayfinding for the users -> keys -> entries hierarchy; the last crumb is the current page.
	let { crumbs }: { crumbs: { label: string; href?: '/' | `/users?${string}` }[] } = $props();
</script>

<nav aria-label="Breadcrumb" class="flex flex-wrap items-center gap-2 text-sm tracking-wider">
	{#each crumbs as crumb, i (i)}
		{#if i > 0}<span class="text-gray-300" aria-hidden="true">/</span>{/if}
		{#if crumb.href}
			<a href={resolve(crumb.href)} class="text-gray-600 underline-offset-4 hover:underline"
				>{crumb.label}</a
			>
		{:else}
			<span class="font-bold break-all" aria-current="page">{crumb.label}</span>
		{/if}
	{/each}
</nav>
