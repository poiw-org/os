<script lang="ts" module>
	type TurnstileApi = {
		render: (
			host: HTMLElement,
			options: {
				sitekey: string;
				appearance: 'interaction-only';
				callback: (token: string) => void;
				'expired-callback': () => void;
			}
		) => string;
		reset: (id: string) => void;
		remove: (id: string) => void;
	};

	const api = () => (window as unknown as { turnstile?: TurnstileApi }).turnstile;

	const SCRIPT_URL = 'https://challenges.cloudflare.com/turnstile/v0/api.js?render=explicit';
	const PRIVACY_URL = 'https://www.cloudflare.com/turnstile-privacy-policy/';
	const TERMS_URL = 'https://www.cloudflare.com/website-terms/';

	let loading: Promise<void> | undefined;
	const load = () =>
		(loading ??= new Promise((resolve, reject) => {
			const script = document.createElement('script');
			script.src = SCRIPT_URL;
			script.onload = () => resolve();
			script.onerror = () => reject(new Error('Could not load the captcha.'));
			document.head.append(script);
		}));

	// Cloudflare's script only exists on the forms that need it: it leaves with the widget, and the
	// signed-in app never loads it.
	const discard = () => {
		document.querySelector(`script[src="${SCRIPT_URL}"]`)?.remove();
		delete (window as unknown as { turnstile?: TurnstileApi }).turnstile;
		loading = undefined;
	};
</script>

<script lang="ts">
	import { onMount } from 'svelte';
	import { config } from '#lib/config.ts';

	const WAIT_MS = 20_000;

	let { lang = 'en' }: { lang?: 'en' | 'el' } = $props();

	let host: HTMLDivElement;
	let widget: string | undefined;
	let token: string | undefined;
	let waiting: ((value: string) => void) | undefined;

	onMount(() => {
		let gone = false;
		load()
			.then(() => {
				if (gone) return;
				widget = api()?.render(host, {
					sitekey: config.turnstileSiteKey,
					// Stays invisible unless Cloudflare needs the visitor to solve something.
					appearance: 'interaction-only',
					callback: (value) => {
						token = value;
						waiting?.(value);
					},
					'expired-callback': () => (token = undefined)
				});
			})
			.catch(() => {});
		return () => {
			gone = true;
			if (widget) api()?.remove(widget);
			discard();
		};
	});

	/** A fresh token for one Supabase request. A token works once, so the widget starts over. */
	export async function take() {
		const value =
			token ??
			(await new Promise<string>((resolve, reject) => {
				waiting = resolve;
				setTimeout(() => reject(new Error('Complete the captcha and try again.')), WAIT_MS);
			}));
		token = undefined;
		waiting = undefined;
		if (widget) api()?.reset(widget);
		return value;
	}
</script>

<div bind:this={host}></div>
<p class="text-xs text-gray-400">
	{#if lang === 'el'}
		Η φόρμα προστατεύεται από το Cloudflare Turnstile, το οποίο επεξεργάζεται τεχνικά στοιχεία της
		συσκευής σου για τον εντοπισμό bots. Ισχύουν η
		<a href={PRIVACY_URL} target="_blank" rel="noreferrer" class="underline">Πολιτική Απορρήτου</a>
		και οι
		<a href={TERMS_URL} target="_blank" rel="noreferrer" class="underline">Όροι</a> της Cloudflare.
	{:else}
		This form is protected by Cloudflare Turnstile, which processes technical details about your
		device to detect bots. Cloudflare's
		<a href={PRIVACY_URL} target="_blank" rel="noreferrer" class="underline">Privacy Policy</a>
		and <a href={TERMS_URL} target="_blank" rel="noreferrer" class="underline">Terms</a> apply.
	{/if}
</p>
