<script lang="ts">
	import { resolve } from '$app/paths';
	import PrinterViewer from '#lib/PrinterViewer.svelte';
	import RegisterDialog from '#lib/RegisterDialog.svelte';
	import Brand from '#lib/ui/Brand.svelte';
	import type { PageProps } from './$types';

	let { data }: PageProps = $props();
	let register = $state<RegisterDialog>();
	// Past the hero logo the bar grows to full height and shows the wordmark.
	let scrolled = $state(false);

	const email = 'poiw-group@hmu.gr';
	const nav = [
		['Σχετικά με', '#about'],
		['Συμμετοχή', '#join'],
		['Έργα', '#projects'],
		['Εξοπλισμός', '#equipment']
	];

	// [name, note?]. `many` groups hold quantities, not a single unit.
	const equipment: { group: string; blurb: string; items: [string, string?][] }[] = [
		{
			group: 'Αναλώσιμα',
			blurb: 'Ηλεκτρονικά και ηλεκτρολογικά, σε αποθέματα.',
			items: [
				['Αντιστάσεις'],
				['Πυκνωτές'],
				['Transistors'],
				['Δίοδοι'],
				['Αισθητήρια']
			]
		},
		{
			group: 'Μικροελεγκτές & υπολογιστές',
			blurb: 'Σε πολλαπλές ποσότητες, όχι μόνο ένα από το καθένα.',
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
			group: 'Κατασκευή',
			blurb: 'Για να γίνει η ιδέα αντικείμενο.',
			items: [
				['3D Printer'],
				['Prototyping boards'],
				['Ηλεκτρικά εργαλεία']
			]
		},
		{
			group: 'Συγκόλληση',
			blurb: 'Κόλληση, αποκόλληση και καθαρός αέρας.',
			items: [
				['Κολλητήρια'],
				['Σταθμός αποκόλλησης'],
				['Heat gun'],
				['Απορροφητήρες καπνού']
			]
		},
		{
			group: 'Μετρήσεις',
			blurb: 'Για να δεις τι συμβαίνει στο κύκλωμα.',
			items: [['Παλμογράφοι'], ['Γεννήτριες παλμών']]
		},
		{
			group: 'Τροφοδοσία',
			blurb: '',
			items: [['Ισχυρά τροφοδοτικά DC'], ['Φωτοβολταϊκά', 'Μικρής τάσης']]
		},
		{
			group: 'Έκπληξη',
			blurb: '',
			items: [['Ψυγειάκι με καλούδια', 'Καμία φορά']]
		}
	];

	let query = $state('');
	let only = $state<string | null>(null);

	const norm = (s: string) =>
		s
			.toLowerCase()
			.normalize('NFD')
			.replace(/\p{M}/gu, '');

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
		content="Ένας χώρος εκκόλαψης ιδεών. Το εργαστήριο του po/iw είναι ένας ανοικτός χώρος για κάθε άτομο που θέλει να υλοποιήσει μια ιδέα."
	/>
</svelte:head>

<svelte:window onscroll={() => (scrolled = scrollY > 160)} />

<div lang="el" class="console">
	<RegisterDialog bind:this={register} />
	<header class="bar" class:full={scrolled}>
		<a
			href={resolve('/')}
			class="label brand-cell"
			aria-hidden={!scrolled}
			tabindex={scrolled ? 0 : -1}>po/iw</a
		>
		{#each nav as [text, hash] (hash)}
			<a href={hash} class="label hide-sm">{text}</a>
		{/each}
		<div class="grow"></div>
		<a href="https://github.com/poiw-org" class="label hide-sm">GitHub</a>
		<a href={resolve(data.user ? '/os' : '/login')} class="label">
			{data.user ? 'OS' : 'Σύνδεση'}
		</a>
		{#if !data.user}
			<div class="key-cell">
				<button type="button" class="key key-signal" onclick={() => register?.open()}>
					Γίνε μέλος
				</button>
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
					<h1 class="display">Ένας χώρος<br />εκκόλαψης ιδεών.</h1>
					<p class="mt-6 max-w-xl text-lg text-[var(--c-mute)]">
						Το εργαστήριο του po/iw είναι ένας ανοικτός χώρος που μπορεί κάθε άτομο να υλοποιήσει
						κάποια ιδέα που έχει, χρησιμοποιώντας τα αναλώσιμα και τον εξοπλισμό που παρέχονται.
					</p>
					<div class="mt-8 flex flex-wrap gap-3">
						{#if data.user}
							<a href={resolve('/os')} class="key key-signal">Άνοιγμα OS</a>
						{:else}
							<button type="button" class="key key-signal" onclick={() => register?.open()}
								>Γίνε μέλος</button
							>
						{/if}
						<a href="#about" class="key">Τι φάση;</a>
					</div>
				</div>
			</div>

			<div class="rise p-2 md:p-4" style="--d: 100ms">
				<PrinterViewer />
			</div>
		</section>

		<section class="grid md:grid-cols-2">
			<article class="plate m-4 p-6 md:m-6 md:p-8">
				<h2 class="text-2xl md:text-3xl">Ρώτησε, πρότεινε, δράσε.</h2>
				<p class="mute mt-4">
					Στοχεύουμε για ένα ποιοτικό περιβάλλον διά βίου μάθησης, όπου οι φοιτητές και φοιτήτριες
					μέλη ανταλλάσσουν ιδέες & απορίες.
				</p>
			</article>
			<article class="plate m-4 p-6 md:m-6 md:ml-0 md:p-8">
				<h2 class="text-2xl md:text-3xl">Εξωστρέφεια και πολιτισμός.</h2>
				<p class="mute mt-4">
					Κάτω από την ομπρέλα του po/iw υπάγονται projects και εκδηλώσεις που στόχο έχουν την
					διάδοση της κουλτούρας του ανοικτού και ελεύθερου λογισμικού και υλισμικού.
				</p>
			</article>
		</section>

		<section id="about" class="strip scroll-mt-14">
			<div class="spine"><h2>Σχετικά με</h2></div>
			<div class="strip-body grid gap-6 lg:grid-cols-2">
				<div class="plate space-y-4 p-6">
					<h3 class="text-xl">Εισαγωγή</h3>
					<p>
						Το po/iw (ποιώ, φτιάχνω) γεννήθηκε από την ανάγκη φοιτητών του Ελληνικού Μεσογειακού
						Πανεπιστημίου για έναν χώρο συνεργασίας και δημιουργίας για πρότζεκτ διαφόρων ειδών,
						κυρίως ανοικτού/ελεύθερου κώδικα και υλισμικού.
					</p>
					<p>
						Το εργαστήριο στεγάζεται στο Ελληνικό Μεσογειακό Πανεπιστήμιο (Campus Ηρακλείου), στο
						ισόγειο του κτιρίου της παλιάς ΣΕΥΠ (Κ14) και υπάγεται στο Γραφείο Πολιτιστικών και
						Αθλητικών Δραστηριοτήτων ΕΛ.ΜΕ.ΠΑ.
					</p>
				</div>
				<div class="flex flex-col gap-6">
					<div class="plate space-y-4 p-6">
						<h3 class="text-xl">Ομάδα Διαχείρισης</h3>
						<p>
							Το εργαστήριο και η ομάδα σαν σύνολο, διαχειρίζονται από ένα 3-μελές συμβούλιο (την
							Ομάδα Διαχείρισης, ή Ο.Δ.), το οποίο εκλέγεται κάθε ακαδημαϊκό έτος βάσει του
							<a href="https://eclass.hmu.gr/modules/document/?course=KARANASTASIS106"
								>εσωτερικού κανονισμού λειτουργίας</a
							>.
						</p>
					</div>
					<div class="plate space-y-3 p-6">
						<h3 class="text-xl">Πως θα βρείς το εργαστήριο</h3>
						<p>
							Το εργαστήριο βρίσκεται απέναντι από την κεντρική είσοδο του ΕΛ.ΜΕ.ΠΑ. Αφού περάσεις
							το φυλάκειο, την σίτιση και το γραφείο διεθνών, μπες στο κτίριο της παλίας ΣΕΥΠ (το
							κτίριο πίσω από τα δέντρα στον ακάλυπτο). Βρισκόμαστε στο ισόγειο, στον τελευταίο
							διάδρομο αριστερά.
						</p>
					</div>
				</div>
			</div>
		</section>

		<section id="join" class="strip scroll-mt-14">
			<div class="spine"><h2>Συμμετοχή</h2></div>
			<div class="strip-body grid gap-6 lg:grid-cols-2">
				<div class="plate space-y-4 p-6">
					<h3 class="text-xl">Εγγραφή</h3>
					<p>
						Το εργαστήριο δεν απαιτεί να είσαι μέλος για να το χρησιμοποιήσεις. Μπορείς, να
						επικοινωνήσεις με κάποιο υπάρχον μέλος της ομάδας για να έρθετε μαζί. Αν πάλι θέλεις
						μεγαλύτερη αυτονομία ή συμμετοχή στις αποφάσεις της ομάδας, μπορείς να γίνεις κάνεις
						εγγραφή. Για εγγραφή, μπορείς να επικοινωνήσεις με την ομάδα.
					</p>
					{#if !data.user}
						<button type="button" class="key key-signal" onclick={() => register?.open()}
							>Γίνε μέλος</button
						>
					{/if}
				</div>
				<div class="plate space-y-4 p-6">
					<h3 class="text-xl">Επικοινωνία</h3>
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
			<div class="spine"><h2>Έργα</h2></div>
			<div class="strip-body grid gap-6 lg:grid-cols-2">
				<article class="plate flex flex-col gap-4 p-6">
					<h3 class="text-2xl">Saffron</h3>
					<p>
						Το Saffron δημιουργήθηκε σε συνεργασία με το
						<a href="https://unistudents.gr">UniStudents</a> και μέσα από την ανάγκη του για εύκολη, εύρυθμη,
						ομοιόμορφη και επεκτάσιμη περισυλλογή ανακοινώσεων από εκατοντάδες ελληνικές σχολές και πανεπιστήμια.
						Η ανάπτυξή του άρχισε το πρώτο τετράμηνο του 2021.
					</p>
					<div class="mt-auto flex flex-wrap gap-3 pt-2">
						<a href="https://saffron.poiw.org" class="key">saffron.poiw.org</a>
						<a href="https://github.com/poiw-org/Saffron" class="key">GitHub</a>
					</div>
				</article>
				<article class="plate flex flex-col gap-4 p-6">
					<h3 class="text-2xl">Doora</h3>
					<p>
						Η προστάτιδα του εργαστηρίου. Ένα σύστημα ελέγχου και καταγραφής εισόδου, πειραματικό
						πρότζεκτ που δημιουργήθηκε για τις ανάγκες του εργαστηρίου. Λειτουργεί αυτόνομα και
						συγχρονίζεται σε πραγματικό χρόνο με το cloud, επιτρέποντας τον απομακρυσμένο έλεγχο της
						πόρτας και την τήρηση αρχείου καταγραφής.
					</p>
					<p>
						Τα μέλη εισέρχονται με κάρτες RFID που εμπεριέχουν signed tokens. Η επαλήθευση γίνεται
						offline, ενώ η συσκευή συνδέεται σε UPS: δεν υπάρχει φόβος σε διακοπή ρεύματος ή
						συνδεσιμότητας.
					</p>
					<div class="mt-auto flex flex-wrap gap-3 pt-2">
						<a href={resolve(data.user ? '/os' : '/login')} class="key">Διαχείριση καρτών</a>
						<a href="https://github.com/poiw-org/Doora" class="key">GitHub</a>
					</div>
				</article>
			</div>
		</section>

		<section id="equipment" class="strip scroll-mt-14">
			<div class="spine"><h2>Εξοπλισμός</h2></div>
			<div class="strip-body">
				<div class="space-y-6">
					<div class="flex flex-wrap items-center gap-3">
						<input
							type="search"
							bind:value={query}
							placeholder="Ψάξε εξοπλισμό: esp32, κολλητήρι, παλμογράφος…"
							aria-label="Αναζήτηση εξοπλισμού"
							class="slot min-w-0 flex-1 basis-72"
						/>
						<div class="flex flex-wrap gap-2">
							<button
								type="button"
								class="key"
								aria-pressed={only === null}
								class:key-signal={only === null}
								onclick={() => (only = null)}>Όλα</button
							>
							{#each equipment as g (g.group)}
								<button
									type="button"
									class="key"
									aria-pressed={only === g.group}
									class:key-signal={only === g.group}
									onclick={() => (only = only === g.group ? null : g.group)}>{g.group}</button
								>
							{/each}
						</div>
					</div>

					{#if shown.length}
						<div class="grid gap-4 md:grid-cols-2 xl:grid-cols-3">
							{#each shown as g (g.group)}
								<section class="plate space-y-3 p-5">
									<header>
										<h3 class="text-lg">{g.group}</h3>
										{#if g.blurb}<p class="mute text-sm">{g.blurb}</p>{/if}
									</header>
									<ul class="readout">
										{#each g.items as [name, note] (name)}
											<li class="flex flex-wrap items-baseline justify-between gap-x-4 px-4 py-2.5">
												<span class="font-bold">{name}</span>
												{#if note}<span class="label mute">{note}</span>{/if}
											</li>
										{/each}
									</ul>
								</section>
							{/each}
						</div>
					{:else}
						<p class="plate p-6">
							Δεν βρήκαμε «{query}». Ρώτα μας στο <a href="mailto:{email}">{email}</a>, ίσως το
							έχουμε.
						</p>
					{/if}

					<div class="plate space-y-2 p-5 text-sm">
						<p class="mute">Έρχεται περισσότερος & καλύτερος εξοπλισμός σύντομα :D</p>
					</div>
				</div>
			</div>
		</section>
	</main>

	<footer class="border-t-2 text-xs text-[var(--c-mute)]">
		<div class="flex flex-wrap items-center justify-between gap-3 px-6 py-5 md:px-12">
			<p>
				Open Source/FOSS project made by po/iw. Find the source code in our
				<a href="https://github.com/poiw-org" class="font-bold underline">Github page</a>. 3D model
				by
				<a
					href="https://sketchfab.com/3d-models/3d-printer-3150a227e54a419e956daba61610e904"
					class="underline">Michael V</a
				>, CC BY 4.0. Logo by
				<a
					href="https://www.linkedin.com/in/mariadigenaki/"
					target="_blank"
					rel="noreferrer"
					class="underline">Maria Digenaki</a
				>.
			</p>
			<a href="https://hmu.gr" class="label hover:text-[var(--c-text)]"
				>Ελληνικό Μεσογειακό Πανεπιστήμιο</a
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
