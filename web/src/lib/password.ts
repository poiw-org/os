/** What counts as a strong password. Supabase Auth enforces the same rules on the server. */
export const PASSWORD_RULES: { el: string; test: (password: string) => boolean }[] = [
	{ el: 'Τουλάχιστον 12 χαρακτήρες', test: (p) => p.length >= 12 },
	{ el: 'Ένα πεζό γράμμα', test: (p) => /[a-z]/.test(p) },
	{ el: 'Ένα κεφαλαίο γράμμα', test: (p) => /[A-Z]/.test(p) },
	{ el: 'Έναν αριθμό', test: (p) => /[0-9]/.test(p) },
	{ el: 'Ένα σύμβολο', test: (p) => /[^A-Za-z0-9]/.test(p) }
];

export const passwordIsStrong = (password: string) =>
	PASSWORD_RULES.every((rule) => rule.test(password));

export const PASSWORD_HINT =
	'Use at least 12 characters with lower and upper case letters, a number and a symbol.';
