import {
	CanvasTexture,
	Color,
	DirectionalLight,
	HemisphereLight,
	Mesh,
	PerspectiveCamera,
	Scene,
	SRGBColorSpace,
	Texture,
	Vector3,
	WebGLRenderer,
	type Material,
	type Object3D
} from 'three';

// The shared language of the three.js scenes: tokens as colours, the lighting, springs, and a
// stage that renders only while something moves. Import this module dynamically, like three
// itself, so pages without a scene never download it.

let swatch: CanvasRenderingContext2D | undefined;

/** The raw value of a design token from `app.css`. */
export const token = (name: string) =>
	getComputedStyle(document.documentElement).getPropertyValue(name).trim();

/** A design token (any CSS colour, oklch included) as an sRGB colour three.js understands. */
export const tokenColour = (name: string) => {
	swatch ??= document.createElement('canvas').getContext('2d', { willReadFrequently: true })!;
	swatch.clearRect(0, 0, 1, 1);
	swatch.fillStyle = token(name);
	swatch.fillRect(0, 0, 1, 1);
	const [r, g, b] = swatch.getImageData(0, 0, 1, 1).data;
	return new Color().setRGB(r / 255, g / 255, b / 255, SRGBColorSpace);
};

/** A texture drawn once with the 2D canvas API. */
export const canvasTexture = (
	width: number,
	height: number,
	draw: (c: CanvasRenderingContext2D) => void
) => {
	const canvas = document.createElement('canvas');
	canvas.width = width;
	canvas.height = height;
	draw(canvas.getContext('2d')!);
	const map = new CanvasTexture(canvas);
	map.colorSpace = SRGBColorSpace;
	map.anisotropy = 8;
	return map;
};

/**
 * A value that chases its target, in Apple's terms: `response` is roughly the time to arrive in
 * seconds, `damping` 1 means no overshoot and lower values bounce. A new target re-targets from
 * wherever the value is, so motion never jumps.
 */
export const spring = (value: number, response = 0.5, damping = 1) => ({
	value,
	velocity: 0,
	target: value,
	stiffness: ((2 * Math.PI) / response) ** 2,
	friction: (4 * Math.PI * damping) / response
});
export type Spring = ReturnType<typeof spring>;

/** Advances springs by `dt` seconds in small fixed steps, so a late frame stays stable. */
export const stepSprings = (springs: Spring[], dt: number) => {
	for (let left = dt; left > 0; left -= 1 / 120) {
		const step = Math.min(left, 1 / 120);
		for (const s of springs) {
			s.velocity += (s.stiffness * (s.target - s.value) - s.friction * s.velocity) * step;
			s.value += s.velocity * step;
		}
	}
};

/** Jumps springs to their targets, for reduced motion. */
export const snapSprings = (springs: Spring[]) => {
	for (const s of springs) {
		s.value = s.target;
		s.velocity = 0;
	}
};

export const settled = (springs: Spring[]) =>
	springs.every((s) => Math.abs(s.target - s.value) < 1e-3 && Math.abs(s.velocity) < 1e-3);

/** Eases a colour toward its target; returns whether it is still moving. */
export const easeColour = (colour: Color, target: Color, dt: number) => {
	colour.lerp(target, 1 - Math.exp(-dt * 10));
	const gap =
		Math.abs(colour.r - target.r) + Math.abs(colour.g - target.g) + Math.abs(colour.b - target.b);
	if (gap < 1e-3) colour.copy(target);
	return gap >= 1e-3;
};

type View = {
	/** The point the camera looks at. */
	target: [number, number, number];
	/** Where the camera sits, as a direction from the target. */
	from: [number, number, number];
	/** Camera distance for an aspect ratio, so narrow screens can pull back. */
	distance: (aspect: number) => number;
};

/**
 * A scene in `host` with the shared lighting. `play(draw)` starts it: `draw(dt)` updates the scene
 * and returns whether anything still moves; the loop sleeps when nothing does, and `wake()` starts
 * it again after a state change. Under reduced motion every wake draws one still frame.
 * Returns nothing when WebGL is unavailable.
 */
export function createStage(host: HTMLElement, view: View) {
	let renderer: WebGLRenderer;
	try {
		renderer = new WebGLRenderer({ alpha: true, antialias: true });
	} catch {
		return undefined;
	}
	const reduced = matchMedia('(prefers-reduced-motion: reduce)').matches;
	renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
	const canvas = renderer.domElement;
	canvas.style.cssText = 'display:block;width:100%;height:100%';
	host.appendChild(canvas);

	const world = new Scene();
	const camera = new PerspectiveCamera(30, 1, 0.1, 60);
	const target = new Vector3(...view.target);
	const from = new Vector3(...view.from).normalize();

	// A key light from above, a soft fill, and a brand rim from behind so edges leave the navy.
	world.add(new HemisphereLight(0xffffff, tokenColour('--color-void'), 1.3));
	const key = new DirectionalLight(0xffffff, 2.2);
	key.position.set(2, 6, 4);
	const rim = new DirectionalLight(tokenColour('--color-brand-text'), 1.4);
	rim.position.set(-3, 2, -5);
	world.add(key, rim);

	let draw: (dt: number) => boolean = () => false;
	let frame = 0;
	let last = 0;
	const render = (dt: number) => {
		const moving = draw(dt);
		renderer.render(world, camera);
		return moving && !reduced;
	};
	const tick = (now: number) => {
		const dt = Math.min((now - last) / 1000, 0.1);
		last = now;
		frame = render(dt) ? requestAnimationFrame(tick) : 0;
	};
	const wake = () => {
		if (frame) return;
		if (reduced) {
			render(0);
			return;
		}
		last = performance.now();
		frame = requestAnimationFrame(tick);
	};

	const resize = () => {
		const { clientWidth: w, clientHeight: h } = host;
		if (!w || !h) return;
		renderer.setSize(w, h, false);
		camera.aspect = w / h;
		camera.position.copy(from).multiplyScalar(view.distance(camera.aspect)).add(target);
		camera.lookAt(target);
		camera.updateProjectionMatrix();
		if (!frame) render(0);
	};
	const observer = new ResizeObserver(resize);
	observer.observe(host);

	return {
		world,
		reduced,
		wake,
		play(next: (dt: number) => boolean) {
			draw = next;
			resize();
			wake();
		},
		dispose() {
			cancelAnimationFrame(frame);
			observer.disconnect();
			canvas.remove();
			world.traverse((object: Object3D) => {
				// Meshes, sprites and lines all own a geometry and a material.
				if (!('geometry' in object)) return;
				const { geometry, material } = object as Mesh;
				geometry.dispose();
				for (const m of [material].flat() as Material[]) {
					for (const value of Object.values(m)) if (value instanceof Texture) value.dispose();
					m.dispose();
				}
			});
			renderer.dispose();
		}
	};
}
export type Stage = NonNullable<ReturnType<typeof createStage>>;
