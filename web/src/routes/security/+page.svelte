<script lang="ts">
	import { resolve } from '$app/paths';
	import { onMount } from 'svelte';
	import { fly } from 'svelte/transition';
	import {
		dismissSecondFactorPrompt,
		passkeysSupported,
		registerPasskey,
		signOut
	} from '#lib/auth.ts';
	import OnboardingStage from '#lib/auth/OnboardingStage.svelte';
	import TotpEnroll from '#lib/auth/TotpEnroll.svelte';
	import { easeOut, reduced } from '#lib/motion.ts';
	import Brand from '#lib/ui/Brand.svelte';
	import type { PageProps } from './$types';

	let { data }: PageProps = $props();

	const required = $derived(Boolean(data.user?.secondFactorEnforced));
	let canUsePasskey = $state(false);
	let usingApp = $state(false);
	let finishing = $state(false);
	let knock = $state(0);
	let error = $state('');
	let busy = $state(false);

	onMount(() => (canUsePasskey = passkeysSupported()));

	const enter = () => (window.location.href = '/os');

	// They just said no here, so the app shouldn't ask again until the next sign-in.
	const later = () => {
		if (data.user) dismissSecondFactorPrompt(data.user);
		enter();
	};

	const finish = () => {
		finishing = true;
		enter();
	};

	const passkey = async () => {
		busy = true;
		error = '';
		try {
			await registerPasskey();
			finish();
		} catch (e) {
			if (!(e instanceof Error && e.name === 'NotAllowedError')) {
				error = e instanceof Error ? e.message : 'Κάτι πήγε στραβά.';
				knock++;
			}
			busy = false;
		}
	};

	const leave = async () => {
		await signOut();
		window.location.href = '/';
	};
</script>

<svelte:head>
	<title>Ασφάλεια · po/iw</title>
</svelte:head>

<div lang="el" class="console">
	<header class="bar">
		<a href={resolve('/')} class="label" aria-label="po/iw">
			<Brand size={24} />
			po/iw
		</a>
		<div class="grow"></div>
		<button type="button" class="label" onclick={leave}>Αποσύνδεση</button>
	</header>

	<main class="mx-auto w-full max-w-2xl px-4 py-10 md:py-16">
		<div class="plate flex flex-col gap-6 p-6 md:p-10">
			<OnboardingStage scene={4} ended {knock} />

			<div in:fly={{ y: reduced() ? 0 : 12, duration: reduced() ? 0 : 320, easing: easeOut }}>
				<p class="label">{required ? 'Απαραίτητο βήμα' : 'Τελευταίο βήμα'}</p>
				<h1 class="mt-2 text-3xl md:text-4xl">Κλείδωσε τον λογαριασμό σου.</h1>
				<p class="mute mt-3 max-w-prose">
					{required
						? 'Για να κρατήσεις τα κλειδιά σου χρειάζεται passkey ή εφαρμογή ελέγχου ταυτότητας. Αν έχουν ανασταλεί, ανοίγουν ξανά αμέσως.'
						: 'Με ένα passkey μπαίνεις με Face ID, Touch ID ή κλειδί ασφαλείας, χωρίς κωδικό.'}
				</p>
			</div>

			{#if usingApp}
				<TotpEnroll ondone={finish} />
			{:else}
				<div class="flex flex-wrap gap-3">
					{#if canUsePasskey}
						<button
							type="button"
							class="key key-signal"
							disabled={busy || finishing}
							onclick={passkey}
						>
							Δημιουργία passkey
						</button>
					{/if}
					<button
						type="button"
						class="key"
						disabled={busy || finishing}
						onclick={() => (usingApp = true)}
					>
						Εφαρμογή ελέγχου ταυτότητας
					</button>
				</div>
			{/if}

			{#if error}
				<p role="alert" class="text-sm font-bold" style:color="var(--c-danger)">{error}</p>
			{/if}

			{#if !required}
				<button type="button" class="label self-start" onclick={later}>Όχι τώρα</button>
			{/if}
		</div>
	</main>
</div>
