<script lang="ts">
	import { onMount } from 'svelte';

	// The lab's 3D printer, drag to turn it. Everything three.js is imported on demand so the
	// homepage text never waits for the 4.7 MB model.
	let host: HTMLDivElement;

	onMount(() => {
		let dispose: (() => void) | undefined;
		let cancelled = false;

		(async () => {
			const [{ Box3, Vector3 }, { GLTFLoader }, { OrbitControls }, { createStage }] =
				await Promise.all([
					import('three'),
					import('three/examples/jsm/loaders/GLTFLoader.js'),
					import('three/examples/jsm/controls/OrbitControls.js'),
					import('#lib/three.ts')
				]);

			const stage = createStage(host, {
				target: [0, 0, 0],
				from: [1.1, 0.7, 1.6],
				// Far enough that the whole machine stays in frame at any angle of its turn.
				// The model is 3 units across, up to ~3.9 wide mid-turn; fov 30 shows 0.536 x distance
				// vertically, and aspect times that horizontally.
				distance: (aspect) => Math.max(7, 3.4 / (0.536 * aspect))
			});
			if (!stage) return;

			const gltf = await new GLTFLoader().loadAsync('/models/3d-printer.glb');
			if (cancelled) return stage.dispose();

			// Centre the model on the origin and scale it to a fixed size, whatever units it came in.
			const model = gltf.scene;
			const box = new Box3().setFromObject(model);
			const size = box.getSize(new Vector3());
			const centre = box.getCenter(new Vector3());
			const scale = 3 / Math.max(size.x, size.y, size.z);
			model.scale.setScalar(scale);
			model.position.copy(centre.multiplyScalar(-scale));
			stage.world.add(model);

			// Drag to turn; no zoom or pan so the page still scrolls past it. It idles in a slow turn.
			const controls = new OrbitControls(stage.camera, host.querySelector('canvas')!);
			controls.enableZoom = false;
			controls.enablePan = false;
			controls.enableDamping = true;
			controls.autoRotate = !stage.reduced;
			controls.autoRotateSpeed = 1.2;
			controls.addEventListener('change', stage.wake);
			stage.play(() => {
				controls.update();
				return controls.autoRotate;
			});

			dispose = () => {
				controls.dispose();
				stage.dispose();
			};
		})().catch(() => {});

		return () => {
			cancelled = true;
			dispose?.();
		};
	});
</script>

<div class="viewer" role="img" aria-label="Τρισδιάστατο μοντέλο του 3D printer του εργαστηρίου">
	<div bind:this={host} class="stage"></div>
</div>

<style>
	.viewer {
		position: relative;
		height: min(42rem, 78vh);
		touch-action: pan-y;
	}
	.stage {
		position: absolute;
		inset: 0;
	}
</style>
