<script lang="ts">
	import { fly } from 'svelte/transition';
	import type { UserOption } from '#lib/types.ts';
	import Avatar from '#lib/ui/Avatar.svelte';

	// Pick any number of users: chips sit inside the field, the list stays open while choosing.
	let { users, selected = $bindable([]) }: { users: UserOption[]; selected?: UserOption[] } =
		$props();

	const id = $props.id();
	const MAX = 8;

	let query = $state('');
	let open = $state(false);
	let active = $state(0);
	let input = $state<HTMLInputElement>();

	const needle = $derived(query.trim().toLowerCase());
	const found = $derived(users.filter((user) => user.label.toLowerCase().includes(needle)));
	const matches = $derived(found.slice(0, MAX));
	const isSelected = (user: UserOption) => selected.some((chosen) => chosen.value === user.value);

	const toggle = (user: UserOption) => {
		selected = isSelected(user)
			? selected.filter((chosen) => chosen.value !== user.value)
			: [...selected, user];
		query = '';
		input?.focus();
	};

	const onkeydown = (event: KeyboardEvent) => {
		if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
			event.preventDefault();
			open = true;
			const step = event.key === 'ArrowDown' ? 1 : -1;
			active = (active + step + matches.length) % Math.max(1, matches.length);
		} else if (event.key === 'Enter' && open && matches[active]) {
			event.preventDefault();
			toggle(matches[active]);
		} else if (event.key === 'Backspace' && !query && selected.length) {
			selected = selected.slice(0, -1);
		} else if (event.key === 'Escape' && open) {
			event.stopPropagation();
			open = false;
		}
	};

	const parts = (label: string) => {
		const at = needle ? label.toLowerCase().indexOf(needle) : -1;
		return at < 0
			? { before: label, hit: '', after: '' }
			: {
					before: label.slice(0, at),
					hit: label.slice(at, at + needle.length),
					after: label.slice(at + needle.length)
				};
	};
</script>

<div class="relative">
	<!-- svelte-ignore a11y_click_events_have_key_events, a11y_no_static_element_interactions -->
	<div
		onclick={() => input?.focus()}
		class="flex min-h-11 cursor-text flex-wrap items-center gap-1.5 rounded-lg border bg-paper-2 py-1.5 pr-2 pl-2 text-gray-400 transition-colors focus-within:border-brand-text"
	>
		{#each selected as user (user.value)}
			<span
				in:fly={{ y: 4, duration: 140 }}
				class="flex items-center gap-1.5 rounded-full border bg-paper py-0.5 pr-0.5 pl-1 text-xs text-ink"
			>
				<Avatar email={user.label} size={18} />
				<span class="max-w-40 truncate">{user.label}</span>
				<button
					type="button"
					aria-label="Remove {user.label}"
					onclick={(e) => {
						e.stopPropagation();
						toggle(user);
					}}
					class="flex h-5 w-5 items-center justify-center rounded-full text-gray-400 hover:bg-gray-200 hover:text-ink"
					>×</button
				>
			</span>
		{/each}
		<input
			bind:this={input}
			bind:value={query}
			type="text"
			role="combobox"
			aria-label="Filter by users"
			aria-expanded={open}
			aria-controls="{id}-list"
			aria-activedescendant={open && matches[active] ? `${id}-${active}` : undefined}
			aria-autocomplete="list"
			autocomplete="off"
			placeholder={selected.length ? '' : 'All users'}
			onfocus={() => (open = true)}
			onblur={() => (open = false)}
			oninput={() => {
				open = true;
				active = 0;
			}}
			{onkeydown}
			class="min-w-24 flex-1 border-0! bg-transparent! p-1 text-ink shadow-none! outline-none!"
		/>
	</div>

	{#if open}
		<ul
			id="{id}-list"
			role="listbox"
			aria-multiselectable="true"
			aria-label="Users"
			transition:fly={{ y: -6, duration: 150 }}
			class="panel absolute inset-x-0 top-full z-20 mt-2 max-h-80 overflow-auto"
		>
			{#each matches as user, i (user.value)}
				{@const text = parts(user.label)}
				<!-- mousedown keeps the input focused so the click lands before blur closes the list -->
				<li
					id="{id}-{i}"
					role="option"
					aria-selected={isSelected(user)}
					onmousedown={(e) => e.preventDefault()}
					onclick={() => toggle(user)}
					onkeydown={() => {}}
					onmouseenter={() => (active = i)}
					class="row cursor-pointer justify-between {i === active ? 'bg-brand/20' : ''}"
				>
					<span class="flex min-w-0 items-center gap-3">
						<Avatar email={user.label} size={28} />
						<span class="truncate"
							>{text.before}{#if text.hit}<b class="font-semibold text-ink">{text.hit}</b
								>{/if}{text.after}</span
						>
					</span>
					{#if isSelected(user)}<span class="shrink-0 text-brand-text" aria-hidden="true">✓</span
						>{/if}
				</li>
			{:else}
				<li class="row text-gray-400">No users match “{query.trim()}”.</li>
			{/each}
			{#if found.length > MAX}
				<li class="row text-xs text-gray-400">
					Showing {MAX} of {found.length}. Keep typing to narrow it down.
				</li>
			{/if}
		</ul>
	{/if}
</div>
