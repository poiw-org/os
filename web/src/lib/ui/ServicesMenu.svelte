<script lang="ts">
	import { resolve } from '$app/paths';
	import Dropdown from './Dropdown.svelte';
	import { t } from '#lib/i18n.svelte.ts';

	// The lab's other web services, for the homepage bar.
</script>

{#snippet row(title: string, text: string, external = false)}
	<strong>
		{title}
		{#if external}<span aria-hidden="true">↗</span>{/if}
	</strong>
	<span>{text}</span>
{/snippet}

<!-- The bar lights its cell on hover (console.css), like the links beside this one. -->
<Dropdown trigger="label inline-flex items-center" align="nav">
	{#snippet label()}{t('Online services')}{/snippet}
	{#snippet children(close)}
		<ul class="list">
			<li>
				<a href={resolve('/os')} onclick={close}>
					{@render row(
						t('Lab key management'),
						t('See your door keys, and disable one if you lose it.')
					)}
				</a>
			</li>
			<li>
				<a href="https://print.poiw.org" onclick={close}>
					{@render row(
						t('3D print lab'),
						t("Get something printed on the lab's 3D printer."),
						true
					)}
				</a>
			</li>
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
	.list a > span {
		font-size: 0.8125rem;
		line-height: 1.35;
		color: var(--c-mute);
	}
</style>
