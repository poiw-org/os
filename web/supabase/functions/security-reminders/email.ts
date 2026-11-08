// The second-factor emails, in the layout of supabase/templates/*.html so every message from the app
// looks alike. Colours are the hex forms of the tokens listed in those templates, because email
// clients do not understand oklch.

export type Due = {
	user_id: string;
	email: string;
	kind: 'reminder' | 'suspended';
	days_left: number;
};

const SANS = "'Space Grotesk',Helvetica,Arial,sans-serif";
const MONO = "'Space Mono','Courier New',Courier,monospace";

const escapeHtml = (text: string) => text.replace(/[&<>"']/g, (c) => `&#${c.charCodeAt(0)};`);

function page(
	due: Due,
	appUrl: string,
	content: { title: string; body: string; note: string; lamp: string }
) {
	const { title, body, note, lamp } = content;
	const link = `${appUrl}/security`;
	return `<!DOCTYPE html>
<html lang="el" xmlns="http://www.w3.org/1999/xhtml">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <meta name="color-scheme" content="dark" />
  <meta name="supported-color-schemes" content="dark" />
  <title>${title} · po/iw OS</title>
  <link href="https://fonts.googleapis.com/css2?family=Space+Grotesk:wght@400;700&family=Space+Mono:wght@400;700&display=swap" rel="stylesheet" />
  <style>
    :root { color-scheme: dark; supported-color-schemes: dark; }
    body { margin: 0; padding: 0; background: #090b0f; }
    a { color: #b298f9; }
    @media (max-width: 480px) {
      .px { padding-left: 24px !important; padding-right: 24px !important; }
      .wrap { padding: 24px 12px 32px 12px !important; }
    }
  </style>
</head>
<body style="margin:0;padding:0;background:#090b0f;">
  <div style="display:none;max-height:0;overflow:hidden;opacity:0;color:#090b0f;">${note}</div>
  <table role="presentation" width="100%" cellpadding="0" cellspacing="0" border="0" bgcolor="#090b0f" style="background:#090b0f;">
    <tr>
      <td bgcolor="#24272a" style="background:#24272a;border-bottom:2px solid #07090c;box-shadow:inset 0 1px 0 #45484c;">
        <table role="presentation" cellpadding="0" cellspacing="0" border="0">
          <tr>
            <td valign="middle" style="padding:0 18px;height:56px;border-right:1px solid #07090c;box-shadow:inset 1px 0 0 #45484c;">
              <img src="${appUrl}/poiw.png" width="30" height="32" alt="po/iw" style="display:block;border:0;outline:none;width:30px;height:32px;" />
            </td>
            <td valign="middle" style="padding:0 18px;height:56px;border-right:1px solid #07090c;box-shadow:inset 1px 0 0 #45484c;font-family:${MONO};font-size:12px;line-height:1.3;letter-spacing:0.14em;text-transform:uppercase;color:#e9ebef;white-space:nowrap;">
              Operating System
            </td>
          </tr>
        </table>
      </td>
    </tr>
    <tr>
      <td class="wrap" align="center" style="padding:40px 16px 48px 16px;">
        <table role="presentation" width="100%" cellpadding="0" cellspacing="0" border="0" style="max-width:420px;">
          <tr>
            <td bgcolor="#24272a" style="background:#24272a;border:1px solid #07090c;box-shadow:inset 1px 1px 0 rgba(255,255,255,0.1);">
              <table role="presentation" width="100%" cellpadding="0" cellspacing="0" border="0">
                <tr>
                  <td class="px" style="padding:32px 32px 0 32px;font-family:${SANS};font-size:20px;line-height:1.3;font-weight:700;letter-spacing:-0.01em;color:#e9ebef;">${title}</td>
                </tr>
                <tr>
                  <td class="px" style="padding:12px 32px 0 32px;font-family:${SANS};font-size:14px;line-height:1.6;color:#a1a5ab;">${body}</td>
                </tr>
                <tr>
                  <td class="px" style="padding:24px 32px 0 32px;">
                    <table role="presentation" width="100%" cellpadding="0" cellspacing="0" border="0">
                      <tr>
                        <td align="center" bgcolor="#653eae" style="background:#653eae;border:1px solid #653eae;box-shadow:inset 1px 1px 0 rgba(255,255,255,0.14),inset -1px -1px 0 rgba(0,0,0,0.25);">
                          <a href="${link}" style="display:block;padding:13px 16px;font-family:${MONO};font-size:12px;line-height:1.3;letter-spacing:0.12em;text-transform:uppercase;color:#ffffff;text-decoration:none;">Ενεργοποίηση τώρα</a>
                        </td>
                      </tr>
                    </table>
                  </td>
                </tr>
                <tr>
                  <td class="px" style="padding:28px 32px 32px 32px;">
                    <table role="presentation" cellpadding="0" cellspacing="0" border="0">
                      <tr>
                        <td valign="top" style="padding:5px 10px 0 0;">
                          <table role="presentation" cellpadding="0" cellspacing="0" border="0">
                            <tr><td width="8" height="8" bgcolor="${lamp}" style="width:8px;height:8px;line-height:8px;font-size:1px;background:${lamp};border-radius:1px;">&nbsp;</td></tr>
                          </table>
                        </td>
                        <td style="font-family:${SANS};font-size:13px;line-height:1.6;color:#a1a5ab;">${note}</td>
                      </tr>
                    </table>
                  </td>
                </tr>
              </table>
            </td>
          </tr>
          <tr>
            <td class="px" style="padding:16px 0 0 0;font-family:${SANS};font-size:12px;line-height:1.6;color:#a1a5ab;">
              Open source by po/iw &middot; <a href="https://github.com/poiw-org" style="color:#a1a5ab;text-decoration:underline;text-underline-offset:3px;">GitHub</a><br />
              Στάλθηκε στο ${escapeHtml(due.email)}.
            </td>
          </tr>
        </table>
      </td>
    </tr>
  </table>
</body>
</html>`;
}

export function message(due: Due, appUrl: string) {
	if (due.kind === 'suspended')
		return {
			subject: 'Τα κλειδιά σου στο po/iw ανεστάλησαν',
			html: page(due, appUrl, {
				title: 'Τα κλειδιά σου ανεστάλησαν',
				body: 'Δεν ενεργοποίησες passkey ή εφαρμογή ελέγχου ταυτότητας μέσα σε 14 ημέρες, οπότε τα κλειδιά σου ανεστάλησαν.',
				note: 'Μόλις την ενεργοποιήσεις, ανοίγουν ξανά αυτόματα.',
				lamp: '#ef6567'
			})
		};
	const days = due.days_left === 1 ? '1 ημέρα' : `${due.days_left} ημέρες`;
	return {
		subject: 'Προστάτεψε τον λογαριασμό σου στο po/iw',
		html: page(due, appUrl, {
			title: 'Προστάτεψε τον λογαριασμό σου',
			body: 'Για να κρατήσεις τα κλειδιά σου, χρειάζεται να ενεργοποιήσεις passkey ή εφαρμογή ελέγχου ταυτότητας.',
			note: `Σου απομένουν ${days} πριν ανασταλούν.`,
			lamp: '#e1b75c'
		})
	};
}
