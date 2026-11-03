import * as Sentry from '@sentry/svelte';
import type { HandleClientError } from '@sveltejs/kit/hooks';
import { config } from '#lib/config.ts';

Sentry.init({
	dsn: config.sentryDsn || undefined,
	enabled: Boolean(config.sentryDsn),
	integrations: [Sentry.browserTracingIntegration()],
	tracesSampleRate: 0.1
});

export const handleError: HandleClientError = ({ error }) => {
	console.error(error);
	Sentry.captureException(error);
};
