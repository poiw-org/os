// Tells the login page whether to ask this email for a passkey. It answers nothing else, so an
// unknown address and an account that signs in with a password look the same.
//
// POST { email, captcha_token } -> { passkey }
//
// Secret: TURNSTILE_SECRET_KEY. verify_jwt is off at the gateway because visitors are signed out.
import { env, HttpError, handlePost, serviceClient } from '../_shared/http.ts';

async function captchaPassed(token: string) {
	const response = await fetch('https://challenges.cloudflare.com/turnstile/v0/siteverify', {
		method: 'POST',
		body: new URLSearchParams({ secret: env('TURNSTILE_SECRET_KEY'), response: token })
	});
	if (!response.ok) return false;
	return (await response.json()).success === true;
}

Deno.serve((req) =>
	handlePost(req, async ({ email, captcha_token: token }) => {
		if (typeof email !== 'string' || typeof token !== 'string')
			throw new HttpError(400, 'Email and captcha are required');
		if (!(await captchaPassed(token))) throw new HttpError(403, 'The captcha failed');

		const { data, error } = await serviceClient().rpc('should_offer_passkey', { address: email });
		if (error) {
			console.error(error);
			throw new HttpError(500, 'The lookup failed');
		}
		return { passkey: data === true };
	})
);
