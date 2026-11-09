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

	const [{ data: security }, { data: assurance }] = await Promise.all([
		supabase.rpc('security_status'),
		// Throws on a malformed token (a tampered link); the API rejects that session anyway.
		supabase.auth.mfa.getAuthenticatorAssuranceLevel().catch(() => ({ data: null }))
	]);
	const {
		has_factor: hasSecondFactor = false,
		required: secondFactorRequired = false,
		enforced = false,
		needs_email_code: needsEmailCode = false,
		suspends_at: suspendsAt
	} = (security ?? {}) as {
		has_factor?: boolean;
		required?: boolean;
		enforced?: boolean;
		needs_email_code?: boolean;
		suspends_at?: string | null;
	};
	// A passkey sign-in stays aal1 in Supabase but needs no code on top.
	const usedMethod = (method: string) =>
		assurance?.currentAuthenticationMethods.some(
			(entry) => (typeof entry === 'string' ? entry : entry.method) === method
		);
	const usedPasskey = usedMethod('passkey');

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
		hasSecondFactor,
		secondFactorRequired,
		secondFactorEnforced: enforced || (isAdmin && secondFactorRequired),
		keysSuspendAt: suspendsAt ? new Date(suspendsAt) : undefined,
		needsCode: assurance?.currentLevel === 'aal1' && assurance.nextLevel === 'aal2' && !usedPasskey,
		needsEmailCode,
		usedPassword: Boolean(usedMethod('password')),
		canUnlock: Boolean(canUnlock),
		pendingEmail: session.user.new_email,
		createdAt: new Date(session.user.created_at),
		lastSignInAt: session.user.last_sign_in_at ? new Date(session.user.last_sign_in_at) : undefined
	};
}

// Remembers the sign-in the 2FA prompt was closed for, so the next sign-in shows it again.
const PROMPT_KEY = 'second-factor-prompt-dismissed';
const signInStamp = (user: CurrentUser) => user.lastSignInAt?.toISOString() ?? '';

export function secondFactorPromptDismissed(user: CurrentUser) {
	try {
		return localStorage.getItem(PROMPT_KEY) === signInStamp(user);
	} catch {
		return false;
	}
}

export function dismissSecondFactorPrompt(user: CurrentUser) {
	try {
		localStorage.setItem(PROMPT_KEY, signInStamp(user));
	} catch {
		// Without storage the prompt simply comes back on the next page load.
	}
}

export async function signIn(email: string, password: string, captchaToken: string) {
	const { error } = await supabase.auth.signInWithPassword({
		email,
		password,
		options: { captchaToken }
	});
	if (error) throw error;
}

/**
 * WebAuthn sign-in with a passkey the user registered from the settings modal. Takes no captcha:
 * Safari only opens the prompt shortly after the tap, and waiting for a token outlasts that.
 */
export async function signInWithPasskey(mediation?: 'conditional') {
	const { error } = await supabase.auth.signInWithPasskey({ options: { mediation } });
	if (error) throw error;
}

/**
 * Whether this email should be offered a passkey. A failed lookup answers false, so the password
 * form still works when the check is down.
 */
export async function hasPasskey(email: string, captchaToken: string) {
	const { data, error } = await supabase.functions.invoke('sign-in-method', {
		body: { email, captcha_token: captchaToken }
	});
	return !error && data?.passkey === true;
}

/** Emails the code for the second step of a password sign-in; never creates an account. */
export async function sendEmailCode(email: string, captchaToken: string) {
	const { error } = await supabase.auth.signInWithOtp({
		email,
		options: { captchaToken, shouldCreateUser: false }
	});
	if (error) throw error;
}

export async function confirmEmailCode(code: string) {
	const { error } = await supabase.functions.invoke('confirm-email-code', { body: { code } });
	if (!error) return;
	const reason = await (error as { context?: Response }).context?.json().catch(() => null);
	throw new Error(reason?.error ?? 'Could not verify the code.');
}

/** Whether the browser or a password manager can offer passkeys in the email field's autofill. */
export const passkeyAutofillSupported = async () =>
	passkeysSupported() && (await PublicKeyCredential.isConditionalMediationAvailable?.()) === true;

export const passkeysSupported = () =>
	typeof window !== 'undefined' && 'PublicKeyCredential' in window;

export async function registerPasskey() {
	const { error } = await supabase.auth.registerPasskey();
	if (error) throw error;
	await restoreKeys();
}

/** A suspended member gets their keys back the moment they register a factor. */
async function restoreKeys() {
	const { error } = await supabase.rpc('restore_my_keys');
	if (error) throw error;
}

export async function listTotpFactors() {
	const { data, error } = await supabase.auth.mfa.listFactors();
	if (error) throw error;
	return data.totp;
}

/** Starts authenticator enrollment, clearing any attempt that was abandoned before its code. */
export async function beginTotp() {
	const { data: factors, error: listError } = await supabase.auth.mfa.listFactors();
	if (listError) throw listError;
	for (const factor of factors.all.filter((f) => f.status === 'unverified')) {
		const { error } = await supabase.auth.mfa.unenroll({ factorId: factor.id });
		if (error) throw error;
	}
	const { data, error } = await supabase.auth.mfa.enroll({
		factorType: 'totp',
		friendlyName: 'Authenticator'
	});
	if (error) throw error;
	return { factorId: data.id, qrCode: data.totp.qr_code, secret: data.totp.secret };
}

export async function confirmTotp(factorId: string, code: string) {
	const { error } = await supabase.auth.mfa.challengeAndVerify({ factorId, code });
	if (error) throw error;
	await restoreKeys();
}

export async function removeTotp(factorId: string) {
	const { error } = await supabase.auth.mfa.unenroll({ factorId });
	if (error) throw error;
}

/** The second step of a password sign-in. */
export async function verifyTotp(code: string) {
	const [factor] = await listTotpFactors();
	if (!factor) throw new Error('No authenticator is registered.');
	const { error } = await supabase.auth.mfa.challengeAndVerify({ factorId: factor.id, code });
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
 * Creates an account with the questionnaire answers, which the database stores with the new profile.
 * Returns true when Supabase signed it in right away, false when it first waits for the email
 * confirmation.
 */
export async function signUp(
	email: string,
	password: string,
	a: OnboardingAnswers,
	captchaToken: string
) {
	const { data, error } = await supabase.auth.signUp({
		email,
		password,
		options: {
			captchaToken,
			emailRedirectTo: `${window.location.origin}/security`,
			data: {
				onboarding: {
					first_name: a.firstName,
					last_name: a.lastName,
					studies: a.studies,
					department: a.studies ? a.department : null,
					referral_source: a.referralSource,
					diy_familiar: a.diyFamiliar
				}
			}
		}
	});
	if (error) throw error;
	// With email confirmation on, Supabase hides a taken address behind a user with no identities.
	if (data.user?.identities?.length === 0)
		throw new Error('Υπάρχει ήδη λογαριασμός με αυτό το email.');
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

export async function sendPasswordReset(email: string, captchaToken: string) {
	const { error } = await supabase.auth.resetPasswordForEmail(email, {
		captchaToken,
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

/** Stored instead of asking members who had an account before the questionnaire existed. */
export const EXISTING_MEMBER_SOURCE = 'existing member before questionaire';

export type OnboardingAnswers = {
	firstName: string;
	lastName: string;
	studies: boolean;
	department: string;
	referralSource:
		'friend' | 'welcome_day' | 'newsletter' | 'campaign' | typeof EXISTING_MEMBER_SOURCE;
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
