import { fade, fly, slide } from 'svelte/transition';
import { cubicOut } from 'svelte/easing';

const reduced = () =>
	typeof matchMedia === 'function' && matchMedia('(prefers-reduced-motion: reduce)').matches;

/** A row that arrives live slides down into place; reduced motion gets a plain fade. */
export const arrive = (node: Element) =>
	fly(node, { y: reduced() ? 0 : -10, duration: reduced() ? 0 : 240 });

/** Text replaced in place: the new copy rises in while the old one fades out on the same spot. */
export const swapIn = (node: Element) =>
	fly(node, { y: reduced() ? 0 : 8, duration: reduced() ? 0 : 260, easing: cubicOut });
export const swapOut = (node: Element) => fade(node, { duration: reduced() ? 0 : 120 });

/** A section that appears under existing content opens up instead of making the layout jump. */
export const reveal = (node: Element) =>
	slide(node, { duration: reduced() ? 0 : 320, easing: cubicOut });
