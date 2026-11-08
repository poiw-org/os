import { redirect } from '@sveltejs/kit';
import { getCurrentUser } from '#lib/auth.ts';
import { openedRecoveryLink } from '#lib/supabase.ts';
import type { LayoutLoad } from './$types';

// Client-only app: no SSR. Routes are still prerendered as empty shells so static hosting
// has a real file for every path (/os, /os/keys/create, ...). The :id pages are not prerendered;
// the host serves the app shell for them.
export const ssr = false;
export const prerender = true;

const publicRoutes = ['/', '/login', '/reset-password', '/onboarding'];
// Only admins manage users and keys; members see their own keys at /os/keys.
const adminRoutes = ['/os/users', '/os/keys/create'];

// The one auth guard: every other route requires a signed-in user.
export const load: LayoutLoad = async ({ url }) => {
	const user = await getCurrentUser();

	// If the redirect URL is not allow-listed in Supabase, the recovery link lands on the Site URL
	// instead of /reset-password; send the user to the new-password form either way.
	if (openedRecoveryLink && url.pathname !== '/reset-password') redirect(307, '/reset-password');

	if (!user && !publicRoutes.includes(url.pathname)) redirect(307, '/login');
	if (user && url.pathname === '/login') redirect(307, '/os');
	const inApp = url.pathname.startsWith('/os') || url.pathname === '/security';
	// A password sign-in is not finished until the authenticator code is in.
	if (user?.needsCode && inApp) redirect(307, '/verify');
	if (user && !user.needsCode && url.pathname === '/verify') redirect(307, '/os');
	// Everyone answers the onboarding questions once, before using the OS.
	if (user && !user.onboarded && inApp) redirect(307, '/onboarding');
	if (user?.onboarded && url.pathname === '/onboarding') redirect(307, '/os');
	// Admins must have 2FA to use the app. Members get a prompt at each sign-in instead, and the
	// daily job suspends their keys if they never act on it.
	if (user?.isAdmin && user.secondFactorRequired && url.pathname.startsWith('/os'))
		redirect(307, '/security');
	if (user?.hasSecondFactor && url.pathname === '/security') redirect(307, '/os');
	if (user && !user.isAdmin) {
		if (url.pathname === '/os') redirect(307, '/os/keys');
		if (adminRoutes.some((route) => url.pathname.startsWith(route))) redirect(307, '/os/keys');
	}

	return { user };
};
