import { fly } from 'svelte/transition';

const reduced = () =>
	typeof matchMedia === 'function' && matchMedia('(prefers-reduced-motion: reduce)').matches;

/** A row that arrives live slides down into place; reduced motion gets a plain fade. */
export const arrive = (node: Element) =>
	fly(node, { y: reduced() ? 0 : -10, duration: reduced() ? 0 : 240 });
