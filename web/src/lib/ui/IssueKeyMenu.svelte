<script lang="ts">
	import { resolve } from '$app/paths';
	import Dropdown from './Dropdown.svelte';

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

	const href = (type: string) =>
		resolve(`/os/keys/create?type=${type}${user ? `&user=${user}` : ''}`);
</script>

<Dropdown {trigger} {inline} {align}>
	{#snippet label()}Issue key{/snippet}
	{#snippet children(close)}
		<ul class="list">
			{#each kinds as kind (kind.type)}
				<li>
					<a
						href={href(kind.type)}
						onclick={() => {
							close();
							onnavigate?.();
						}}
					>
						<strong>{kind.label}</strong>
						<span>{kind.text}</span>
					</a>
				</li>
			{/each}
		</ul>
	{/snippet}
</Dropdown>

<style>
	.list {
		margin: 0;
		padding: 0;
		list-style: none;
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
</style>
