import * as Sentry from '@sentry/svelte';
import type { HandleClientError } from '@sveltejs/kit/hooks';
import { config } from '#lib/config.ts';

// Auth email links land with the session in the URL fragment (#access_token=...&refresh_token=...)
// until supabase-js consumes it. Sentry keeps fragments, so drop them from every URL it records.
const stripFragment = (url: unknown) => {
	if (typeof url !== 'string') return url;
	const hash = url.indexOf('#');
	return hash === -1 ? url : url.slice(0, hash);
};
const URL_KEYS = ['url.full', 'http.url', 'url', 'from', 'to'];
const scrubData = (data: Record<string, unknown> | undefined) => {
	if (!data) return;
	for (const key of URL_KEYS) if (key in data) data[key] = stripFragment(data[key]);
};
const scrubEvent = <T extends Sentry.Event>(event: T): T => {
	if (event.request?.url) event.request.url = stripFragment(event.request.url) as string;
	scrubData(event.contexts?.trace?.data);
	for (const span of event.spans ?? []) scrubData(span.data);
	for (const crumb of event.breadcrumbs ?? []) scrubData(crumb.data);
	return event;
};

Sentry.init({
	dsn: config.sentryDsn || undefined,
	enabled: Boolean(config.sentryDsn),
	integrations: [Sentry.browserTracingIntegration()],
	tracesSampleRate: 0.1,
	beforeSend: scrubEvent,
	beforeSendTransaction: scrubEvent,
	beforeBreadcrumb: (crumb) => {
		scrubData(crumb.data);
		return crumb;
	}
});

export const handleError: HandleClientError = ({ error }) => {
	console.error(error);
	Sentry.captureException(error);
};
