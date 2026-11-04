import { supabase } from '#lib/supabase.ts';
import type { CurrentUser } from '#lib/types.ts';

/**
 * The signed-in user, or null when signed out. This only drives the UI: row-level security and
 * the edge functions enforce access.
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
	const isAdmin = Boolean(admin);

	const { data: canUnlock } = isAdmin ? { data: true } : await supabase.rpc('can_unlock');

	const { data: profile } = await supabase
		.from('profiles')
		.select('nickname, onboarded_at, first_name, last_name')
		.eq('id', session.user.id)
		.maybeSingle();

	const email = session.user.email ?? '';
	return {
		id: session.user.id,
		email,
		nickname: profile?.nickname ?? email,
		isAdmin,
		fullName: profile?.onboarded_at
			? `${profile.first_name ?? ''} ${profile.last_name ?? ''}`.trim()
			: '',
		onboarded: Boolean(profile?.onboarded_at),
		canUnlock: Boolean(canUnlock),
		pendingEmail: session.user.new_email,
		createdAt: new Date(session.user.created_at),
		lastSignInAt: session.user.last_sign_in_at ? new Date(session.user.last_sign_in_at) : undefined
	};
}

export async function signIn(email: string, password: string) {
	const { error } = await supabase.auth.signInWithPassword({ email, password });
	if (error) throw error;
}

/** WebAuthn sign-in with a passkey the user registered from the settings modal. */
export async function signInWithPasskey() {
	const { error } = await supabase.auth.signInWithPasskey();
	if (error) throw error;
}

export const passkeysSupported = () =>
	typeof window !== 'undefined' && 'PublicKeyCredential' in window;

export async function registerPasskey() {
	const { error } = await supabase.auth.registerPasskey();
	if (error) throw error;
}

export async function listPasskeys() {
	const { data, error } = await supabase.auth.passkey.list();
	if (error) throw error;
	return data;
}

export async function deletePasskey(passkeyId: string) {
	const { error } = await supabase.auth.passkey.delete({ passkeyId });
	if (error) throw error;
}

/**
 * Creates an account. Returns true when Supabase signed it in right away, false when it first
 * waits for the email confirmation.
 */
export async function signUp(email: string, password: string) {
	const { data, error } = await supabase.auth.signUp({
		email,
		password,
		options: { emailRedirectTo: `${window.location.origin}/onboarding` }
	});
	if (error) throw error;
	return Boolean(data.session);
}

/** `everywhere` also revokes the sessions on the user's other devices. */
export async function signOut(everywhere = false) {
	await supabase.auth.signOut({ scope: everywhere ? 'global' : 'local' });
}

/** Supabase mails a confirmation link; the address changes once it is opened. */
export async function setEmail(email: string) {
	const { error } = await supabase.auth.updateUser(
		{ email },
		{ emailRedirectTo: `${window.location.origin}/` }
	);
	if (error) throw error;
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

export type OnboardingAnswers = {
	firstName: string;
	lastName: string;
	studies: boolean;
	department: string;
	referralSource: 'friend' | 'welcome_day' | 'newsletter' | 'campaign';
	diyFamiliar: boolean;
};

/** Saves the answers and marks the profile onboarded; the database rejects incomplete ones. */
export async function completeOnboarding(userId: string, a: OnboardingAnswers) {
	const { error } = await supabase
		.from('profiles')
		.update({
			first_name: a.firstName.trim(),
			last_name: a.lastName.trim(),
			studies: a.studies,
			department: a.studies ? a.department.trim() : null,
			referral_source: a.referralSource,
			diy_familiar: a.diyFamiliar,
			onboarded_at: new Date().toISOString()
		})
		.eq('id', userId);
	if (error) throw error;
}
