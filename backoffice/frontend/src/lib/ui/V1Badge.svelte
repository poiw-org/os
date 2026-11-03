<script lang="ts">
	let { large = false }: { large?: boolean } = $props();
</script>

<span
	title="Imported from the legacy system. The card keeps working until it is rewritten with a new token."
	class="inline-flex items-center rounded-full border bg-paper-2 {large
		? 'px-3 py-1 text-xs'
		: 'px-2 py-0.5 text-[11px]'} font-mono font-medium tracking-wide text-gray-500">v1</span
>
