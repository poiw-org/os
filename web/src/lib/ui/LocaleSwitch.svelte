<script lang="ts">
	import { getLocale, setLocale, t } from '#lib/i18n.svelte.ts';

	let { class: className = 'label' }: { class?: string } = $props();
	const next = $derived(getLocale() === 'el' ? 'en' : 'el');
</script>

<!-- Shows the language you would switch to, in that language. -->
<button
	type="button"
	class={className}
	lang={next}
	aria-label={t('Switch language')}
	onclick={() => setLocale(next)}>{next === 'el' ? 'ΕΛ' : 'EN'}</button
>
