<script lang="ts">
	import { page } from '$app/state';
	import { goto } from '$app/navigation';
	import { resolve } from '$app/paths';
	import { onMount } from 'svelte';
	import { DateInput } from 'date-picker-svelte';
	import { issueKey, listUsers } from '#lib/api.ts';
	import { nfcSupported, scanTag } from '#lib/nfc.ts';
	import { toast } from '#lib/notify.svelte.ts';
	import { canonicalSerial } from '#lib/serial.ts';
	import WriteCardModal from '#lib/header/WriteCardModal.svelte';
	import Loading from '#lib/loading.svelte';
	import type { UserOption } from '#lib/types.ts';
	import Avatar from '#lib/ui/Avatar.svelte';
	import Breadcrumb from '#lib/ui/Breadcrumb.svelte';
	import UserPicker from '#lib/ui/UserPicker.svelte';

	const DAY = 86_400_000;
	const addMonths = (from: Date, months: number) =>
		new Date(from.getFullYear(), from.getMonth() + months, from.getDate());
	const presets = [
		{ label: '1 month', months: 1 },
		{ label: '6 months', months: 6 },
		{ label: '1 year', months: 12 },
		{ label: '5 years', months: 60 }
	];

	let users = $state<UserOption[]>();
	let user = $state<UserOption>();
	let serialNumber = $state('');
	// A virtual key has no tag: it only unlocks the door remotely.
	let virtual = $state(false);
	let notBefore = $state(new Date());
	let expiresAt = $state(addMonths(new Date(), 12));
	let scanning = $state(false);
	let processing = $state(false);
	let doorScan = $state(false);
	let stopScan: (() => void) | undefined;

	// Whole calendar days, so the time of day on "now" does not skew the count.
	const startOfDay = (date: Date) =>
		new Date(date.getFullYear(), date.getMonth(), date.getDate()).getTime();
	const days = $derived(Math.round((startOfDay(expiresAt) - startOfDay(notBefore)) / DAY));
	const validRange = $derived(days > 0);
	const day = (date: Date) => date.toLocaleDateString('en-GB', { dateStyle: 'medium' });

	const submit = async (event: SubmitEvent) => {
		event.preventDefault();
		if (!user) {
			toast.error('Select a user first.');
			return;
		}
		if (!validRange) {
			toast.error('The key must expire after it activates.');
			return;
		}
		// One tag, one spelling: "04A2B3C4" and "04:a2:b3:c4" must not become two keys.
		const serial = virtual ? undefined : canonicalSerial(serialNumber);
		if (!virtual && !serial) {
			toast.error('The S/N must be a 4, 7 or 10 byte tag ID in hex, e.g. 04:a2:b3:c4.');
			return;
		}
		processing = true;
		try {
			const { id } = await issueKey({
				userId: user.value,
				serialNumber: serial,
				notBefore,
				expiresAt
			});
			toast.success(`Key issued for ${user.label}.`);
			await goto(resolve(`/os/items?id=${id}`));
		} catch (e) {
			toast.error(
				e instanceof Error ? e.message : 'An error occured while communicating with the server'
			);
			processing = false;
		}
	};

	const scan = async () => {
		if (scanning) return stopScan?.();
		scanning = true;
		stopScan = await scanTag(
			(serial, _ndef, stop) => {
				serialNumber = serial;
				toast.success('Tag read.');
				stop();
			},
			() => (scanning = false)
		);
	};

	onMount(() => {
		(async () => {
			try {
				users = await listUsers();
				// Arriving from a user's page ("Issue key") pre-selects that user.
				user = users.find((option) => option.value === page.url.searchParams.get('user'));
			} catch (e) {
				toast.error(e instanceof Error ? e.message : 'Could not load the users.');
				users = [];
			}
		})();
		return () => stopScan?.();
	});
</script>

<svelte:head>
	<title>Issue a key</title>
</svelte:head>

<div class="flex flex-col gap-6 p-4 md:p-8">
	<div class="flex flex-col gap-3">
		<Breadcrumb crumbs={[{ label: 'Users', href: '/os' }, { label: 'Issue a key' }]} />
		<div class="flex flex-col gap-1">
			<h1 class="text-xl font-bold">Issue a key</h1>
			<p class="text-gray-400">Bind a new key to a user and a physical tag, or make it virtual.</p>
		</div>
	</div>

	{#if users}
		<form
			id="issue"
			onsubmit={submit}
			class="grid items-start gap-6 lg:grid-cols-[minmax(0,1fr)_340px]"
		>
			<div class="panel">
				<section class="flex gap-4 p-5 md:p-6">
					<div class="flex min-w-0 flex-1 flex-col gap-4">
						<div>
							<h2 class="font-bold">Who is it for?</h2>
							<p class="text-gray-400">Only users who are not blocked can hold a key.</p>
						</div>
						<UserPicker {users} bind:value={user} />
					</div>
				</section>

				<section class="flex gap-4 border-t p-5 md:p-6">
					<div class="flex min-w-0 flex-1 flex-col gap-4">
						<div>
							<h2 class="font-bold">Which tag?</h2>
							<p class="text-gray-400">
								{virtual
									? 'A virtual key has no tag. It only unlocks the door remotely.'
									: 'The key is bound to this serial number. It cannot be written to another tag.'}
							</p>
						</div>
						<label class="flex items-center gap-2 text-sm">
							<input type="checkbox" bind:checked={virtual} />Virtual key (no tag)
						</label>
						{#if !virtual}
							<div class="flex flex-wrap items-center gap-3">
								<label
									class="flex min-w-0 flex-1 basis-56 items-center gap-3 rounded-lg border bg-paper-2 pl-3.5 text-gray-400 transition-colors focus-within:border-brand-text"
								>
									<input
										required
										placeholder="Tag S/N"
										aria-label="Tag S/N"
										autocomplete="off"
										spellcheck="false"
										bind:value={serialNumber}
										maxlength="64"
										class="w-full border-0! bg-transparent! p-3 font-mono text-base text-ink shadow-none! outline-none!"
									/>
								</label>
								{#if nfcSupported}
									<button type="button" onclick={scan} class="btn py-3">
										{#if scanning}
											Hold a tag to the phone
										{:else}
											Scan tag
										{/if}
									</button>
								{/if}
								<button type="button" onclick={() => (doorScan = true)} class="btn py-3">
									Scan at door
								</button>
							</div>
						{/if}
					</div>
				</section>

				<section class="flex gap-4 border-t p-5 md:p-6">
					<div class="flex min-w-0 flex-1 flex-col gap-4">
						<div>
							<h2 class="font-bold">When is it valid?</h2>
							<p class="text-gray-400">Outside this window the door refuses the key.</p>
						</div>
						<div class="flex flex-wrap gap-4">
							<div class="flex flex-col gap-2">
								<span class="eyebrow">Activate not before</span>
								<DateInput
									format="yyyy-MM-dd"
									min={new Date()}
									max={new Date('2100')}
									bind:value={notBefore}
								/>
							</div>
							<div class="flex flex-col gap-2">
								<span class="eyebrow">Expires at</span>
								<DateInput
									format="yyyy-MM-dd"
									min={new Date()}
									max={new Date('2100')}
									bind:value={expiresAt}
								/>
							</div>
						</div>
						<div class="flex flex-wrap items-center gap-2">
							<span class="eyebrow mr-1">Valid for</span>
							{#each presets as preset (preset.months)}
								{@const target = addMonths(notBefore, preset.months)}
								<button
									type="button"
									onclick={() => (expiresAt = target)}
									aria-pressed={target.toDateString() === expiresAt.toDateString()}
									class="chip">{preset.label}</button
								>
							{/each}
						</div>
						{#if !validRange}
							<p class="text-red-500" role="status">The key must expire after it activates.</p>
						{/if}
					</div>
				</section>
			</div>

			<aside class="panel flex flex-col lg:sticky lg:top-20">
				<div class="flex flex-col gap-4 p-5">
					<h2 class="font-bold">Summary</h2>
					{#if user}
						<div class="flex items-center gap-3">
							<Avatar email={user.label} size={40} />
							<span class="min-w-0 truncate">{user.label}</span>
						</div>
					{:else}
						<p class="text-gray-400">Pick a user to get started.</p>
					{/if}
					<dl class="grid grid-cols-[auto_1fr] gap-x-6 gap-y-2">
						<dt class="text-gray-400">Tag</dt>
						<dd class="truncate text-right font-mono">
							{virtual ? 'Virtual' : serialNumber || '—'}
						</dd>
						<dt class="text-gray-400">From</dt>
						<dd class="text-right">{day(notBefore)}</dd>
						<dt class="text-gray-400">Until</dt>
						<dd class="text-right">{day(expiresAt)}</dd>
						<dt class="text-gray-400">Length</dt>
						<dd class="text-right">{validRange ? `${days} days` : '—'}</dd>
					</dl>
				</div>
				<div class="border-t p-5">
					<button type="submit" disabled={processing} class="btn btn-primary w-full py-3">
						{#if processing}
							Issuing…
						{:else}
							Issue key
						{/if}
					</button>
				</div>
			</aside>
		</form>
	{:else}
		<span class="flex pb-20"><Loading /></span>
	{/if}
</div>

{#if doorScan}
	<WriteCardModal
		onclose={() => (doorScan = false)}
		onscanned={(serial) => {
			serialNumber = serial;
			toast.success('Tag read at the door.');
		}}
	/>
{/if}
