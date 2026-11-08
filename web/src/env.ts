import { defineEnvVars } from '@sveltejs/kit/env';

// All values are public and inlined at build time. Values live in .env.production / .env.development.
export const variables = defineEnvVars({
	PUBLIC_SUPABASE_URL: { public: true, static: true },
	// The publishable (anon) key: safe in the browser, access is enforced by row-level security.
	PUBLIC_SUPABASE_PUBLISHABLE_KEY: { public: true, static: true },
	// Cloudflare Turnstile widget; the secret half is set in Supabase Auth.
	PUBLIC_TURNSTILE_SITE_KEY: { public: true, static: true },
	// Empty disables Sentry.
	PUBLIC_SENTRY_DSN: { public: true, static: true }
});
