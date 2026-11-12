<script lang="ts">
	import { swapIn } from '#lib/motion.ts';

	// The app is not in the stores: every build is a GitHub release (.github/workflows/app-release.yml).
	const RELEASES = 'https://github.com/poiw-org/os/releases/latest';
	const APK = `${RELEASES}/download/doora.apk`;
	const IPA = `${RELEASES}/download/doora.ipa`;
	const youtube = (q: string) =>
		`https://www.youtube.com/results?search_query=${encodeURIComponent(q)}`;

	type Step = {
		title: string;
		body: string;
		path?: string[];
		link?: { href: string; label: string };
	};
	type Guide = {
		label: string;
		intro: string;
		steps: Step[];
		notes: string[];
		videos: { q: string; label: string }[];
	};

	const GUIDES: Record<'android' | 'ios', Guide> = {
		android: {
			label: 'Android',
			intro: 'About two minutes. You need the phone and nothing else.',
			steps: [
				{
					title: 'Download the APK',
					body: 'Open this page on the phone and tap the button. Chrome may say the file can be harmful; tap Download anyway.',
					link: { href: APK, label: 'Download doora.apk' }
				},
				{
					title: 'Allow your browser to install apps',
					body: 'Android 8 and newer ask for this once per app. Tapping the downloaded file usually takes you straight to the switch; if not, find it here and turn on Allow from this source for Chrome (or the Files app you open the APK with).',
					path: ['Settings', 'Apps', 'Special app access', 'Install unknown apps', 'Chrome']
				},
				{
					title: 'Install',
					body: 'Open the download and tap Install. If Play Protect warns that it does not recognise the developer, tap More details, then Install anyway.'
				},
				{
					title: 'Sign in',
					body: 'Open Doora and sign in with this account. Your virtual keys appear on their own.'
				}
			],
			notes: [
				'Android 7 and older: the switch is a single Unknown sources toggle under Settings, Security.',
				'Samsung puts it under Settings, Apps, the ⋮ menu, Special access, Install unknown apps. Xiaomi under Settings, Privacy protection, Special permissions.',
				'If Android asks you to confirm the developer is verified or to wait before installing, follow the on-screen steps; newer Android releases add this check for apps from outside Play.',
				'To update, install the newer APK over the old one. Your sign-in stays.'
			],
			videos: [
				{
					q: 'android install unknown apps allow from this source apk',
					label: 'Allowing an APK to install'
				},
				{ q: 'play protect install anyway unknown developer', label: 'Getting past Play Protect' }
			]
		},
		ios: {
			label: 'iPhone',
			intro:
				'About ten minutes the first time. You need a Mac or Windows PC, a cable and an Apple Account.',
			steps: [
				{
					title: 'Download the IPA on your computer',
					body: 'Save it somewhere you can find it, like Downloads.',
					link: { href: IPA, label: 'Download doora.ipa' }
				},
				{
					title: 'Install Sideloadly',
					body: 'Sideloadly signs the app with your own Apple Account so the iPhone accepts it. On Windows also install iTunes and iCloud from apple.com (not the Microsoft Store versions).',
					link: { href: 'https://sideloadly.io', label: 'Get Sideloadly' }
				},
				{
					title: 'Connect the iPhone',
					body: 'Plug it in with a cable, unlock it and tap Trust when it asks about this computer. It shows up in Sideloadly’s iDevice list.'
				},
				{
					title: 'Sideload',
					body: 'Drag doora.ipa onto the IPA icon in Sideloadly, type your Apple Account email and press Start. Enter the password (and the two-factor code) when asked. A burner Apple Account works fine.'
				},
				{
					title: 'Turn on Developer Mode',
					body: 'iOS 16 and newer only. Switch it on, let the iPhone restart, then tap Turn On after it unlocks.',
					path: ['Settings', 'Privacy & Security', 'Developer Mode']
				},
				{
					title: 'Trust your certificate',
					body: 'Tap your Apple Account under Developer App, then Trust. Now Doora opens; sign in with this account.',
					path: ['Settings', 'General', 'VPN & Device Management']
				}
			],
			notes: [
				'With a free Apple Account the app stops opening after 7 days. Plug in and press Start in Sideloadly again to refresh it; your data stays. Sideloadly can also refresh automatically over Wi-Fi when the computer is on.',
				'A free account can keep 3 sideloaded apps at a time.',
				'To update, sideload the newer IPA the same way.'
			],
			videos: [
				{ q: 'sideloadly tutorial install ipa iphone', label: 'Sideloadly, start to finish' },
				{
					q: 'iphone enable developer mode trust developer app',
					label: 'Developer Mode and trusting the app'
				}
			]
		}
	};

	const ua = typeof navigator === 'undefined' ? '' : navigator.userAgent;
	let platform = $state<'android' | 'ios'>(/iPhone|iPad|Macintosh/.test(ua) ? 'ios' : 'android');
	const guide = $derived(GUIDES[platform]);
</script>

<section class="flex flex-col gap-4" aria-labelledby="install-title">
	<div class="flex flex-wrap items-end justify-between gap-3">
		<div class="flex flex-col gap-1">
			<h2 id="install-title" class="text-lg font-bold">Get the app</h2>
			<p class="text-gray-400">Doora is not in the stores. Install the latest build from GitHub.</p>
		</div>
		<div class="flex gap-2" role="group" aria-label="Phone">
			{#each ['android', 'ios'] as const as p (p)}
				<button
					type="button"
					class="btn px-4 py-2 text-sm {platform === p ? 'btn-primary' : ''}"
					aria-pressed={platform === p}
					onclick={() => (platform = p)}>{GUIDES[p].label}</button
				>
			{/each}
		</div>
	</div>

	{#key platform}
		<div class="panel" in:swapIn>
			<p class="row text-gray-400">{guide.intro}</p>
			<ol>
				{#each guide.steps as step, i (step.title)}
					<li class="row items-start gap-4 py-5">
						<span class="step-n" aria-hidden="true">{i + 1}</span>
						<div class="flex min-w-0 flex-1 flex-col gap-2">
							<h3 class="font-bold">{step.title}</h3>
							<p class="leading-relaxed text-gray-400">{step.body}</p>
							{#if step.path}
								<p class="path" aria-label="Path: {step.path.join(', ')}">
									{#each step.path as part, j (part)}
										{#if j}<span class="text-gray-400" aria-hidden="true">›</span>{/if}<span
											>{part}</span
										>
									{/each}
								</p>
							{/if}
							{#if step.link}
								<a
									href={step.link.href}
									class="btn self-start px-4 py-2.5 text-sm {i === 0 ? 'btn-primary' : ''}"
									rel="noopener">{step.link.label}</a
								>
							{/if}
						</div>
					</li>
				{/each}
			</ol>

			<details class="row block">
				<summary class="cursor-pointer font-bold">If something looks different</summary>
				<ul class="mt-3 flex flex-col gap-2 text-gray-400">
					{#each guide.notes as note (note)}<li class="leading-relaxed">— {note}</li>{/each}
				</ul>
			</details>

			<div class="row flex-col items-stretch gap-2">
				<p class="eyebrow">Prefer a video</p>
				{#each guide.videos as v (v.q)}
					<a href={youtube(v.q)} target="_blank" rel="noopener" class="text-brand-text">
						{v.label} on YouTube ↗
					</a>
				{/each}
			</div>
		</div>
	{/key}

	<p class="text-xs text-gray-400">
		Every build is listed on <a href={RELEASES} class="text-brand-text" rel="noopener"
			>GitHub releases</a
		>.
	</p>
</section>

<style>
	.step-n {
		display: grid;
		place-items: center;
		width: 1.75rem;
		height: 1.75rem;
		flex-shrink: 0;
		border: 1px solid var(--color-brand);
		border-radius: 4px;
		color: var(--color-brand-text);
		font-weight: 700;
		font-size: 0.8125rem;
	}
	.path {
		display: flex;
		flex-wrap: wrap;
		gap: 0.375rem;
		align-self: flex-start;
		padding: 0.375rem 0.625rem;
		border: 1px solid var(--color-gray-200);
		border-radius: 4px;
		font-size: 0.8125rem;
	}
</style>
