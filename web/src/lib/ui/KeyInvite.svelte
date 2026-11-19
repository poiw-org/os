<script lang="ts">
	import KeyInviteStage from '#lib/ui/KeyInviteStage.svelte';
	import { t } from '#lib/i18n.svelte.ts';

	// Shown to anyone with no active key: a warm nudge to the Discord, where the team hands them out.
	// The picture is KeyInviteStage: people wave beside a door that is slightly open.
</script>

<section
	class="flex flex-col overflow-hidden rounded-lg border bg-paper-2 sm:flex-row sm:items-stretch"
	aria-labelledby="key-invite-title"
>
	<KeyInviteStage class="h-44 w-full shrink-0 sm:h-auto sm:min-h-64 sm:w-[46%]" />
	<div class="flex flex-1 flex-col justify-center gap-3 p-5 sm:p-8">
		<p class="eyebrow">{t('Welcome to po/iw')}</p>
		<h2 id="key-invite-title" class="text-xl font-bold md:text-2xl">
			{t('Come say hi, and get your key')}
		</h2>
		<p class="max-w-prose leading-relaxed text-gray-400">
			{t(
				"You don't have an active key yet. Hop into our Discord, meet the people behind the lab and ask for one."
			)}
		</p>
		<div class="flex flex-wrap items-center gap-x-5 gap-y-2 pt-1">
			<a
				href="https://discord.poiw.org"
				target="_blank"
				rel="noopener"
				class="btn btn-primary px-5 py-2.5">{t('Join discord.poiw.org')} ♥</a
			>
			<span class="text-xs text-gray-400"
				>{t('Your key shows up here as soon as someone issues it.')}</span
			>
		</div>
	</div>
</section>
