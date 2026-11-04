import { fade, fly, slide } from 'svelte/transition';

export const reduced = () =>
	typeof matchMedia === 'function' && matchMedia('(prefers-reduced-motion: reduce)').matches;

/**
 * One number driven by a spring in Apple's terms (the same model as `spring` in three.ts, kept
 * here so DOM motion doesn't load three.js): `response` is roughly the seconds to arrive, `damping`
 * 1 settles without overshoot. A new target starts from the live value and velocity, so motion can
 * be grabbed or reversed mid-flight without a jump. `apply` paints each frame.
 */
export const springDriver = (
	apply: (value: number) => void,
	{ value = 0, response = 0.35, damping = 1 } = {}
) => {
	const stiffness = ((2 * Math.PI) / response) ** 2;
	const friction = (4 * Math.PI * damping) / response;
	let velocity = 0;
	let target = value;
	let frame = 0;
	let last = 0;
	let settle: (() => void) | undefined;

	const tick = (now: number) => {
		// Small fixed steps keep a late frame stable; a long pause is not replayed.
		const dt = Math.min(Math.max((now - last) / 1000, 0), 1 / 30);
		last = now;
		for (let left = dt; left > 0; left -= 1 / 240) {
			const step = Math.min(left, 1 / 240);
			velocity += (stiffness * (target - value) - friction * velocity) * step;
			value += velocity * step;
		}
		const done = Math.abs(target - value) < 1e-3 && Math.abs(velocity) < 1e-2;
		if (done) {
			value = target;
			velocity = 0;
			frame = 0;
		}
		apply(value);
		if (done) settle?.();
		else frame = requestAnimationFrame(tick);
	};

	const stop = () => {
		cancelAnimationFrame(frame);
		frame = 0;
	};

	return {
		get value() {
			return value;
		},
		/** Springs toward `next`, optionally starting at a gesture's velocity (units per second). */
		to(next: number, options: { velocity?: number; onsettle?: () => void } = {}) {
			target = next;
			if (options.velocity !== undefined) velocity = options.velocity;
			settle = options.onsettle;
			if (!frame) {
				last = performance.now();
				frame = requestAnimationFrame(tick);
			}
		},
		/** Holds the value exactly where a finger puts it. */
		set(next: number) {
			stop();
			value = target = next;
			velocity = 0;
			apply(value);
		},
		stop
	};
};

/** CSS cubic-bezier(x1, y1, x2, y2) as a Svelte easing, so JS motion matches `--ease-out`. */
const bezier = (x1: number, y1: number, x2: number, y2: number) => {
	const at = (a: number, b: number, s: number) =>
		3 * a * (1 - s) ** 2 * s + 3 * b * (1 - s) * s ** 2 + s ** 3;
	return (t: number) => {
		if (t <= 0 || t >= 1) return t;
		let lo = 0;
		let hi = 1;
		for (let i = 0; i < 24; i++) {
			const mid = (lo + hi) / 2;
			if (at(x1, x2, mid) < t) lo = mid;
			else hi = mid;
		}
		return at(y1, y2, (lo + hi) / 2);
	};
};
/** Same curve as `--ease-out` in app.css. */
export const easeOut = bezier(0.23, 1, 0.32, 1);

/** A row that arrives live slides down into place; reduced motion gets a plain fade. */
export const arrive = (node: Element, { index = 0 }: { index?: number } = {}) =>
	fly(node, {
		y: reduced() ? 0 : -10,
		duration: reduced() ? 0 : 240,
		// A freshly loaded list cascades its first rows; a row arriving live is index 0 and lands at once.
		delay: reduced() ? 0 : Math.min(index, 7) * 40,
		easing: easeOut
	});

/** Text replaced in place: the new copy rises in while the old one fades out on the same spot. */
export const swapIn = (node: Element) =>
	fly(node, { y: reduced() ? 0 : 8, duration: reduced() ? 0 : 260, easing: easeOut });
export const swapOut = (node: Element) => fade(node, { duration: reduced() ? 0 : 120 });

/** A section that appears under existing content opens up instead of making the layout jump. */
export const reveal = (node: Element) =>
	slide(node, { duration: reduced() ? 0 : 320, easing: easeOut });
