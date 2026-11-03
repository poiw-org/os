<script lang="ts">
	import { statusLabel } from '#lib/status.ts';
	import type { KeyStatus } from '#lib/types.ts';

	let { status, large = false }: { status: KeyStatus; large?: boolean } = $props();

	const dot: Record<KeyStatus, string> = {
		active: 'bg-green-500',
		disabled: 'bg-red-500',
		expired: 'bg-yellow-500',
		pending: 'bg-blue-500'
	};
</script>

<span
	class="inline-flex items-center gap-2 rounded-full border bg-paper-2 {large
		? 'px-3 py-1 text-xs'
		: 'px-2 py-0.5 text-[11px]'} font-medium tracking-wide text-gray-600"
>
	<span class="h-1.5 w-1.5 rounded-full {dot[status]}"></span>{statusLabel[status]}
</span>
