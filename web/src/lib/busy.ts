import { config } from '#lib/config.ts';

// The loading cursor without any page knowing about it. One attribute on <html> that app.css turns
// into a cursor: "wait" until the app has started, "work" while a page load or a request to our own
// origin or Supabase has been pending for a moment (short ones would only flicker).
const SHOW_AFTER_MS = 200;

let booting = true;
let navigating = false;
let requests = 0;
let showing = false;
let timer: ReturnType<typeof setTimeout> | undefined;

const apply = () => {
	const root = document.documentElement;
	if (booting) root.dataset.busy = 'wait';
	else if (showing) root.dataset.busy = 'work';
	else delete root.dataset.busy;
};

const update = () => {
	const pending = navigating || requests > 0;
	if (!pending) {
		clearTimeout(timer);
		timer = undefined;
		showing = false;
		apply();
	} else if (!showing && timer === undefined) {
		timer = setTimeout(() => {
			timer = undefined;
			showing = navigating || requests > 0;
			apply();
		}, SHOW_AFTER_MS);
	}
};

/** The app has started: stop showing the startup cursor. */
export const ready = () => {
	booting = false;
	apply();
};

export const setNavigating = (value: boolean) => {
	navigating = value;
	update();
};

const origins = new Set<string>();
const ours = (input: RequestInfo | URL) => {
	try {
		const url = new URL(input instanceof Request ? input.url : String(input), location.href);
		return origins.has(url.origin);
	} catch {
		return false;
	}
};

/** Counts the app's own requests (not Sentry's) by wrapping fetch once. */
export const trackRequests = () => {
	origins.add(location.origin);
	try {
		origins.add(new URL(config.supabaseUrl).origin);
	} catch {
		// No valid Supabase URL: only same-origin requests count.
	}
	const original = window.fetch.bind(window);
	window.fetch = (input, init) => {
		if (!ours(input)) return original(input, init);
		requests++;
		update();
		return original(input, init).finally(() => {
			requests--;
			update();
		});
	};
};
