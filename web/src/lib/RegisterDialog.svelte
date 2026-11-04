<script lang="ts">
	import { signUp } from '#lib/auth.ts';

	let dialog = $state<HTMLDialogElement>();
	let email = $state('');
	let password = $state('');
	let confirm = $state('');
	let message = $state('');
	let failed = $state(false);
	let busy = $state(false);

	export const open = () => {
		message = '';
		dialog?.showModal();
	};

	const report = (text: string, error = true) => {
		message = text;
		failed = error;
	};

	const submit = async (event: SubmitEvent) => {
		event.preventDefault();
		if (password !== confirm) return report('Οι κωδικοί δεν ταιριάζουν.');
		busy = true;
		try {
			// A session means the account is live: go straight to the questions.
			if (await signUp(email, password)) window.location.href = '/onboarding';
			else report('Έλεγξε το email σου για να επιβεβαιώσεις τον λογαριασμό, μετά συνδέσου.', false);
		} catch (e) {
			report(e instanceof Error ? e.message : 'Κάτι πήγε στραβά.');
		}
		busy = false;
	};
</script>

<dialog bind:this={dialog} aria-labelledby="register-title" class="register">
	<form onsubmit={submit} class="plate flex flex-col gap-5 p-6 sm:p-8">
		<div class="flex items-start justify-between gap-4">
			<h2 id="register-title" class="text-2xl">Γίνε μέλος</h2>
			<button type="button" class="key" onclick={() => dialog?.close()} aria-label="Κλείσιμο"
				>Esc</button
			>
		</div>
		<p class="mute text-sm">Φτιάξε λογαριασμό. Μετά θα σε ρωτήσουμε μερικά πράγματα για εσένα.</p>

		<label class="flex flex-col gap-1.5">
			<span class="label">Email</span>
			<input
				type="email"
				required
				autocomplete="username"
				placeholder="you@example.com"
				bind:value={email}
				class="slot"
			/>
		</label>
		<label class="flex flex-col gap-1.5">
			<span class="label">Κωδικός</span>
			<input
				type="password"
				required
				minlength="8"
				autocomplete="new-password"
				bind:value={password}
				class="slot"
			/>
		</label>
		<label class="flex flex-col gap-1.5">
			<span class="label">Επιβεβαίωση κωδικού</span>
			<input
				type="password"
				required
				minlength="8"
				autocomplete="new-password"
				bind:value={confirm}
				class="slot"
			/>
		</label>

		{#if message}
			<p
				role="alert"
				class="text-sm font-bold"
				style:color={failed ? 'var(--c-danger)' : 'inherit'}
			>
				{message}
			</p>
		{/if}

		<button type="submit" class="key key-signal" disabled={busy}>
			{busy ? 'Δημιουργία…' : 'Δημιουργία λογαριασμού'}
		</button>
	</form>
</dialog>

<style>
	dialog.register {
		margin: auto;
		width: min(26rem, calc(100vw - 2rem));
		max-height: calc(100vh - 2rem);
		overflow-y: auto;
		padding: 0;
		border: 0;
		background: transparent;
		color: inherit;
	}
	dialog.register::backdrop {
		background: oklch(10% 0.008 260 / 0.7);
	}
	dialog.register[open] {
		animation: pop 180ms var(--ease-out);
	}
	@keyframes pop {
		from {
			opacity: 0;
			transform: translateY(8px);
		}
	}
	@media (prefers-reduced-motion: reduce) {
		dialog.register[open] {
			animation: none;
		}
	}
</style>
