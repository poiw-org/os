<script lang="ts">
	import { onMount } from 'svelte';

	// Reads "Operating System", then folds into "OS" once the reader has had a moment with it.
	let short = $state(false);

	onMount(() => {
		const t = setTimeout(() => (short = true), 3000);
		return () => clearTimeout(t);
	});
</script>

<span class="os" class:short aria-label="Operating System">
	<span aria-hidden="true"
		>O<span class="fold">perating&nbsp;</span>S<span class="fold">ystem</span></span
	>
</span>

<style>
	.os {
		display: inline-block;
		white-space: nowrap;
		transition: letter-spacing 700ms var(--ease-out);
	}
	.short {
		letter-spacing: 0.08em;
	}
	.fold {
		display: inline-block;
		vertical-align: bottom;
		overflow: hidden;
		max-width: 9em;
		opacity: 1;
		transition:
			max-width 700ms var(--ease-out),
			opacity 400ms ease-out;
	}
	.short .fold {
		max-width: 0;
		opacity: 0;
	}
	@media (prefers-reduced-motion: reduce) {
		.fold,
		.os {
			transition: none;
		}
	}
</style>
