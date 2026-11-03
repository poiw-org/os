<script lang="ts">
	import { DateInput } from 'date-picker-svelte';

	// An optional, inclusive day range. Either end may stay open.
	let { from = $bindable(null), to = $bindable(null) }: { from?: Date | null; to?: Date | null } =
		$props();

	const day = (offset = 0) => {
		const date = new Date();
		date.setHours(0, 0, 0, 0);
		date.setDate(date.getDate() + offset);
		return date;
	};

	const presets = [
		{ label: 'Today', days: 0 },
		{ label: '7 days', days: 6 },
		{ label: '30 days', days: 29 }
	];

	const isPreset = (days: number) =>
		from?.getTime() === day(-days).getTime() && to?.getTime() === day().getTime();
</script>

<div class="flex flex-col gap-2">
	<div class="flex flex-wrap items-center gap-2">
		<span class="eyebrow mr-1">Date</span>
		{#each presets as preset (preset.days)}
			<button
				type="button"
				class="btn"
				aria-pressed={isPreset(preset.days)}
				class:btn-primary={isPreset(preset.days)}
				onclick={() => {
					from = day(-preset.days);
					to = day();
				}}>{preset.label}</button
			>
		{/each}
	</div>
	<div class="grid grid-cols-2 gap-2">
		<label class="flex min-w-0 flex-col gap-1 text-xs text-gray-400">
			From
			<DateInput
				format="yyyy-MM-dd"
				placeholder="Any time"
				closeOnSelection
				min={new Date('2000')}
				max={to ?? new Date('2100')}
				bind:value={from}
			/>
		</label>
		<label class="flex min-w-0 flex-col gap-1 text-xs text-gray-400">
			To
			<DateInput
				format="yyyy-MM-dd"
				placeholder="Any time"
				closeOnSelection
				min={from ?? new Date('2000')}
				max={new Date('2100')}
				bind:value={to}
			/>
		</label>
	</div>
</div>
