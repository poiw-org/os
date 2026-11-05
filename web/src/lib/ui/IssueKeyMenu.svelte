<script lang="ts">
	import { resolve } from '$app/paths';

	let {
		trigger,
		user,
		inline = false,
		align = 'left',
		onnavigate
	}: {
		/** Classes for the button that opens the menu. */
		trigger: string;
		/** Pre-selects this user on the form. */
		user?: string;
		/** The list pushes the page down instead of floating over it (phone menu). */
		inline?: boolean;
		align?: 'left' | 'right';
		onnavigate?: () => void;
	} = $props();

	const kinds = [
		{
			type: 'tag',
			label: 'NFC tag',
			text: 'A physical tag the member taps on the door. The key is bound to the tag’s serial number and cannot be written to another tag.'
		},
		{
			type: 'virtual',
			label: 'Virtual key',
			text: 'No tag. It unlocks the door remotely, from the backoffice.'
		}
	];

	let open = $state(false);
	let root = $state<HTMLElement>();

	const href = (type: string) =>
		resolve(`/os/keys/create?type=${type}${user ? `&user=${user}` : ''}`);
	const choose = () => {
		open = false;
		onnavigate?.();
	};
</script>

<svelte:window
	onpointerdown={(event) => open && !root?.contains(event.target as Node) && (open = false)}
	onkeydown={(event) => event.key === 'Escape' && (open = false)}
/>

<span bind:this={root} class={inline ? 'contents' : 'relative inline-flex'}>
	<button
		type="button"
		class="{trigger} {inline ? 'w-full' : ''}"
		aria-expanded={open}
		onclick={() => (open = !open)}
	>
		Issue key
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
		<ul class="list" class:stacked={inline} class:floating={!inline} class:end={align === 'right'}>
			{#each kinds as kind (kind.type)}
				<li>
					<a href={href(kind.type)} onclick={choose}>
						<strong>{kind.label}</strong>
						<span>{kind.text}</span>
					</a>
				</li>
			{/each}
		</ul>
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
	.list {
		z-index: 30;
		margin: 0;
		padding: 0;
		list-style: none;
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
	.list li + li {
		border-top: 1px solid var(--c-seam);
	}
	.list a {
		display: flex;
		flex-direction: column;
		gap: 0.25rem;
		padding: 0.75rem 1rem;
		color: inherit;
		text-decoration: none;
		letter-spacing: normal;
		text-transform: none;
		white-space: normal;
		transition: background-color var(--dur-fast) var(--ease-out);
	}
	.list a:hover,
	.list a:focus-visible {
		background: var(--c-chassis);
		outline: none;
	}
	.list a:focus-visible {
		box-shadow: inset 2px 0 0 var(--c-signal-hi);
	}
	.list strong {
		font-family: var(--font-mono);
		font-size: 0.75rem;
		font-weight: 700;
		letter-spacing: 0.12em;
		text-transform: uppercase;
	}
	.list span {
		font-size: 0.8125rem;
		line-height: 1.35;
		color: var(--c-mute);
	}
	@keyframes menu-in {
		from {
			opacity: 0;
			transform: translateY(-4px) scale(0.97);
		}
	}
	@media (prefers-reduced-motion: reduce) {
		.list {
			animation: none;
		}
		.chevron {
			transition: none;
		}
	}
</style>
