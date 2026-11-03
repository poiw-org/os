<script lang="ts">
	// A grid of CSS 3D cubes that tilt toward the pointer, wander on their own when idle, and
	// ripple on click. Adapted from React Bits' Cubes, after Can Tastemel's lambda.ai landing page.
	import { onMount } from 'svelte';

	let {
		gridSize = 10,
		maxAngle = 45,
		radius = 3,
		cellGap = 1.5,
		faceColor = '#120F17',
		edgeColor = '#ffffff',
		rippleColor = '#ffffff',
		idleAfter = 3000
	}: {
		gridSize?: number;
		maxAngle?: number;
		radius?: number;
		/** Gap between cubes, as a percentage of the grid's width. */
		cellGap?: number;
		faceColor?: string;
		edgeColor?: string;
		rippleColor?: string;
		/** Milliseconds without pointer input before the cubes start moving on their own. */
		idleAfter?: number;
	} = $props();

	const faces = ['front', 'back', 'left', 'right', 'top', 'bottom'];
	const cells = $derived(Array.from({ length: gridSize * gridSize }, (_, i) => i));

	let scene = $state<HTMLDivElement>();
	// Half a cube's edge in px: translateZ takes no percentages.
	let half = $state(0);

	onMount(() => {
		const root = scene!;
		const cubes = [...root.querySelectorAll<HTMLElement>('.cube')];
		const reduced = matchMedia('(prefers-reduced-motion: reduce)').matches;

		const resize = new ResizeObserver(() => (half = cubes[0].offsetWidth / 2));
		resize.observe(root);

		const distance = (i: number, row: number, col: number) =>
			Math.hypot(Math.floor(i / gridSize) + 0.5 - row, (i % gridSize) + 0.5 - col);

		const tiltAround = (row: number, col: number) => {
			for (const [i, cube] of cubes.entries()) {
				const dist = distance(i, row, col);
				const angle = dist <= radius ? (1 - dist / radius) * maxAngle : 0;
				cube.style.transform = `rotateX(${-angle}deg) rotateY(${angle}deg)`;
			}
		};

		const cellAt = (event: PointerEvent): [number, number] => {
			const rect = root.getBoundingClientRect();
			return [
				((event.clientY - rect.top) / rect.height) * gridSize,
				((event.clientX - rect.left) / rect.width) * gridSize
			];
		};

		let lastInput = -Infinity;
		const onMove = (event: PointerEvent) => {
			lastInput = performance.now();
			tiltAround(...cellAt(event));
		};
		const onLeave = () => tiltAround(-99, -99);

		// Ripple: faces flash outward from the clicked cell, delayed by distance.
		const onDown = (event: PointerEvent) => {
			if (reduced) return;
			const [row, col] = cellAt(event);
			for (const [i, cube] of cubes.entries()) {
				cube.animate([{ '--flash': 0 }, { '--flash': 0.6 }, { '--flash': 0 }] as Keyframe[], {
					duration: 700,
					delay: distance(i, row, col) * 60,
					easing: 'ease-out'
				});
			}
		};

		// Idle: a focus point drifts smoothly between random targets.
		let frame = 0;
		const focus = { row: gridSize / 2, col: gridSize / 2 };
		const target = { ...focus };
		let nextTarget = 0;
		const idle = (now: number) => {
			frame = requestAnimationFrame(idle);
			if (now - lastInput < idleAfter) return;
			if (now > nextTarget) {
				target.row = Math.random() * gridSize;
				target.col = Math.random() * gridSize;
				nextTarget = now + 1800 + Math.random() * 1400;
			}
			focus.row += (target.row - focus.row) * 0.02;
			focus.col += (target.col - focus.col) * 0.02;
			tiltAround(focus.row, focus.col);
		};

		root.addEventListener('pointermove', onMove);
		root.addEventListener('pointerleave', onLeave);
		root.addEventListener('pointerdown', onDown);
		if (!reduced) frame = requestAnimationFrame(idle);
		return () => {
			cancelAnimationFrame(frame);
			resize.disconnect();
			root.removeEventListener('pointermove', onMove);
			root.removeEventListener('pointerleave', onLeave);
			root.removeEventListener('pointerdown', onDown);
		};
	});
</script>

<div
	bind:this={scene}
	class="scene"
	style="--grid:{gridSize};--gap:{cellGap}%;--half:{half}px;--face:{faceColor};--edge:{edgeColor};--ripple:{rippleColor}"
	aria-hidden="true"
>
	{#each cells as i (i)}
		<div class="cube">
			{#each faces as face (face)}<div class="face {face}"></div>{/each}
		</div>
	{/each}
</div>

<style>
	@property --flash {
		syntax: '<number>';
		inherits: true;
		initial-value: 0;
	}
	.scene {
		display: grid;
		grid-template-columns: repeat(var(--grid), 1fr);
		gap: var(--gap);
		width: 100%;
		aspect-ratio: 1;
		perspective: 99999999px;
		touch-action: none;
	}
	.cube {
		position: relative;
		aspect-ratio: 1;
		transform-style: preserve-3d;
		transition: transform 0.6s cubic-bezier(0.22, 1, 0.36, 1);
	}
	.face {
		position: absolute;
		inset: 0;
		background: color-mix(in oklab, var(--face), var(--ripple) calc(var(--flash) * 100%));
		border: 1px solid color-mix(in oklab, var(--edge) 50%, transparent);
	}
	.front {
		transform: translateZ(var(--half));
	}
	.back {
		transform: rotateY(180deg) translateZ(var(--half));
	}
	.left {
		transform: rotateY(-90deg) translateZ(var(--half));
	}
	.right {
		transform: rotateY(90deg) translateZ(var(--half));
	}
	.top {
		transform: rotateX(90deg) translateZ(var(--half));
	}
	.bottom {
		transform: rotateX(-90deg) translateZ(var(--half));
	}
	@media (prefers-reduced-motion: reduce) {
		.cube {
			transition: none;
		}
	}
</style>
