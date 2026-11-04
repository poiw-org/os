<script lang="ts">
	import { fly } from 'svelte/transition';
	import type { UserOption } from '#lib/types.ts';
	import Avatar from '#lib/ui/Avatar.svelte';

	let { users, value = $bindable() }: { users: UserOption[]; value?: UserOption } = $props();

	const id = $props.id();
	const MAX = 8;

	let query = $state('');
	let open = $state(false);
	let active = $state(0);
	let input = $state<HTMLInputElement>();

	const needle = $derived(query.trim().toLowerCase());
	const found = $derived(users.filter((user) => user.label.toLowerCase().includes(needle)));
	const matches = $derived(found.slice(0, MAX));

	const choose = (user: UserOption) => {
		value = user;
		query = '';
		open = false;
	};

	const change = async () => {
		value = undefined;
		open = true;
		await Promise.resolve();
		input?.focus();
	};

	const onkeydown = (event: KeyboardEvent) => {
		if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
			event.preventDefault();
			open = true;
			const step = event.key === 'ArrowDown' ? 1 : -1;
			active = (active + step + matches.length) % Math.max(1, matches.length);
		} else if (event.key === 'Enter' && open && matches[active]) {
			// Enter picks the highlighted user instead of submitting the form.
			event.preventDefault();
			choose(matches[active]);
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
	{#if value}
		<div
			in:fly={{ y: 6, duration: 180 }}
			class="flex items-center justify-between gap-3 rounded-lg border bg-paper-2 py-2 pr-2 pl-3"
		>
			<span class="flex min-w-0 items-center gap-3">
				<Avatar email={value.email} size={36} />
				<span class="truncate">{value.label}</span>
			</span>
			<button type="button" onclick={change} class="btn">Change</button>
		</div>
	{:else}
		<div
			class="flex items-center gap-3 rounded-lg border bg-paper-2 py-1.5 pl-3.5 text-gray-400 transition-colors focus-within:border-gray-400"
		>
			<input
				bind:this={input}
				bind:value={query}
				type="text"
				role="combobox"
				aria-label="User"
				aria-expanded={open}
				aria-controls="{id}-list"
				aria-activedescendant={open && matches[active] ? `${id}-${active}` : undefined}
				aria-autocomplete="list"
				autocomplete="off"
				placeholder="Search users by email"
				onfocus={() => (open = true)}
				onblur={() => (open = false)}
				oninput={() => {
					open = true;
					active = 0;
				}}
				{onkeydown}
				class="w-full border-0! bg-transparent! p-1.5 text-ink shadow-none! outline-none!"
			/>
		</div>

		{#if open}
			<ul
				id="{id}-list"
				role="listbox"
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
						aria-selected={i === active}
						onmousedown={(e) => e.preventDefault()}
						onclick={() => choose(user)}
						onkeydown={() => {}}
						onmouseenter={() => (active = i)}
						class="row cursor-pointer justify-start {i === active ? 'bg-brand/20' : ''}"
					>
						<Avatar email={user.email} size={28} />
						<span class="truncate"
							>{text.before}{#if text.hit}<b class="font-semibold text-ink">{text.hit}</b
								>{/if}{text.after}</span
						>
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
	{/if}
</div>
