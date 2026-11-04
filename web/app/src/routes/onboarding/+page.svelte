<script lang="ts">
	import { completeOnboarding, signOut, type OnboardingAnswers } from '#lib/auth.ts';
	import Brand from '#lib/ui/Brand.svelte';
	import type { PageProps } from './$types';

	let { data }: PageProps = $props();

	const sources: [OnboardingAnswers['referralSource'], string][] = [
		['friend', 'Μέσω φίλου'],
		['welcome_day', 'Ημέρα υποδοχής πρωτοετών'],
		['newsletter', 'Newsletter'],
		['campaign', 'Διαφημιστική καμπάνια']
	];

	let firstName = $state('');
	let lastName = $state('');
	let studies = $state<boolean>();
	let department = $state('');
	let referralSource = $state<OnboardingAnswers['referralSource']>();
	let diyFamiliar = $state<boolean>();
	let error = $state('');
	let busy = $state(false);

	const submit = async (event: SubmitEvent) => {
		event.preventDefault();
		error = '';
		if (!data.user || studies === undefined || !referralSource || diyFamiliar === undefined) {
			error = 'Απάντησε σε όλες τις ερωτήσεις.';
			return;
		}
		busy = true;
		try {
			await completeOnboarding(data.user.id, {
				firstName,
				lastName,
				studies,
				department,
				referralSource,
				diyFamiliar
			});
			window.location.href = '/os';
		} catch (e) {
			error = e instanceof Error ? e.message : 'Κάτι πήγε στραβά.';
			busy = false;
		}
	};

	const leave = async () => {
		await signOut();
		window.location.href = '/';
	};
</script>

<svelte:head>
	<title>Καλώς ήρθες · po/iw</title>
</svelte:head>

<div lang="el" class="console">
	<header class="bar">
		<a href="/" class="label" aria-label="po/iw">
			<Brand size={24} />
			po/iw
		</a>
		<div class="grow"></div>
		<button type="button" class="label" onclick={leave}>Αποσύνδεση</button>
	</header>

	<main class="mx-auto w-full max-w-2xl px-4 py-10 md:py-16">
		<form onsubmit={submit} class="plate flex flex-col gap-8 p-6 md:p-10">
			<div>
				<h1 class="text-3xl md:text-4xl">Καλώς ήρθες.</h1>
				<p class="mute mt-3 max-w-prose">
					Πέντε γρήγορες ερωτήσεις για να σε γνωρίσουμε. Τις απαντάς μία φορά.
				</p>
			</div>

			<div class="grid gap-5 sm:grid-cols-2">
				<label class="flex flex-col gap-1.5">
					<span class="label">Όνομα</span>
					<input required autocomplete="given-name" bind:value={firstName} class="slot" />
				</label>
				<label class="flex flex-col gap-1.5">
					<span class="label">Επώνυμο</span>
					<input required autocomplete="family-name" bind:value={lastName} class="slot" />
				</label>
			</div>

			<fieldset class="flex flex-col gap-3">
				<legend class="label mb-3">Σπουδάζεις;</legend>
				<div class="choices">
					<label class="choice"
						><input type="radio" name="studies" required onchange={() => (studies = true)} /><span
							>Ναι</span
						></label
					>
					<label class="choice"
						><input type="radio" name="studies" onchange={() => (studies = false)} /><span>Όχι</span
						></label
					>
				</div>
				{#if studies}
					<label class="mt-2 flex flex-col gap-1.5">
						<span class="label">Τμήμα σπουδών</span>
						<input required bind:value={department} class="slot" />
					</label>
				{/if}
			</fieldset>

			<fieldset>
				<legend class="label mb-3">Πώς μας βρήκες;</legend>
				<div class="choices">
					{#each sources as [value, text] (value)}
						<label class="choice"
							><input
								type="radio"
								name="source"
								required
								onchange={() => (referralSource = value)}
							/><span>{text}</span></label
						>
					{/each}
				</div>
			</fieldset>

			<fieldset>
				<legend class="label mb-3">Είσαι εξοικειωμένος/η με DIY εξοπλισμό;</legend>
				<div class="choices">
					<label class="choice"
						><input type="radio" name="diy" required onchange={() => (diyFamiliar = true)} /><span
							>Ναι</span
						></label
					>
					<label class="choice"
						><input type="radio" name="diy" onchange={() => (diyFamiliar = false)} /><span>Όχι</span
						></label
					>
				</div>
			</fieldset>

			{#if error}
				<p role="alert" class="text-sm font-bold" style:color="var(--c-danger)">{error}</p>
			{/if}

			<button type="submit" class="key key-signal self-start" disabled={busy}>
				{busy ? 'Αποθήκευση…' : 'Συνέχεια'}
			</button>
		</form>
	</main>
</div>

<style>
	.choices {
		display: flex;
		flex-wrap: wrap;
		gap: 0.5rem;
	}
	.choice {
		position: relative;
		display: inline-flex;
	}
	.choice input {
		position: absolute;
		inset: 0;
		opacity: 0;
		cursor: pointer;
	}
	.choice span {
		display: inline-flex;
		align-items: center;
		min-height: 2.75rem;
		padding: 0 1rem;
		border: 1px solid var(--c-seam);
		background: var(--c-plate-2);
		box-shadow: var(--bevel-up);
		transition:
			transform 90ms var(--ease-out),
			background-color 120ms var(--ease-out);
	}
	.choice:hover span {
		background: var(--c-plate-hover);
	}
	.choice input:checked + span {
		background: var(--c-signal);
		color: var(--c-signal-ink);
		box-shadow: var(--bevel-down);
		transform: translateY(1px);
	}
	.choice input:focus-visible + span {
		outline: 2px solid var(--c-signal);
		outline-offset: 2px;
	}
	@media (prefers-reduced-motion: reduce) {
		.choice span {
			transition: none;
		}
	}
</style>
