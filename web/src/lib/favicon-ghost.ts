// Animated pixel-art favicon: a Pac-Man style ghost in the login backdrop's violet,
// wobbling its feet and glancing left and right.
const SIZE = 14; // sprite grid
const CELL = 2; // canvas pixels per sprite pixel
const FRAME_MS = 220;

const BODY = '#a07deb';
const WHITE = '#f5eeff';
const PUPIL = '#261e4d';

// '#' = body. Eyes are drawn on top; the last two rows are the feet (two wobble frames).
const TOP = [
	'....######....',
	'..##########..',
	'.############.',
	'.############.',
	'.############.',
	'.############.',
	'.############.',
	'.############.',
	'##############',
	'##############',
	'##############',
	'##############'
];
const FEET = [
	['##.###..###.##', '#...##..##...#'],
	['###.###.###.##', '##...#..#...##']
];
const LOOK = [2, 2, 1, 0, 0, 1]; // pupil offset, left -> right -> left

export function startFaviconGhost(): () => void {
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

	const px = (x: number, y: number, w: number, h: number, color: string) => {
		ctx.fillStyle = color;
		ctx.fillRect(x * CELL, y * CELL, w * CELL, h * CELL);
	};

	let tick = 0;
	function draw() {
		ctx!.clearRect(0, 0, SIZE * CELL, SIZE * CELL);
		const rows = [...TOP, ...FEET[Math.floor(tick / 2) % 2]];
		rows.forEach((row, y) => {
			[...row].forEach((c, x) => c === '#' && px(x, y, 1, 1, BODY));
		});
		const look = LOOK[tick % LOOK.length];
		for (const ex of [2, 8]) {
			px(ex, 3, 4, 4, WHITE);
			px(ex + look, 4, 2, 2, PUPIL);
		}
		link!.href = canvas.toDataURL('image/png');
		tick++;
	}

	draw();
	if (matchMedia('(prefers-reduced-motion: reduce)').matches) return () => {};
	const timer = setInterval(draw, FRAME_MS);
	return () => clearInterval(timer);
}
