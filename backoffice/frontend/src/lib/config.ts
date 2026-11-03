import {
	PUBLIC_SENTRY_DSN,
	PUBLIC_SUPABASE_PUBLISHABLE_KEY,
	PUBLIC_SUPABASE_URL
} from '$app/env/public';

export const config = {
	supabaseUrl: PUBLIC_SUPABASE_URL,
	supabasePublishableKey: PUBLIC_SUPABASE_PUBLISHABLE_KEY,
	sentryDsn: PUBLIC_SENTRY_DSN
};
