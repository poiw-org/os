<script lang="ts">
	import Cubes from './Cubes.svelte';
</script>

<!-- The decorative half of the auth screens. -->
<div class="art relative flex h-full w-full items-center justify-center overflow-hidden">
	<div class="w-[min(78%,78vh)]">
		<Cubes gridSize={10} faceColor="#120F17" edgeColor="#ffffff" maxAngle={45} radius={3} />
	</div>

	<div class="pointer-events-none absolute inset-x-0 bottom-0 flex flex-col gap-1 p-10">
		<p class="text-lg font-medium tracking-tight text-ink">Access, without the keyring.</p>
		<p class="max-w-sm text-gray-400">
			Issue NFC keys, revoke them in a tap, and watch every door event as it happens.
		</p>
	</div>
</div>

<style>
	.art {
		background: radial-gradient(120% 90% at 50% 40%, oklch(15% 0.012 300), var(--color-void));
	}
</style>
