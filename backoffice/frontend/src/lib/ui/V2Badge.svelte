<script lang="ts">
	let { large = false }: { large?: boolean } = $props();
</script>

<span
	title="RSA-signed (RS256) key. The door verifies it with the public key it fetches from the backoffice."
	class="inline-flex items-center rounded-full border border-blue-500/40 bg-blue-500/10 {large
		? 'px-3 py-1 text-xs'
		: 'px-2 py-0.5 text-[11px]'} font-mono font-medium tracking-wide text-blue-500">v2</span
>
