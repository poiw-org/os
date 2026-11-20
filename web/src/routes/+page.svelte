<script lang="ts">
	import { resolve } from '$app/paths';
	import PrinterViewer from '#lib/PrinterViewer.svelte';
	import Brand from '#lib/ui/Brand.svelte';
	import LocaleSwitch from '#lib/ui/LocaleSwitch.svelte';
	import ServicesMenu from '#lib/ui/ServicesMenu.svelte';
	import { getLocale, t } from '#lib/i18n.svelte.ts';
	import type { PageProps } from './$types';

	let { data }: PageProps = $props();
	// Past the hero logo the bar grows to full height and shows the wordmark.
	let scrolled = $state(false);

	const email = 'poiw-group@hmu.gr';
	// Labels are translated where they are shown; the English text doubles as each group's id.
	const nav = [
		['About', '#about'],
		['Join', '#join'],
		['Projects', '#projects'],
		['Equipment', '#equipment']
	];

	// [name, note?]
	const equipment: { group: string; blurb: string; items: [string, string?][] }[] = [
		{
			group: 'Consumables',
			blurb: 'Electronics and electrical parts, kept in stock.',
			items: [['Resistors'], ['Capacitors'], ['Transistors'], ['Diodes'], ['Sensors']]
		},
		{
			group: 'Microcontrollers & computers',
			blurb: 'Several of each, not just one.',
			items: [
				['ESP32'],
				['ESP8266'],
				['Raspberry Pi 4'],
				['Raspberry Pi 5'],
				['Raspberry Pi Pico'],
				['Raspberry Pi Zero 2'],
				['Workstation PCs', '8 CPUs, 8GB RAM']
			]
		},
		{
			group: 'Fabrication',
			blurb: 'Where an idea becomes an object.',
			items: [['3D Printer'], ['Prototyping boards'], ['Power tools']]
		},
		{
			group: 'Soldering',
			blurb: 'Soldering, desoldering and clean air.',
			items: [['Soldering irons'], ['Desoldering station'], ['Heat gun'], ['Fume extractors']]
		},
		{
			group: 'Measurement',
			blurb: 'To see what the circuit is up to.',
			items: [['Oscilloscopes'], ['Function generators']]
		},
		{
			group: 'Power',
			blurb: '',
			items: [['High-power DC supplies'], ['Solar panels', 'Low voltage']]
		},
		{
			group: 'Surprise',
			blurb: '',
			items: [['A mini fridge full of goodies', 'Now and then']]
		}
	];

	let query = $state('');
	let only = $state<string | null>(null);

	const norm = (s: string) => s.toLowerCase().normalize('NFD').replace(/\p{M}/gu, '');

	const shown = $derived.by(() => {
		const q = norm(query.trim());
		return equipment
			.filter((g) => !only || g.group === only)
			.map((g) => ({
				...g,
				items: g.items.filter(
					([name, note]) => !q || norm(`${name} ${note ?? ''} ${g.group}`).includes(q)
				)
			}))
			.filter((g) => g.items.length);
	});
</script>

<svelte:head>
	<title>po/iw ~ students makerspace @ hmu</title>
	<meta
		name="description"
		content={t(
			'An incubator for ideas. The po/iw lab is an open space for anyone who wants to build an idea.'
		)}
	/>
</svelte:head>

<svelte:window onscroll={() => (scrolled = scrollY > 160)} />

<div lang={getLocale()} class="console">
	<header class="bar tight" class:full={scrolled}>
		<a
			href={resolve('/')}
			class="label brand-cell"
			aria-hidden={!scrolled}
			tabindex={scrolled ? 0 : -1}>po/iw</a
		>
		{#each nav as [text, hash] (hash)}
			<a href={hash} class="label hide-sm">{t(text)}</a>
		{/each}
		<ServicesMenu />
		<div class="grow"></div>
		<a href="https://github.com/poiw-org" class="label hide-sm">GitHub</a>
		<LocaleSwitch />
		<a href={resolve(data.user ? '/os' : '/login')} class="label">
			{data.user ? 'OS' : t('Sign in')}
		</a>
		{#if !data.user}
			<!-- Not on a phone: the bar has no room left, and the hero shows the same button. -->
			<div class="key-cell hide-sm">
				<a href={resolve('/onboarding')} class="key key-signal">{t('Become a member')}</a>
			</div>
		{/if}
	</header>

	<main>
		<section class="grid lg:grid-cols-[minmax(0,5fr)_minmax(0,4fr)]">
			<div class="rise flex flex-col justify-between gap-12 p-6 md:p-12" style="--d: 0ms">
				<div>
					<Brand size={150} />
					<p class="label mt-6 text-[var(--c-mute)]">students makerspace @ hmu</p>
				</div>
				<div>
					<h1 class="display">{t('A place to')}<br />{t('hatch ideas.')}</h1>
					<p class="mt-6 max-w-xl text-lg text-[var(--c-mute)]">
						{t(
							'The po/iw lab is an open space where anyone can turn an idea of theirs into something real, using the supplies and equipment we provide.'
						)}
					</p>
					<div class="mt-8 flex flex-wrap gap-3">
						{#if data.user}
							<a href={resolve('/os')} class="key key-signal">{t('Open OS')}</a>
						{:else}
							<a href={resolve('/onboarding')} class="key key-signal">{t('Become a member')}</a>
						{/if}
						<a href="#about" class="key">{t("What's this about?")}</a>
					</div>
				</div>
			</div>

			<div class="rise p-2 md:p-4" style="--d: 100ms">
				<PrinterViewer />
			</div>
		</section>

		<section class="grid md:grid-cols-2">
			<article class="plate m-4 p-6 md:m-6 md:p-8">
				<h2 class="text-2xl md:text-3xl">{t('Ask, suggest, act.')}</h2>
				<p class="mute mt-4">
					{t(
						'We want a good place for learning at every age, where student members trade ideas and questions.'
					)}
				</p>
			</article>
			<article class="plate m-4 p-6 md:m-6 md:ml-0 md:p-8">
				<h2 class="text-2xl md:text-3xl">{t('Reaching out, sharing culture.')}</h2>
				<p class="mute mt-4">
					{t(
						'Under the po/iw umbrella we run projects and events that spread the culture of free and open source software and hardware.'
					)}
				</p>
			</article>
		</section>

		<section id="about" class="strip scroll-mt-14">
			<div class="spine"><h2>{t('About')}</h2></div>
			<div class="strip-body grid gap-6 lg:grid-cols-2">
				<div class="plate space-y-4 p-6">
					<h3 class="text-xl">{t('Introduction')}</h3>
					<p>
						{t(
							'po/iw (from the Greek ποιώ, “I make”) grew out of a need among students at the Hellenic Mediterranean University for a place to work together and build projects of all kinds, mostly open source software and hardware.'
						)}
					</p>
					<p>
						{t(
							"The lab is on the Heraklion campus, on the ground floor of the old ΣΕΥΠ building (K14), and belongs to HMU's Office of Cultural and Sports Activities."
						)}
					</p>
				</div>
				<div class="flex flex-col gap-6">
					<div class="plate space-y-4 p-6">
						<h3 class="text-xl">{t('Management team')}</h3>
						<p>
							{t(
								'The lab and the group are run by a three-person council, the Management Team, elected every academic year under the'
							)}
							<a href="https://eclass.hmu.gr/modules/document/?course=KARANASTASIS106"
								>{t('internal regulations')}</a
							>.
						</p>
					</div>
					<div class="plate space-y-3 p-6">
						<h3 class="text-xl">{t('How to find us')}</h3>
						<p>
							{t(
								"We're across from HMU's main entrance. Walk past the guardhouse, the canteen and the international office, then into the old ΣΕΥΠ building, the one behind the trees in the open yard. We're on the ground floor, down the last corridor on the left."
							)}
						</p>
					</div>
				</div>
			</div>
		</section>

		<section id="join" class="strip scroll-mt-14">
			<div class="spine"><h2>{t('Join')}</h2></div>
			<div class="strip-body grid gap-6 lg:grid-cols-2">
				<div class="plate space-y-4 p-6">
					<h3 class="text-xl">{t('Signing up')}</h3>
					<p>
						{t(
							"You don't need to be a member to use the lab. Message someone who already is and come along together. If you'd like more independence, or a say in the group's decisions, you can sign up as a member. Just get in touch and we'll help."
						)}
					</p>
					{#if !data.user}
						<a href={resolve('/onboarding')} class="key key-signal">{t('Become a member')}</a>
					{/if}
				</div>
				<div class="plate space-y-4 p-6">
					<h3 class="text-xl">{t('Contact')}</h3>
					<dl class="readout">
						<div class="px-4 py-3">
							<dt class="label mute">HMU eClass</dt>
							<dd class="mt-1 font-bold">
								<a href="https://eclass.hmu.gr/courses/KARANASTASIS106/">KARANASTASIS106</a>
							</dd>
						</div>
						<div class="px-4 py-3">
							<dt class="label mute">Discord Server</dt>
							<dd class="mt-1 font-bold">
								<a href="https://discord.poiw.org">discord.poiw.org</a>
							</dd>
						</div>
						<div class="px-4 py-3">
							<dt class="label mute">E-mail</dt>
							<dd class="mt-1 font-bold"><a href="mailto:{email}">{email}</a></dd>
						</div>
					</dl>
				</div>
			</div>
		</section>

		<section id="projects" class="strip scroll-mt-14">
			<div class="spine"><h2>{t('Projects')}</h2></div>
			<div class="strip-body grid gap-6 lg:grid-cols-2">
				<article class="plate flex flex-col gap-4 p-6">
					<h3 class="text-2xl">Saffron</h3>
					<p>
						{t(
							'Saffron gathers announcements from hundreds of Greek schools and universities into one consistent, easy-to-extend feed. We started building it in the first four months of 2021, together with'
						)}
						<a href="https://unistudents.gr">UniStudents</a>.
					</p>
					<div class="mt-auto flex flex-wrap gap-3 pt-2">
						<a href="https://saffron.poiw.org" class="key">saffron.poiw.org</a>
						<a href="https://github.com/poiw-org/Saffron" class="key">GitHub</a>
					</div>
				</article>
				<article class="plate flex flex-col gap-4 p-6">
					<h3 class="text-2xl">Doora</h3>
					<p>
						{t(
							"The lab's guardian. A door access and logging system, an experimental project built for the lab's own needs. It runs on its own and syncs with the cloud in real time, so the door can be opened remotely and every entry is logged."
						)}
					</p>
					<p>
						{t(
							'Members get in with RFID cards that carry signed tokens. Verification happens offline and the device runs on a UPS, so a power or network outage will not lock anyone out.'
						)}
					</p>
					<div class="mt-auto flex flex-wrap gap-3 pt-2">
						<a href={resolve(data.user ? '/os' : '/login')} class="key">{t('Manage cards')}</a>
						<a href="https://github.com/poiw-org/Doora" class="key">GitHub</a>
					</div>
				</article>
			</div>
		</section>

		<section id="equipment" class="strip scroll-mt-14">
			<div class="spine"><h2>{t('Equipment')}</h2></div>
			<div class="strip-body">
				<div class="space-y-6">
					<div class="flex flex-wrap items-center gap-3">
						<input
							type="search"
							bind:value={query}
							placeholder={t('Search equipment: esp32, soldering iron, oscilloscope…')}
							aria-label={t('Search equipment')}
							class="slot min-w-0 flex-1 basis-72"
						/>
						<div class="flex flex-wrap gap-2">
							<button
								type="button"
								class="key"
								aria-pressed={only === null}
								class:key-signal={only === null}
								onclick={() => (only = null)}>{t('All')}</button
							>
							{#each equipment as g (g.group)}
								<button
									type="button"
									class="key"
									aria-pressed={only === g.group}
									class:key-signal={only === g.group}
									onclick={() => (only = only === g.group ? null : g.group)}>{t(g.group)}</button
								>
							{/each}
						</div>
					</div>

					{#if shown.length}
						<div class="grid gap-4 md:grid-cols-2 xl:grid-cols-3">
							{#each shown as g (g.group)}
								<section class="plate space-y-3 p-5">
									<header>
										<h3 class="text-lg">{t(g.group)}</h3>
										{#if g.blurb}<p class="mute text-sm">{t(g.blurb)}</p>{/if}
									</header>
									<ul class="readout">
										{#each g.items as [name, note] (name)}
											<li class="flex flex-wrap items-baseline justify-between gap-x-4 px-4 py-2.5">
												<span class="font-bold">{t(name)}</span>
												{#if note}<span class="label mute">{t(note)}</span>{/if}
											</li>
										{/each}
									</ul>
								</section>
							{/each}
						</div>
					{:else}
						<p class="plate p-6">
							{t('We could not find “{query}”. Ask us at', { query })}
							<a href="mailto:{email}">{email}</a>{t(', we might have it.')}
						</p>
					{/if}

					<div class="plate space-y-2 p-5 text-sm">
						<p class="mute">{t('More and better equipment is coming soon :D')}</p>
					</div>
				</div>
			</div>
		</section>
	</main>

	<footer class="border-t-2 text-xs text-[var(--c-mute)]">
		<div class="flex flex-wrap items-center justify-between gap-3 px-6 py-5 md:px-12">
			<p>
				{t('Open source project made by po/iw. Find the code on our')}
				<a href="https://github.com/poiw-org" class="font-bold underline">{t('GitHub page')}</a>. {t(
					'3D model by'
				)}
				<a
					href="https://sketchfab.com/3d-models/3d-printer-3150a227e54a419e956daba61610e904"
					class="underline">Michael V</a
				>{t(', CC BY 4.0. Logo by')}
				<a
					href="https://www.linkedin.com/in/mariadigenaki/"
					target="_blank"
					rel="noreferrer"
					class="underline">Maria Digenaki</a
				>.
			</p>
			<a href="https://hmu.gr" class="label hover:text-[var(--c-text)]"
				>{t('Hellenic Mediterranean University')}</a
			>
		</div>
	</footer>
</div>

<style>
	:global(html) {
		scroll-behavior: smooth;
	}
	.display {
		font-size: clamp(2.5rem, 7.5vw, 5.75rem);
		line-height: 1.02;
		letter-spacing: 0.01em;
	}
	@media (prefers-reduced-motion: reduce) {
		:global(html) {
			scroll-behavior: auto;
		}
	}
</style>
