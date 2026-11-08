<script lang="ts">
	import { resolve } from '$app/paths';
	import { fade, fly } from 'svelte/transition';
	import {
		completeOnboarding,
		EXISTING_MEMBER_SOURCE,
		signOut,
		signUp,
		type OnboardingAnswers
	} from '#lib/auth.ts';
	import OnboardingStage from '#lib/auth/OnboardingStage.svelte';
	import Turnstile from '#lib/auth/Turnstile.svelte';
	import { easeOut, reduced } from '#lib/motion.ts';
	import { PASSWORD_RULES, passwordIsStrong } from '#lib/password.ts';
	import Brand from '#lib/ui/Brand.svelte';
	import type { PageProps } from './$types';

	let { data }: PageProps = $props();

	const sources: [OnboardingAnswers['referralSource'], string][] = [
		['friend', 'Μέσω φίλου'],
		['welcome_day', 'Ημέρα υποδοχής πρωτοετών'],
		['newsletter', 'Newsletter'],
		['campaign', 'Διαφημιστική καμπάνια']
	];

	type Step = 'name' | 'studies' | 'source' | 'diy' | 'account';

	// New members answer before an account exists, so a signed-in visitor here is an older member.
	const existing = $derived(Boolean(data.user));
	const steps = $derived<Step[]>(
		existing ? ['name', 'studies', 'diy'] : ['name', 'studies', 'source', 'diy', 'account']
	);

	let index = $state(0);
	let direction = $state(1);
	let firstName = $state('');
	let lastName = $state('');
	let studies = $state<boolean>();
	let department = $state('');
	let referralSource = $state<OnboardingAnswers['referralSource']>();
	// svelte-ignore state_referenced_locally
	let diyFamiliar = $state<boolean | undefined>(data.user ? true : undefined);
	let email = $state('');
	let password = $state('');
	let confirm = $state('');
	let error = $state('');
	let busy = $state(false);
	let waitingForEmail = $state(false);
	let knock = $state(0);
	let captcha = $state<Turnstile>();

	const step = $derived(steps[index]);
	const last = $derived(index === steps.length - 1);
	const SCENES: Record<Step, 0 | 1 | 2 | 3 | 4> = {
		name: 0,
		studies: 1,
		source: 2,
		diy: 3,
		account: 4
	};
	const scene = $derived(SCENES[step]);

	const stepIn = (node: Element) =>
		fly(node, {
			x: reduced() ? 0 : direction * 36,
			duration: reduced() ? 0 : 340,
			easing: easeOut
		});
	const stepOut = (node: Element) => fade(node, { duration: reduced() ? 0 : 110 });

	const move = (by: number) => {
		error = '';
		direction = by;
		index += by;
	};

	const fail = (message: string) => {
		error = message;
		knock++;
		busy = false;
	};

	const answers = (): OnboardingAnswers => ({
		firstName,
		lastName,
		studies: studies as boolean,
		department,
		referralSource: existing
			? EXISTING_MEMBER_SOURCE
			: (referralSource as OnboardingAnswers['referralSource']),
		diyFamiliar: diyFamiliar as boolean
	});

	const submit = async (event: SubmitEvent) => {
		event.preventDefault();
		error = '';
		if (!last) return move(1);
		if (!existing && !passwordIsStrong(password))
			return fail('Ο κωδικός δεν καλύπτει όλες τις απαιτήσεις.');
		if (!existing && password !== confirm) return fail('Οι κωδικοί δεν ταιριάζουν.');
		busy = true;
		try {
			if (data.user) {
				await completeOnboarding(data.user.id, answers());
				window.location.href = '/security';
				return;
			}
			const token = await captcha!.take();
			if (await signUp(email, password, answers(), token)) window.location.href = '/security';
			else {
				waitingForEmail = true;
				busy = false;
			}
		} catch (e) {
			fail(e instanceof Error ? e.message : 'Κάτι πήγε στραβά.');
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
		<a href={resolve('/')} class="label" aria-label="po/iw">
			<Brand size={24} />
			po/iw
		</a>
		<div class="grow"></div>
		{#if existing}
			<button type="button" class="label" onclick={leave}>Αποσύνδεση</button>
		{:else}
			<a href={resolve('/login')} class="label">Σύνδεση</a>
		{/if}
	</header>

	<main class="mx-auto w-full max-w-2xl px-4 py-10 md:py-16">
		<div class="plate flex flex-col gap-6 p-6 md:p-10">
			<OnboardingStage {scene} {knock} />

			{#if waitingForEmail}
				<div in:fly={{ y: reduced() ? 0 : 12, duration: reduced() ? 0 : 320, easing: easeOut }}>
					<h1 class="text-3xl md:text-4xl">Έλεγξε το email σου.</h1>
					<p class="mute mt-3 max-w-prose">
						Στείλαμε έναν σύνδεσμο στο {email}. Άνοιξέ τον για να ολοκληρώσεις την εγγραφή σου.
					</p>
				</div>
			{:else}
				<form onsubmit={submit} class="flex flex-col gap-6">
					<div class="flex items-center gap-3">
						<span class="label">Βήμα {index + 1} από {steps.length}</span>
						<span class="track" aria-hidden="true">
							<span class="fill" style:transform="scaleX({(index + 1) / steps.length})"></span>
						</span>
					</div>

					<div class="slot-stack">
						{#key step}
							<div in:stepIn out:stepOut class="flex flex-col gap-6">
								{#if step === 'name'}
									<h1 class="text-3xl md:text-4xl">Καλώς ήρθες. Πώς σε λένε;</h1>
									<div class="grid gap-5 sm:grid-cols-2">
										<label class="flex flex-col gap-1.5">
											<span class="label">Όνομα</span>
											<input
												required
												autocomplete="given-name"
												bind:value={firstName}
												class="slot"
											/>
										</label>
										<label class="flex flex-col gap-1.5">
											<span class="label">Επώνυμο</span>
											<input
												required
												autocomplete="family-name"
												bind:value={lastName}
												class="slot"
											/>
										</label>
									</div>
								{:else if step === 'studies'}
									<fieldset class="flex flex-col gap-3">
										<legend class="mb-5 text-3xl md:text-4xl">Σπουδάζεις;</legend>
										<div class="choices">
											<label class="choice">
												<input
													type="radio"
													name="studies"
													required
													value={true}
													bind:group={studies}
												/>
												<span>Ναι</span>
											</label>
											<label class="choice">
												<input type="radio" name="studies" value={false} bind:group={studies} />
												<span>Όχι</span>
											</label>
										</div>
										{#if studies}
											<label class="mt-2 flex flex-col gap-1.5">
												<span class="label">Τμήμα σπουδών</span>
												<input required bind:value={department} class="slot" />
											</label>
										{/if}
									</fieldset>
								{:else if step === 'source'}
									<fieldset>
										<legend class="mb-5 text-3xl md:text-4xl">Πώς μας βρήκες;</legend>
										<div class="choices">
											{#each sources as [value, text] (value)}
												<label class="choice">
													<input
														type="radio"
														name="source"
														required
														{value}
														bind:group={referralSource}
													/>
													<span>{text}</span>
												</label>
											{/each}
										</div>
									</fieldset>
								{:else if step === 'diy'}
									<fieldset>
										<legend class="mb-5 text-3xl md:text-4xl">
											Είσαι εξοικειωμένος/η με DIY εξοπλισμό;
										</legend>
										<div class="choices">
											<label class="choice">
												<input
													type="radio"
													name="diy"
													required
													value={true}
													bind:group={diyFamiliar}
												/>
												<span>Ναι</span>
											</label>
											<label class="choice">
												<input type="radio" name="diy" value={false} bind:group={diyFamiliar} />
												<span>Όχι</span>
											</label>
										</div>
									</fieldset>
								{:else}
									<h1 class="text-3xl md:text-4xl">Φτιάξε τον λογαριασμό σου.</h1>
									<label class="flex flex-col gap-1.5">
										<span class="label">Email</span>
										<input
											type="email"
											required
											autocomplete="username"
											bind:value={email}
											class="slot"
										/>
									</label>
									<label class="flex flex-col gap-1.5">
										<span class="label">Κωδικός</span>
										<input
											type="password"
											required
											autocomplete="new-password"
											bind:value={password}
											class="slot"
										/>
									</label>
									<ul class="rules">
										{#each PASSWORD_RULES as rule (rule.el)}
											<li class:met={rule.test(password)}>{rule.el}</li>
										{/each}
									</ul>
									<label class="flex flex-col gap-1.5">
										<span class="label">Επιβεβαίωση κωδικού</span>
										<input
											type="password"
											required
											autocomplete="new-password"
											bind:value={confirm}
											class="slot"
										/>
									</label>
									<Turnstile bind:this={captcha} lang="el" />
								{/if}
							</div>
						{/key}
					</div>

					{#if error}
						<p role="alert" class="text-sm font-bold" style:color="var(--c-danger)">{error}</p>
					{/if}

					<div class="flex gap-3">
						{#if index > 0}
							<button type="button" class="key" disabled={busy} onclick={() => move(-1)}
								>Πίσω</button
							>
						{/if}
						<button type="submit" class="key key-signal" disabled={busy}>
							{busy ? 'Αποθήκευση…' : step === 'account' ? 'Δημιουργία λογαριασμού' : 'Συνέχεια'}
						</button>
					</div>
				</form>
			{/if}
		</div>
	</main>
</div>

<style>
	.track {
		flex: 1;
		height: 2px;
		background: var(--c-seam);
	}
	.fill {
		display: block;
		height: 100%;
		background: var(--c-signal);
		transform-origin: left;
		transition: transform 420ms var(--ease-out);
	}
	/* The outgoing and incoming steps share one cell while they cross. */
	.slot-stack {
		display: grid;
	}
	.slot-stack > :global(*) {
		grid-area: 1 / 1;
		min-width: 0;
	}
	.rules {
		display: grid;
		gap: 0.25rem;
		margin: 0;
		padding: 0;
		list-style: none;
		font-size: 0.8125rem;
		color: var(--c-mute);
	}
	.rules li {
		transition: color 160ms var(--ease-out);
	}
	.rules li::before {
		content: '○ ';
	}
	.rules li.met {
		color: var(--c-text);
	}
	.rules li.met::before {
		content: '● ';
		color: var(--c-signal);
	}
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
		.choice span,
		.fill,
		.rules li {
			transition: none;
		}
	}
</style>
