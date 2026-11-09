<script lang="ts">
	import type { Snippet } from 'svelte';

	let {
		trigger,
		label,
		children,
		inline = false,
		align = 'left'
	}: {
		/** Classes for the button that opens the menu. */
		trigger: string;
		label: Snippet;
		/** The menu's content; it receives a function that closes the menu. */
		children: Snippet<[() => void]>;
		/** The list pushes the page down instead of floating over it (phone menu). */
		inline?: boolean;
		/** `bar` hangs from the right on wide screens and spans the top bar on phones. */
		align?: 'left' | 'right' | 'bar';
	} = $props();

	let open = $state(false);
	let root = $state<HTMLElement>();
	let button = $state<HTMLButtonElement>();

	// Focus goes back first, so a dialog opened from the menu returns it here when it closes.
	const close = () => {
		open = false;
		button?.focus();
	};
</script>

<svelte:window
	onpointerdown={(event) => open && !root?.contains(event.target as Node) && (open = false)}
	onkeydown={(event) => event.key === 'Escape' && (open = false)}
/>

<span
	bind:this={root}
	class={inline ? 'contents' : 'relative inline-flex'}
	class:bar-anchor={align === 'bar'}
>
	<button
		bind:this={button}
		type="button"
		class="{trigger} {inline ? 'w-full' : ''}"
		aria-expanded={open}
		onclick={() => (open = !open)}
	>
		{@render label()}
		<svg
			viewBox="0 0 10 6"
			class="chevron"
			class:open
			width="10"
			height="6"
			fill="none"
			stroke="currentColor"
			stroke-width="1.5"
			aria-hidden="true"><path d="M1 1l4 4 4-4" /></svg
		>
	</button>
	{#if open}
		<div
			class="drop"
			class:stacked={inline}
			class:floating={!inline}
			class:end={align === 'right' || align === 'bar'}
			class:across={align === 'bar'}
		>
			{@render children(close)}
		</div>
	{/if}
</span>

<style>
	.chevron {
		margin-left: 0.5rem;
		transition: transform var(--dur-base) var(--ease-out);
	}
	.chevron.open {
		transform: rotate(180deg);
	}
	.drop {
		z-index: 30;
		text-align: left;
		background: var(--c-cell);
		color: var(--c-text);
		border: 1px solid var(--c-seam);
		box-shadow:
			inset 1px 1px 0 var(--c-edge),
			0 8px 24px oklch(0% 0 0 / 0.45);
		animation: menu-in var(--dur-base) var(--ease-out);
	}
	.floating {
		position: absolute;
		top: calc(100% + 0.25rem);
		left: 0;
		width: 20rem;
		max-width: calc(100vw - 2rem);
		transform-origin: top left;
	}
	.floating.end {
		left: auto;
		right: 0;
		transform-origin: top right;
	}
	.stacked {
		grid-column: 1 / -1;
	}
	/* The trigger sits mid-bar on a phone, so the menu measures from the bar instead. */
	@media (max-width: 767px) {
		.bar-anchor {
			position: static;
		}
		.floating.across {
			left: 1rem;
			right: 1rem;
			width: auto;
			max-width: none;
			transform-origin: top center;
		}
	}
	@keyframes menu-in {
		from {
			opacity: 0;
			transform: translateY(-4px) scale(0.97);
		}
	}
	@media (prefers-reduced-motion: reduce) {
		.drop {
			animation: none;
		}
		.chevron {
			transition: none;
		}
	}
</style>
