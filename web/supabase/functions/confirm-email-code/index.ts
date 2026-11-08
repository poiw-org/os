// Second step of a password sign-in for an account with a passkey: proves the caller also controls
// the inbox. The code is one Supabase emailed via signInWithOtp, so it is checked here against a
// separate session, which is then discarded; the caller keeps their password session, now recorded
// as verified.
//
// POST { code } with the password session's token -> { verified }
//
// verify_jwt is off at the gateway because the token is checked here.
import { accessToken, callerId, HttpError, handlePost, serviceClient } from '../_shared/http.ts';

type Claims = { session_id?: string; amr?: { method: string }[] };

const claims = (token: string): Claims =>
	JSON.parse(atob(token.split('.')[1].replace(/-/g, '+').replace(/_/g, '/')));

Deno.serve((req) =>
	handlePost(req, async ({ code }) => {
		if (typeof code !== 'string' || !code) throw new HttpError(400, 'The code is required');

		const db = serviceClient();
		const token = accessToken(req);
		const userId = await callerId(db, token); // validates the token the claims are read from
		const { session_id: sessionId, amr = [] } = claims(token);
		// Without this, two emails (a sign-in code and this code) would stand in for the password.
		if (!sessionId || !amr.some((entry) => entry.method === 'password'))
			throw new HttpError(403, 'Sign in with your password first');

		const { data: caller, error: callerError } = await db.auth.admin.getUserById(userId);
		if (callerError || !caller.user.email) throw new HttpError(500, 'The lookup failed');

		// Its own client: verifying signs it in, and `db` must stay the service role.
		const verifier = serviceClient();
		const { data, error } = await verifier.auth.verifyOtp({
			email: caller.user.email,
			token: code,
			type: 'email'
		});
		if (error || !data.session || data.user?.id !== userId)
			throw new HttpError(400, 'That code is wrong or has expired');

		const { error: signOutError } = await db.auth.admin.signOut(data.session.access_token, 'local');
		if (signOutError) console.error(signOutError);

		const { error: markError } = await db.rpc('mark_session_email_verified', {
			session: sessionId,
			target: userId
		});
		if (markError) {
			console.error(markError);
			throw new HttpError(500, 'Could not record the verification');
		}
		return { verified: true };
	})
);
