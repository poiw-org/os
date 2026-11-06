// Draws the mascot's ASCII art onto a 2D canvas, shuffling characters between frames. Dither uploads
// that canvas as a texture, so the figure is rendered by the same shader as the background.

const RAMP = ' .,:;-=+*#%@';
const FONT_SIZE = 15;
const LINE_HEIGHT = 1.22; // cells twice as tall as wide for Space Mono, matching how the art was sampled

export type Ascii = {
	canvas: HTMLCanvasElement;
	/** Redraws the art at time `t` seconds; `animate` false draws it as authored. */
	draw(t: number, animate: boolean): void;
};

export async function createAscii(art: string): Promise<Ascii> {
	await document.fonts.load(`700 ${FONT_SIZE}px "Space Mono"`);
	const rows = art.split('\n');
	const canvas = document.createElement('canvas');
	const ctx = canvas.getContext('2d')!;
	const font = `700 ${FONT_SIZE}px "Space Mono", monospace`;
	ctx.font = font;
	const advance = ctx.measureText('M').width;
	const lineHeight = FONT_SIZE * LINE_HEIGHT;
	canvas.width = Math.ceil(Math.max(...rows.map((row) => row.length)) * advance);
	canvas.height = Math.ceil(rows.length * lineHeight);
	// Resizing a canvas resets its context state.
	ctx.font = font;
	ctx.textBaseline = 'top';
	ctx.fillStyle = '#fff';

	// A band of shuffling sweeps diagonally across the figure; outside it, and for the silhouette's
	// blank cells, characters stay put apart from the odd flicker.
	const shuffled = (t: number) =>
		rows.map((row, y) => {
			let out = '';
			for (let x = 0; x < row.length; x++) {
				const target = RAMP.indexOf(row[x]);
				if (target <= 0) {
					out += row[x];
					continue;
				}
				const wave = Math.sin(x * 0.12 + y * 0.2 - t * 1.4);
				const spread = wave > 0.55 ? 3 : Math.random() < 0.015 ? 2 : 0;
				const i = target + Math.round((Math.random() * 2 - 1) * spread);
				out += RAMP[Math.min(RAMP.length - 1, Math.max(1, i))];
			}
			return out;
		});

	return {
		canvas,
		draw(t, animate) {
			ctx.clearRect(0, 0, canvas.width, canvas.height);
			(animate ? shuffled(t) : rows).forEach((row, y) => ctx.fillText(row, 0, y * lineHeight));
		}
	};
}
