<script lang="ts">
	import { t } from '#lib/i18n.svelte.ts';
	import KeyInviteStage from '#lib/ui/KeyInviteStage.svelte';

	// Shown instead of a sign-in when an app is for people with lab access and this account has none.
	let { app }: { app: string } = $props();
</script>

<div class="flex flex-col gap-4" role="alert">
	<KeyInviteStage locked class="h-40 w-full overflow-hidden rounded-md border bg-paper-2 sm:h-48" />
	<p>
		{t(
			'We’d love to have you in {app}, but we can’t let you in just yet: it’s only for people who have access to the lab, and this account doesn’t.',
			{ app }
		)}
	</p>
	<p class="text-sm text-gray-400">
		{t(
			'Come say hi on Discord and the team will gladly sort you out. Once you have lab access, just come back and sign in again.'
		)}
	</p>
	<a
		href="https://discord.poiw.org"
		target="_blank"
		rel="noopener"
		class="btn btn-primary py-2.5 text-center">{t('Say hi on Discord ♥')}</a
	>
</div>
