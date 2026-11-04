<script lang="ts">
	import { onMount } from 'svelte';

	// A field of dots that washes in from the centre of the screen, then sits still.
	let canvas: HTMLCanvasElement;

	const DOT = 5;
	const GAP = 18;
	const INTRO_MS = 1400;

	type Dot = { cx: number; cy: number };

	onMount(() => {
		const ctx = canvas.getContext('2d')!;
		const reduced = matchMedia('(prefers-reduced-motion: reduce)').matches;
		const css = getComputedStyle(document.documentElement);
		const swatch = document.createElement('canvas').getContext('2d', { willReadFrequently: true })!;
		const rgb = (name: string) => {
			swatch.clearRect(0, 0, 1, 1);
			swatch.fillStyle = css.getPropertyValue(name).trim();
			swatch.fillRect(0, 0, 1, 1);
			const [r, g, b] = swatch.getImageData(0, 0, 1, 1).data;
			return [r, g, b];
		};
		const base = rgb('--color-gray-200');
		const voidColour = css.getPropertyValue('--color-void').trim();
		const dotColour = `rgb(${base.join(',')})`;

		let dots: Dot[] = [];
		let w = 0;
		let h = 0;
		let dpr = 1;
		let frame = 0;
		let born = performance.now();

		const build = () => {
			dpr = Math.min(devicePixelRatio, 2);
			w = canvas.clientWidth;
			h = canvas.clientHeight;
			canvas.width = w * dpr;
			canvas.height = h * dpr;
			const step = DOT + GAP;
			const cols = Math.ceil(w / step) + 1;
			const rows = Math.ceil(h / step) + 1;
			dots = [];
			for (let r = 0; r < rows; r++)
				for (let c = 0; c < cols; c++) {
					const cx = c * step + GAP / 2;
					const cy = r * step + GAP / 2;
					dots.push({ cx, cy });
				}
			draw(performance.now());
		};

		function draw(now: number) {
			ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
			ctx.fillStyle = voidColour;
			ctx.fillRect(0, 0, w, h);
			const intro = reduced ? 1 : Math.min((now - born) / INTRO_MS, 1);
			const reach = Math.hypot(w, h) * intro * 1.1;
			ctx.fillStyle = dotColour;
			for (const d of dots) {
				// The intro washes in from the centre of the screen.
				const fromMiddle = Math.hypot(d.cx - w / 2, d.cy - h / 2);
				ctx.globalAlpha = reduced ? 1 : Math.min(Math.max((reach - fromMiddle) / 160, 0), 1);
				ctx.beginPath();
				ctx.arc(d.cx, d.cy, DOT / 2, 0, Math.PI * 2);
				ctx.fill();
			}
			ctx.globalAlpha = 1;
		}

		const loop = (now: number) => {
			draw(now);
			if (now - born < INTRO_MS + 200) frame = requestAnimationFrame(loop);
		};

		build();
		const observer = new ResizeObserver(build);
		observer.observe(canvas);
		if (!reduced) {
			born = performance.now();
			frame = requestAnimationFrame(loop);
		}
		return () => {
			cancelAnimationFrame(frame);
			observer.disconnect();
		};
	});
</script>

<canvas
	bind:this={canvas}
	class="pointer-events-none fixed inset-0 h-full w-full"
	aria-hidden="true"
></canvas>
