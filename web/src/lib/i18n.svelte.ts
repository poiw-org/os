import { el } from './i18n/el.ts';

// Gettext-style: the English text is the key, so a message missing from `el` shows in English
// instead of breaking. Run `node scripts/check-i18n.mjs` to list any that are missing.
export type Locale = 'en' | 'el';

const STORAGE = 'doora.locale';

const initial = (): Locale => {
	try {
		const saved = localStorage.getItem(STORAGE);
		if (saved === 'en' || saved === 'el') return saved;
	} catch {
		// Storage can be blocked; fall through to the browser's language.
	}
	return typeof navigator !== 'undefined' && navigator.language.toLowerCase().startsWith('el')
		? 'el'
		: 'en';
};

const current = $state<{ locale: Locale }>({ locale: initial() });
if (typeof document !== 'undefined') document.documentElement.lang = current.locale;

export const getLocale = () => current.locale;

export const setLocale = (locale: Locale) => {
	current.locale = locale;
	document.documentElement.lang = locale;
	try {
		localStorage.setItem(STORAGE, locale);
	} catch {
		// Not persisted; the choice still applies until the tab closes.
	}
};

/** Translates `message`, filling `{name}` placeholders from `params`. */
export const t = (message: string, params?: Record<string, string | number>) => {
	const text = (current.locale === 'el' && el[message]) || message;
	return params ? text.replace(/\{(\w+)\}/g, (hole, name) => String(params[name] ?? hole)) : text;
};

/** Picks the singular or plural message for `n`; both may use `{n}`. */
export const tn = (n: number, one: string, other: string) => t(n === 1 ? one : other, { n });

/** The BCP 47 tag for dates and relative times. */
export const dateLocale = () => (current.locale === 'el' ? 'el-GR' : 'en-GB');
