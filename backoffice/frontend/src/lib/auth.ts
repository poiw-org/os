import { supabase } from '#lib/supabase.ts';
import type { CurrentUser } from '#lib/types.ts';

/**
 * The signed-in admin, or null when signed out or signed in without admin rights.
 * This only drives the UI: row-level security and the edge function enforce access.
 */
export async function getCurrentUser(): Promise<CurrentUser | null> {
	const {
		data: { session }
	} = await supabase.auth.getSession();
	if (!session) return null;

	const { data: admin, error } = await supabase
		.from('admins')
		.select('user_id')
		.eq('user_id', session.user.id)
		.maybeSingle();
	if (error) throw error;
	if (!admin) return null;

	const { data: profile } = await supabase
		.from('profiles')
		.select('nickname')
		.eq('id', session.user.id)
		.maybeSingle();

	const email = session.user.email ?? '';
	return { id: session.user.id, email, nickname: profile?.nickname ?? email };
}

export async function signIn(email: string, password: string) {
	const { error } = await supabase.auth.signInWithPassword({ email, password });
	if (error) throw error;
}

export async function signOut() {
	await supabase.auth.signOut();
}

export async function sendPasswordReset(email: string) {
	const { error } = await supabase.auth.resetPasswordForEmail(email, {
		redirectTo: `${window.location.origin}/reset-password`
	});
	if (error) throw error;
}

export async function hasSession() {
	const {
		data: { session }
	} = await supabase.auth.getSession();
	return Boolean(session);
}

export async function setPassword(password: string) {
	const { error } = await supabase.auth.updateUser({ password });
	if (error) throw error;
}

export async function setNickname(userId: string, nickname: string) {
	const { error } = await supabase.from('profiles').update({ nickname }).eq('id', userId);
	if (error) throw error;
}
