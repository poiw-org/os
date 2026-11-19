/** Supabase Auth enforces the same minimum on the server. `text` is an i18n message: pass it to t(). */
export const PASSWORD_RULES: { text: string; test: (password: string) => boolean }[] = [
	{ text: 'At least 8 characters', test: (p) => p.length >= 8 }
];

export const passwordIsStrong = (password: string) =>
	PASSWORD_RULES.every((rule) => rule.test(password));

export const PASSWORD_HINT = 'Use at least 8 characters.';
