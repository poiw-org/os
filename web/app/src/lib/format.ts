export const formatDateTime = (date: Date) =>
	`${date.toLocaleDateString('el-GR')} ${date.toLocaleTimeString('el-GR')}`;

export const formatDate = (date: Date) => date.toLocaleString('el-GR');

// Key ids are uuids; the first block is enough to tell keys apart on screen.
export const shortId = (id: string) => id.slice(0, 8);

const rtf = new Intl.RelativeTimeFormat('en', { numeric: 'auto' });
const UNITS: [Intl.RelativeTimeFormatUnit, number][] = [
	['year', 31_536_000],
	['month', 2_592_000],
	['day', 86_400],
	['hour', 3_600],
	['minute', 60]
];

/** "5 minutes ago", "yesterday". */
export function timeAgo(date: Date, now = Date.now()) {
	const seconds = Math.round((date.getTime() - now) / 1000);
	for (const [unit, size] of UNITS) {
		if (Math.abs(seconds) >= size) return rtf.format(Math.round(seconds / size), unit);
	}
	return 'just now';
}

/** "Full Name · email" once the member has a name on file, else just the email. */
export const withName = (user: { email: string; fullName?: string }) =>
	user.fullName ? `${user.fullName} · ${user.email}` : user.email;
