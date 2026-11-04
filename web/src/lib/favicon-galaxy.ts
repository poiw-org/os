// Animated pixel-art favicon: a slowly rotating spiral galaxy with twinkling stars,
// in the violet palette of the login backdrop.
const SIZE = 16; // simulation grid
const CELL = 2; // canvas pixels per grid cell
const FRAME_MS = 120;
const SPIN_STEP = 0.1;

// Dim -> bright. Index 0 is transparent so the tab background shows through.
const PALETTE = [
	null,
	[38, 30, 77],
	[72, 45, 138],
	[111, 70, 196],
	[160, 125, 235],
	[203, 182, 250],
	[245, 238, 255]
] as const;

// Fixed background stars (grid x, y, twinkle phase) kept clear of the galaxy disc.
const STARS: [number, number, number][] = [
	[1, 2, 0],
	[13, 1, 2],
	[14, 12, 4],
	[2, 13, 1],
	[8, 0, 3]
];

function brightness(x: number, y: number, spin: number): number {
	const dx = x - (SIZE - 1) / 2;
	const dy = y - (SIZE - 1) / 2;
	const r = Math.hypot(dx, dy);
	if (r > 7.5) return 0;
	const theta = Math.atan2(dy, dx);
	// Two logarithmic spiral arms fading outward, plus a bright core.
	const arms = 0.5 + 0.5 * Math.cos(2 * (theta - spin) - 3.5 * Math.log(r + 1));
	const disc = Math.exp(-r / 4.2) * (0.35 + 0.65 * arms);
	const core = Math.exp(-(r * r) / 3);
	return Math.min(1, disc + core);
}

export function startFaviconGalaxy(): () => void {
	const canvas = document.createElement('canvas');
	canvas.width = canvas.height = SIZE * CELL;
	const ctx = canvas.getContext('2d');
	if (!ctx) return () => {};

	let link = document.querySelector<HTMLLinkElement>('link[rel="icon"]');
	if (!link) {
		link = document.createElement('link');
		link.rel = 'icon';
		document.head.append(link);
	}
	link.type = 'image/png';

	let tick = 0;
	function draw() {
		const spin = tick * SPIN_STEP;
		ctx!.clearRect(0, 0, SIZE * CELL, SIZE * CELL);
		const paint = (x: number, y: number, level: number) => {
			const c = PALETTE[level];
			if (!c) return;
			ctx!.fillStyle = `rgb(${c[0]} ${c[1]} ${c[2]})`;
			ctx!.fillRect(x * CELL, y * CELL, CELL, CELL);
		};
		for (let y = 0; y < SIZE; y++) {
			for (let x = 0; x < SIZE; x++) {
				const level = Math.round(brightness(x, y, spin) * (PALETTE.length - 1));
				if (level > 1) paint(x, y, level);
			}
		}
		for (const [x, y, phase] of STARS) {
			paint(x, y, 3 + ((Math.floor(tick / 3) + phase) % 4)); // twinkle 3..6
		}
		link!.href = canvas.toDataURL('image/png');
		tick++;
	}

	draw();
	if (matchMedia('(prefers-reduced-motion: reduce)').matches) return () => {};
	const timer = setInterval(draw, FRAME_MS);
	return () => clearInterval(timer);
}
