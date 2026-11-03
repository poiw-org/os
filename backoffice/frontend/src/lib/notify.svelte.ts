export type ToastTone = 'success' | 'error' | 'info';
export type Toast = { id: number; tone: ToastTone; message: string };

export type ConfirmOptions = {
	title: string;
	message?: string;
	confirmLabel?: string;
	cancelLabel?: string;
	/** `danger` paints the confirm button red; use it for destructive actions only. */
	tone?: 'default' | 'danger';
};

// Read by lib/ui/Notifications.svelte, which is mounted once in the root layout.
export const toasts = $state<Toast[]>([]);
export const confirmation = $state<{
	current?: ConfirmOptions & { resolve: (accepted: boolean) => void };
}>({});

let nextId = 0;

const dismiss = (id: number) => {
	const index = toasts.findIndex((toast) => toast.id === id);
	if (index >= 0) toasts.splice(index, 1);
};

const show = (tone: ToastTone, message: string) => {
	const id = nextId++;
	toasts.push({ id, tone, message });
	// Errors stay longer: they are the ones people need to read.
	setTimeout(() => dismiss(id), tone === 'error' ? 8000 : 4500);
};

export const toast = {
	success: (message: string) => show('success', message),
	error: (message: string) => show('error', message),
	info: (message: string) => show('info', message),
	dismiss
};

/** Replaces window.confirm(): resolves true on confirm, false on cancel, Escape or backdrop click. */
export const ask = (options: ConfirmOptions) =>
	new Promise<boolean>((resolve) => {
		// A second question supersedes the first.
		confirmation.current?.resolve(false);
		confirmation.current = { ...options, resolve };
	});
