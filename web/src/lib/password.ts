/** Supabase Auth enforces the same minimum on the server. */
export const PASSWORD_RULES: { el: string; test: (password: string) => boolean }[] = [
	{ el: 'Τουλάχιστον 8 χαρακτήρες', test: (p) => p.length >= 8 }
];

export const passwordIsStrong = (password: string) =>
	PASSWORD_RULES.every((rule) => rule.test(password));

export const PASSWORD_HINT = 'Use at least 8 characters.';
