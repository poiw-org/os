import { redirect } from '@sveltejs/kit';
import { getCurrentUser } from '#lib/auth.ts';
import { openedRecoveryLink } from '#lib/supabase.ts';
import type { LayoutLoad } from './$types';

// Client-only app: no SSR. Routes are still prerendered as empty shells so static hosting
// has a real file for every path (/items, /items/create, ...).
export const ssr = false;
export const prerender = true;

const publicRoutes = ['/login', '/reset-password'];
// Only admins manage users and keys; members see their own keys at /keys.
const adminRoutes = ['/users', '/items/create', '/items/renew'];

// The one auth guard: every other route requires a signed-in user.
export const load: LayoutLoad = async ({ url }) => {
	const user = await getCurrentUser();

	// If the redirect URL is not allow-listed in Supabase, the recovery link lands on the Site URL
	// instead of /reset-password; send the user to the new-password form either way.
	if (openedRecoveryLink && url.pathname !== '/reset-password') redirect(307, '/reset-password');

	if (!user && !publicRoutes.includes(url.pathname)) redirect(307, '/login');
	if (user && url.pathname === '/login') redirect(307, '/');
	if (user && !user.isAdmin) {
		if (url.pathname === '/') redirect(307, '/keys');
		if (adminRoutes.some((route) => url.pathname.startsWith(route))) redirect(307, '/keys');
	}

	return { user };
};
