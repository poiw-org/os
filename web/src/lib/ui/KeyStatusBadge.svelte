<script lang="ts">
	import { statusLabel } from '#lib/status.ts';
	import type { KeyStatus } from '#lib/types.ts';

	let { status }: { status: KeyStatus } = $props();

	const dot: Record<KeyStatus, string> = {
		active: 'bg-green-500',
		disabled: 'bg-red-500',
		expired: 'bg-yellow-500',
		pending: 'bg-blue-500'
	};
</script>

<span class="inline-flex items-center gap-2 text-xs text-gray-600">
	<span class="h-1.5 w-1.5 rounded-full {dot[status]}"></span>{statusLabel[status]}
</span>
