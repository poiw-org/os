// The daily second-factor job, called by pg_cron (see the security_onboarding migration). The rules
// live in public.security_sweep(); this function decides when to run it and sends the emails.
//
// POST {} with x-cron-secret -> { ran, sent, failed }
//
// Secrets: SECURITY_CRON_SECRET, RESEND_API_KEY, EMAIL_FROM (a sender on a domain verified in
// Resend), APP_URL. verify_jwt is off at the gateway because the secret is checked here.
import { env, HttpError, handlePost, serviceClient } from '../_shared/http.ts';
import { message, type Due } from './email.ts';

const athensHour = () =>
	Number(
		new Intl.DateTimeFormat('en-GB', {
			timeZone: 'Europe/Athens',
			hour: 'numeric',
			hourCycle: 'h23'
		}).format(new Date())
	);

async function send(to: string, content: { subject: string; html: string }) {
	const response = await fetch('https://api.resend.com/emails', {
		method: 'POST',
		headers: {
			Authorization: `Bearer ${env('RESEND_API_KEY')}`,
			'Content-Type': 'application/json'
		},
		body: JSON.stringify({ from: env('EMAIL_FROM'), to, ...content })
	});
	if (!response.ok) throw new Error(`Resend answered ${response.status}`);
}

Deno.serve((req) =>
	handlePost(req, async () => {
		if (req.headers.get('x-cron-secret') !== env('SECURITY_CRON_SECRET'))
			throw new HttpError(401, 'Invalid secret');
		if (athensHour() !== 12) return { ran: false, sent: 0, failed: 0 };

		// Read before the sweep: a missing setting must not suspend keys with nobody told.
		env('RESEND_API_KEY');
		env('EMAIL_FROM');
		const appUrl = env('APP_URL');

		const db = serviceClient();
		const { data, error } = await db.rpc('security_sweep');
		if (error) {
			console.error(error);
			throw new HttpError(500, 'The sweep failed');
		}

		let sent = 0;
		let failed = 0;
		for (const due of (data ?? []) as Due[]) {
			try {
				await send(due.email, message(due, appUrl));
				sent++;
				if (due.kind === 'reminder') {
					const { error: stampError } = await db
						.from('profiles')
						.update({ security_reminded_at: new Date().toISOString() })
						.eq('id', due.user_id);
					if (stampError) console.error(stampError);
				}
			} catch (e) {
				failed++;
				console.error(due.user_id, e);
			}
			// Resend allows a few requests per second.
			await new Promise((resolve) => setTimeout(resolve, 600));
		}
		return { ran: true, sent, failed };
	})
);
