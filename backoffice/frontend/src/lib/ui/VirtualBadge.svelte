<script lang="ts">
	let { large = false }: { large?: boolean } = $props();
</script>

<span
	title="A virtual key has no tag. It unlocks the door remotely, from the backoffice."
	class="inline-flex items-center rounded-full border border-purple-500/40 bg-purple-500/10 {large
		? 'px-3 py-1 text-xs'
		: 'px-2 py-0.5 text-[11px]'} font-medium tracking-wide text-purple-500">VIRTUAL</span
>
