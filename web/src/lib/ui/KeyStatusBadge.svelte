<script lang="ts">
	import { statusLabel } from '#lib/status.ts';
	import type { KeyStatus } from '#lib/types.ts';

	let { status }: { status: KeyStatus } = $props();

	// A lamp per state: green lit, amber lit, blue waiting (breathes), red dark.
	const lamp: Record<KeyStatus, string> = {
		active: 'lamp-green',
		expired: 'lamp-amber',
		pending: 'lamp-blue lamp-wait',
		disabled: 'lamp-red lamp-off'
	};
</script>

<span class="readout-chip">
	<span class="lamp {lamp[status]}" aria-hidden="true"></span>{statusLabel[status]}
</span>
