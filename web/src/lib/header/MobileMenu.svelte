<script lang="ts">
	import { resolve } from '$app/paths';
	import { withName } from '#lib/format.ts';
	import type { Snippet } from 'svelte';
	import { afterNavigate } from '$app/navigation';
	import { reduced, springDriver } from '#lib/motion.ts';
	import type { CurrentUser } from '#lib/types.ts';
	import Avatar from '#lib/ui/Avatar.svelte';
	import IssueKeyMenu from '#lib/ui/IssueKeyMenu.svelte';

	// The phone header: one "Menu" word that unfolds a sheet from under the bar. The sheet carries
	// the same actions as the desktop header and leaves the way it came: back up under the bar.

	let {
		user,
		brand,
		onunlock,
		onsettings,
		onlogout
	}: {
		user: CurrentUser;
		/** The header's brand link, so the bar inside the sheet matches the real one exactly. */
		brand: Snippet;
		onunlock: () => void;
		onsettings: () => void;
		onlogout: () => Promise<void>;
	} = $props();

	let trigger = $state<HTMLButtonElement>();
	let dialog = $state<HTMLDialogElement>();
	let panel = $state<HTMLElement>();
	let scrim = $state<HTMLElement>();
	/** The dialog is in the page (open, or still leaving). */
	let mounted = $state(false);
	/** Where the sheet is heading; drives the Menu/Close word. */
	let shown = $state(false);
	let leaving = $state(false);

	/** Returns focus here when a dialog opened from the menu closes. */
	export const focus = () => trigger?.focus();

	// 0 = tucked under the bar, 1 = open. Reduced motion keeps the sheet still and fades it.
	const sheet = springDriver((p) => {
		if (!panel || !scrim) return;
		scrim.style.opacity = String(Math.min(Math.max(p, 0), 1));
		if (reduced()) panel.style.opacity = String(Math.min(Math.max(p, 0), 1));
		else panel.style.transform = `translateY(${(p - 1) * panel.offsetHeight}px)`;
	});

	const open = () => {
		shown = true;
		if (mounted) sheet.to(1);
		else mounted = true; // the effect below shows it once it exists
	};

	const close = () => {
		shown = false;
		sheet.to(0, { onsettle: () => dialog?.close() });
	};

	$effect(() => {
		if (!dialog) return;
		dialog.showModal();
		sheet.set(0);
		sheet.to(1);
	});

	// The dialog really closed (spring settled, or the browser forced it): drop it from the page.
	const closed = () => {
		sheet.stop();
		mounted = shown = false;
	};

	// Back/forward or a link inside the sheet: tuck it away over the new page.
	afterNavigate(() => {
		if (shown) close();
	});

	// The menu only exists below md; widening the window past it closes the menu outright.
	$effect(() => {
		const wide = matchMedia('(min-width: 48rem)');
		const onchange = () => wide.matches && dialog?.close();
		wide.addEventListener('change', onchange);
		return () => wide.removeEventListener('change', onchange);
	});

	/** Close the sheet and open what was chosen at once; its own dialog rises over the leaving sheet. */
	const handOff = (action: () => void) => {
		close();
		action();
	};

	const logout = () => {
		leaving = true;
		onlogout().catch(() => (leaving = false));
	};

	// ---- swipe up to dismiss

	/** Apple's momentum projection: where a flick at `velocity` px/s would come to rest. */
	const project = (velocity: number, deceleration = 0.998) =>
		((velocity / 1000) * deceleration) / (1 - deceleration);
	/** Resistance past the open position: the further it goes, the less it follows. */
	const rubberband = (overshoot: number, size: number, c = 0.55) =>
		(overshoot * size * c) / (size + c * Math.abs(overshoot));

	let pointer: number | undefined;
	let startX = 0;
	let startY = 0;
	let dragging = false;
	let originY = 0;
	let originP = 0;
	let history: { y: number; t: number }[] = [];
	/** A drag that started on a row must not also press it. */
	let swallowClick = false;

	const onpointerdown = (e: PointerEvent) => {
		if (!panel?.contains(e.target as Node) || pointer !== undefined) return;
		pointer = e.pointerId;
		startX = e.clientX;
		startY = e.clientY;
		dragging = swallowClick = false;
	};

	const onpointermove = (e: PointerEvent) => {
		if (e.pointerId !== pointer || !panel) return;
		if (!dragging) {
			const dy = e.clientY - startY;
			// A little hysteresis so taps stay taps; only a mostly vertical move is a swipe.
			if (Math.abs(dy) < 8 || Math.abs(dy) < Math.abs(e.clientX - startX)) return;
			dragging = swallowClick = true;
			panel.setPointerCapture(e.pointerId);
			// Grab it wherever it is, even mid-flight.
			originY = e.clientY;
			originP = sheet.value;
			history = [];
		}
		const height = panel.offsetHeight;
		const raw = originP + (e.clientY - originY) / height;
		sheet.set(raw > 1 ? 1 + rubberband((raw - 1) * height, height) / height : raw);
		history.push({ y: e.clientY, t: e.timeStamp });
		while (history.length > 2 && e.timeStamp - history[0].t > 100) history.shift();
	};

	const onpointerup = (e: PointerEvent) => {
		if (e.pointerId !== pointer) return;
		pointer = undefined;
		if (!dragging || !panel) return;
		dragging = false;
		const first = history[0];
		const lastSample = history.at(-1);
		const dt = first && lastSample ? (lastSample.t - first.t) / 1000 : 0;
		const velocity = dt > 0 ? (lastSample!.y - first.y) / dt : 0; // px/s, negative is up
		const height = panel.offsetHeight;
		// Decide from where the flick is going, then carry its speed into the spring.
		const resting = sheet.value + project(velocity) / height;
		if (resting < 0.5) {
			shown = false;
			sheet.to(0, { velocity: velocity / height, onsettle: () => dialog?.close() });
		} else {
			shown = true;
			sheet.to(1, { velocity: velocity / height });
		}
	};

	const onclickcapture = (e: MouseEvent) => {
		if (!swallowClick) return;
		swallowClick = false;
		e.preventDefault();
		e.stopPropagation();
	};
</script>

<button
	bind:this={trigger}
	type="button"
	onclick={open}
	aria-haspopup="dialog"
	aria-expanded={mounted}
	aria-controls="mobile-menu"
	class="btn w-[4.75rem]">Menu</button
>

{#if mounted}
	<dialog
		bind:this={dialog}
		id="mobile-menu"
		aria-label="Menu"
		oncancel={(e) => {
			e.preventDefault();
			close();
		}}
		onclose={closed}
		onclick={(e) => e.target === scrim && close()}
		{onpointerdown}
		{onpointermove}
		{onpointerup}
		onpointercancel={onpointerup}
		{onclickcapture}
		class="m-0 h-dvh max-h-none w-full max-w-none touch-none overflow-hidden overscroll-contain bg-transparent p-0 text-ink backdrop:bg-transparent"
	>
		<div
			bind:this={scrim}
			aria-hidden="true"
			class="absolute inset-0 bg-black/70"
			style="opacity: 0"
		></div>

		<!-- Sits exactly over the real header, so "Menu" simply becomes "Close" in place. -->
		<div class="bar full relative z-10">
			{@render brand()}
			<div class="grow"></div>
			<span class="only-sm">
				<button
					type="button"
					onclick={() => (shown ? close() : open())}
					aria-label={shown ? 'Close menu' : 'Open menu'}
					class="btn w-[4.75rem]">{shown ? 'Close' : 'Menu'}</button
				>
			</span>
		</div>

		<!-- The void above the panel extends upward, so pulling it down past open never shows a gap. -->
		<div
			bind:this={panel}
			class="relative border-b bg-void before:absolute before:inset-x-0 before:bottom-full before:h-40 before:bg-void"
			style={reduced() ? 'opacity: 0' : 'transform: translateY(-100%)'}
		>
			<button
				type="button"
				onclick={() => handOff(onsettings)}
				class="flex w-full items-center gap-3 px-4 py-4 text-left active:bg-paper-2"
			>
				<Avatar email={user.email} size={40} />
				<span class="flex min-w-0 flex-1 flex-col">
					<span class="truncate font-mono font-bold">{user.nickname}</span>
					<span class="truncate text-gray-400">{withName(user)}</span>
				</span>
				<span class="shrink-0 text-gray-400">Settings</span>
			</button>

			{#if user.canUnlock || user.isAdmin}
				<div
					class="grid gap-2 border-t px-4 py-4 {user.canUnlock && user.isAdmin
						? 'grid-cols-2'
						: ''}"
				>
					{#if user.canUnlock}
						<button type="button" onclick={() => handOff(onunlock)} class="btn py-3 text-sm"
							>Remote unlock</button
						>
					{/if}
					{#if user.isAdmin}
						<IssueKeyMenu trigger="btn btn-primary py-3 text-sm" inline onnavigate={close} />
						<a href={resolve('/os/firmware')} onclick={close} class="btn py-3 text-center text-sm"
							>Firmware</a
						>
					{/if}
				</div>
			{/if}

			<button
				type="button"
				onclick={logout}
				disabled={leaving}
				class="flex w-full items-center border-t px-4 py-3.5 text-left text-gray-600 active:bg-paper-2 disabled:opacity-45"
				>{leaving ? 'Logging out…' : 'Log out'}</button
			>

			<div aria-hidden="true" class="flex justify-center pt-1 pb-2.5">
				<span class="h-1 w-9 rounded-sm bg-gray-300"></span>
			</div>
		</div>
	</dialog>
{/if}
