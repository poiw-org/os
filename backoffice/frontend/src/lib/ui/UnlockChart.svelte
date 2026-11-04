<script lang="ts">
	import type { Entry } from '#lib/types.ts';
	import Avatar from '#lib/ui/Avatar.svelte';

	// Successful door opens per local day, the newest day on the right. Each bar is topped by the
	// people who came in that day. The readout above shows the whole window until a day is picked
	// (hover with a mouse, tap on touch, arrows on the keyboard), then that day.
	let {
		unlocks,
		personOf,
		days,
		now
	}: {
		unlocks: Entry[];
		/** Email of whoever opened the door, when it is still known. */
		personOf: (entry: Entry) => string | undefined;
		days: number;
		now: number;
	} = $props();

	const MAX_BAR = 128;
	const STACKED = 3;
	const LISTED = 6;

	type People = [email: string, count: number][];

	const dayKey = (date: Date) => `${date.getFullYear()}-${date.getMonth()}-${date.getDate()}`;
	const byCount = (people: Map<string, number>): People =>
		[...people].sort((a, b) => b[1] - a[1] || a[0].localeCompare(b[0]));

	const columns = $derived.by(() => {
		const clock = new Date(now);
		const day = (offset: number) =>
			new Date(clock.getFullYear(), clock.getMonth(), clock.getDate() - offset);
		const list = Array.from({ length: days }, (_, i) => ({
			date: day(days - 1 - i),
			count: 0,
			people: new Map<string, number>()
		}));
		const byKey = new Map(list.map((column) => [dayKey(column.date), column]));
		for (const entry of unlocks) {
			const column = byKey.get(dayKey(entry.timestamp));
			if (!column) continue;
			column.count++;
			const person = personOf(entry);
			if (person) column.people.set(person, (column.people.get(person) ?? 0) + 1);
		}
		return list.map((column, i) => ({
			...column,
			people: byCount(column.people),
			today: i === days - 1
		}));
	});

	const total = $derived(columns.reduce((sum, column) => sum + column.count, 0));
	const peak = $derived(Math.max(1, ...columns.map((column) => column.count)));
	const average = $derived(total / days);
	const regulars = $derived.by(() => {
		// Built and read inside this derivation only, never reactively.
		// eslint-disable-next-line svelte/prefer-svelte-reactivity
		const people = new Map<string, number>();
		for (const column of columns)
			for (const [email, count] of column.people)
				people.set(email, (people.get(email) ?? 0) + count);
		return byCount(people);
	});

	let selected = $state<number | null>(null);
	const focused = $derived(selected === null ? undefined : columns[selected]);

	const longDate = (date: Date) =>
		date.toLocaleDateString('en-GB', { weekday: 'short', day: 'numeric', month: 'short' });
	const month = (date: Date) => date.toLocaleDateString('en-GB', { month: 'short' });
	const plural = (count: number) => (count === 1 ? 'unlock' : 'unlocks');
	const barHeight = (count: number) => (count ? Math.max(4, (count / peak) * MAX_BAR) : 2);

	const readout = $derived(
		focused
			? {
					label: focused.today ? 'Today' : longDate(focused.date),
					count: focused.count,
					note: '',
					people: focused.people
				}
			: {
					label: `Last ${days} days`,
					count: total,
					note: `${average.toFixed(1)} a day`,
					people: regulars
				}
	);

	let plot = $state<HTMLElement>();
	const indexAt = (clientX: number) => {
		const box = plot!.getBoundingClientRect();
		return Math.min(days - 1, Math.max(0, Math.floor(((clientX - box.left) / box.width) * days)));
	};
	// A mouse scrubs as it moves; a tap picks a day and a second tap on it lets go.
	const press = (event: PointerEvent) => {
		const index = indexAt(event.clientX);
		selected = event.pointerType !== 'mouse' && selected === index ? null : index;
	};
	const keys = (event: KeyboardEvent) => {
		const current = selected ?? days - 1;
		const next = {
			ArrowLeft: Math.max(0, current - 1),
			ArrowRight: Math.min(days - 1, current + 1),
			Home: 0,
			End: days - 1
		}[event.key];
		if (event.key === 'Escape') selected = null;
		else if (next !== undefined) selected = next;
		else return;
		event.preventDefault();
	};

	// On a narrow screen the chart scrolls sideways; start on today.
	const startAtToday = (node: HTMLElement) => {
		node.scrollLeft = node.scrollWidth;
	};
</script>

<section class="flex flex-col gap-6 border-b p-4 md:p-8" aria-labelledby="unlocks-heading">
	<h2 id="unlocks-heading" class="sr-only">Unlocks</h2>
	<div class="flex flex-wrap items-end justify-between gap-x-10 gap-y-4">
		<div class="flex flex-col gap-2">
			<span class="eyebrow">{readout.label}</span>
			<span class="flex items-baseline gap-2.5">
				<span class="text-5xl leading-none font-bold tracking-tight">{readout.count}</span>
				<span class="text-sm text-gray-400">{plural(readout.count)}</span>
				{#if readout.note}<span class="text-sm text-gray-400">· {readout.note}</span>{/if}
			</span>
		</div>
		<ul class="flex min-h-6 flex-wrap items-center gap-x-5 gap-y-2 text-sm">
			{#each readout.people.slice(0, LISTED) as [email, count] (email)}
				<li class="flex items-center gap-2" title={email}>
					<Avatar {email} size={22} />
					<span class="max-w-36 truncate">{email.split('@')[0]}</span>
					<span class="text-xs text-gray-400">{count}</span>
				</li>
			{/each}
			{#if readout.people.length > LISTED}
				<li class="text-xs text-gray-400">+{readout.people.length - LISTED} more</li>
			{/if}
		</ul>
	</div>

	<div class="-mx-4 overflow-x-auto px-4 md:mx-0 md:px-0" {@attach startAtToday}>
		<div class="min-w-[780px] md:min-w-0">
			<div
				bind:this={plot}
				role="slider"
				tabindex="0"
				aria-label="Unlocks per day"
				aria-valuemin={1}
				aria-valuemax={days}
				aria-valuenow={(selected ?? days - 1) + 1}
				aria-valuetext="{longDate((focused ?? columns[days - 1]).date)}: {(
					focused ?? columns[days - 1]
				).count}
				{plural((focused ?? columns[days - 1]).count)}"
				class="relative flex h-52 cursor-default items-end gap-1 border-b pr-9 outline-none select-none focus-visible:ring-2 focus-visible:ring-brand-text focus-visible:ring-offset-4 focus-visible:ring-offset-paper"
				onpointerdown={press}
				onpointermove={(event) =>
					event.pointerType === 'mouse' && (selected = indexAt(event.clientX))}
				onpointerleave={(event) => event.pointerType === 'mouse' && (selected = null)}
				onkeydown={keys}
				onblur={() => (selected = null)}
			>
				{#if total}
					<div
						class="pointer-events-none absolute inset-x-0 border-t border-dashed border-gray-300 transition-[bottom] duration-400 ease-(--ease-out)"
						style="bottom:{(average / peak) * MAX_BAR}px"
						aria-hidden="true"
					>
						<span class="absolute right-0 -translate-y-1/2 text-[10px] leading-4 text-gray-400"
							>avg</span
						>
					</div>
				{/if}
				{#each columns as column, i (column.date.getTime())}
					{@const hidden = column.people.length - STACKED}
					<div
						class="relative flex h-full min-w-0 flex-1 flex-col items-center justify-end gap-1.5 transition-opacity duration-150 {selected !==
							null && selected !== i
							? 'opacity-30'
							: ''}"
						aria-hidden="true"
					>
						{#if hidden > 0}
							<span class="text-[10px] leading-3 text-gray-400">+{hidden}</span>
						{/if}
						<!-- Reversed so the most frequent visitor sits right on the bar. -->
						<span class="flex flex-col-reverse items-center">
							{#each column.people.slice(0, STACKED) as [email] (email)}
								<span class="-mt-2 rounded-full ring-2 ring-paper last:mt-0">
									<Avatar {email} size={22} />
								</span>
							{/each}
						</span>
						<span
							class="w-full max-w-8 rounded-t-sm transition-[height] duration-400 ease-(--ease-out) {column.count
								? 'bg-brand'
								: 'bg-gray-200'}"
							style="height:{barHeight(column.count)}px"
						></span>
					</div>
				{/each}
			</div>
			<div class="flex gap-1 pt-2 pr-9" aria-hidden="true">
				{#each columns as column, i (column.date.getTime())}
					<span
						class="flex min-w-0 flex-1 flex-col items-center text-[10px] leading-4 {column.today ||
						selected === i
							? 'font-bold text-ink'
							: 'text-gray-400'}"
					>
						{column.date.getDate()}
						{#if i === 0 || column.date.getDate() === 1}
							<span class="font-normal text-gray-400">{month(column.date)}</span>
						{/if}
					</span>
				{/each}
			</div>
		</div>
	</div>
</section>
