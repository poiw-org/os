<script lang="ts">
	import { onMount } from 'svelte';

	// The lab's 3D printer, drag to turn it. It prints Manousos Mikrotsipakis, the mascot, on its
	// bed; click the bed to look at him from above. Everything three.js is imported on demand so the
	// homepage text never waits for the 4.7 MB model.
	let host: HTMLDivElement;
	let focused = $state(false);
	let back: (() => void) | undefined;

	onMount(() => {
		let dispose: (() => void) | undefined;
		let cancelled = false;

		(async () => {
			const [
				{
					Box3,
					BoxGeometry,
					Color,
					InstancedMesh,
					Matrix4,
					Mesh,
					MeshStandardMaterial,
					PlaneGeometry,
					Raycaster,
					Vector2,
					Vector3
				},
				{ GLTFLoader },
				{ OrbitControls },
				{ createStage, spring, stepSprings, snapSprings, settled, tokenColour },
				{ default: art }
			] = await Promise.all([
				import('three'),
				import('three/examples/jsm/loaders/GLTFLoader.js'),
				import('three/examples/jsm/controls/OrbitControls.js'),
				import('#lib/three.ts'),
				import('#lib/auth/mascot.txt?raw')
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
			// The model comes with a pistol on the bed; the bed is Manousos's now.
			model.traverse((o) => {
				if (/^(Pistol|Suppressor|Device_Shell)/.test(o.name)) o.visible = false;
			});
			stage.world.add(model);

			// The print: one column per character of the mascot's ASCII art, as tall as the character is
			// dense, so the figure is a relief on the bed. Row 0 is the back of the bed.
			const RAMP = ' .,:;-=+*#%@';
			const rows = art.split('\n');
			const cols = Math.max(...rows.map((r) => r.length));
			const BED = { x: 0.39, y: -1.12, z: 0.17, size: 1.1 };
			const cell = BED.size / Math.max(cols, rows.length * 2);
			const MAX_HEIGHT = 0.12;
			const columns: { x: number; z: number; h: number; shade: number }[] = [];
			rows.forEach((row, y) => {
				for (let x = 0; x < row.length; x++) {
					const level = RAMP.indexOf(row[x]);
					if (level <= 0) continue;
					columns.push({
						x: BED.x + (x - cols / 2 + 0.5) * cell,
						z: BED.z + (y - rows.length / 2 + 0.5) * cell * 2,
						h: (level / (RAMP.length - 1)) * MAX_HEIGHT + 0.01,
						shade: level / (RAMP.length - 1)
					});
				}
			});
			const bright = new Color('#ffffff');
			const brand = tokenColour('--color-brand-text');
			const print = new InstancedMesh(
				new BoxGeometry(1, 1, 1),
				new MeshStandardMaterial({ roughness: 0.55 }),
				columns.length
			);
			columns.forEach((c, i) =>
				print.setColorAt(i, new Color().copy(brand).lerp(bright, 0.15 + c.shade * 0.85))
			);
			const m = new Matrix4();
			const place = (layer: number) => {
				columns.forEach((c, i) => {
					const h = Math.max(1e-4, Math.min(c.h, layer));
					m.makeScale(cell, h, cell * 2).setPosition(c.x, BED.y + h / 2, c.z);
					print.setMatrixAt(i, m);
				});
				print.instanceMatrix.needsUpdate = true;
			};
			stage.world.add(print);

			// The bed, as something to point at: invisible, but the raycaster still finds it.
			const bed = new Mesh(new PlaneGeometry(BED.size * 1.15, BED.size * 1.15));
			bed.rotation.x = -Math.PI / 2;
			bed.position.set(BED.x, BED.y + 0.02, BED.z);
			bed.visible = false;
			stage.world.add(bed);

			// Drag to turn; no zoom or pan so the page still scrolls past it. It idles in a slow turn.
			const canvas = host.querySelector('canvas')!;
			const controls = new OrbitControls(stage.camera, canvas);
			controls.enableZoom = false;
			controls.enablePan = false;
			controls.enableDamping = true;
			controls.autoRotate = !stage.reduced;
			controls.autoRotateSpeed = 1.2;
			controls.addEventListener('change', stage.wake);

			// Focus: 0 is the free orbit, 1 is the camera above the bed looking straight down at him.
			const focus = spring(0, 0.9);
			let looking = false;
			const home = { position: new Vector3(), target: new Vector3() };
			const overhead = new Vector3();
			const aim = new Vector3();
			const target = new Vector3();
			const UP = new Vector3(0, 1, 0);
			const DOWN_UP = new Vector3(0, 0, -1);
			const fit = () => {
				// The figure fills about 1.15 units; the caption sits under it, so aim a little low.
				const d = Math.max(1.5 / 0.536, 1.5 / (0.536 * stage.camera.aspect));
				aim.set(BED.x, BED.y + 0.05, BED.z + 0.12);
				overhead.set(aim.x, aim.y + d, aim.z + 1e-3);
			};

			// Printing takes a few seconds, layer by layer; reduced motion gets the finished figure.
			const FULL = MAX_HEIGHT + 0.02;
			// Above this height over the bed the machine is cut away in the overhead view.
			const CUT = 0.25;
			const PRINT_SECONDS = 7;
			let layer = stage.reduced ? 1 : 0;
			place(layer * FULL);

			const toggle = (to: boolean) => {
				if (to === looking) return;
				looking = to;
				focused = to;
				if (to) {
					home.position.copy(stage.camera.position);
					home.target.copy(controls.target);
					controls.autoRotate = false;
					controls.enabled = false;
					canvas.style.cursor = '';
				} else if (!stage.reduced) controls.autoRotate = true;
				focus.target = to ? 1 : 0;
				if (stage.reduced) snapSprings([focus]);
				stage.wake();
			};
			back = () => toggle(false);

			stage.play((dt) => {
				let moving = false;
				if (layer < 1) {
					layer = Math.min(1, layer + dt / PRINT_SECONDS);
					place(layer * FULL);
					moving = layer < 1;
				}
				stepSprings([focus], dt);
				const t = focus.value;
				if (looking || t > 1e-3) {
					fit();
					stage.camera.position.lerpVectors(home.position, overhead, t);
					target.lerpVectors(home.target, aim, t);
					stage.camera.up.lerpVectors(UP, DOWN_UP, t).normalize();
					stage.camera.lookAt(target);
					// Looking down, the gantry and toolhead would hide him. The near plane slices
					// everything above the print away, like a cutaway.
					stage.camera.near = 0.1 + (overhead.y - (BED.y + CUT) - 0.1) * t * t;
					stage.camera.updateProjectionMatrix();
					moving ||= !settled([focus]);
				} else {
					if (!controls.enabled) {
						stage.camera.near = 0.1;
						stage.camera.updateProjectionMatrix();
						// Hand the camera back to the orbit exactly where the return left it.
						stage.camera.up.copy(UP);
						controls.target.copy(home.target);
						stage.camera.position.copy(home.position);
						controls.enabled = true;
					}
					controls.update();
					moving ||= controls.autoRotate;
				}
				return moving;
			});

			// Over the bed the cursor offers to enlarge; a click that was not a drag does it.
			const ray = new Raycaster();
			const pointer = new Vector2();
			const overBed = (e: PointerEvent) => {
				const r = canvas.getBoundingClientRect();
				pointer.set(
					((e.clientX - r.left) / r.width) * 2 - 1,
					-((e.clientY - r.top) / r.height) * 2 + 1
				);
				ray.setFromCamera(pointer, stage.camera);
				return ray.intersectObjects([bed, print]).length > 0;
			};
			let downAt: [number, number] | undefined;
			const onMove = (e: PointerEvent) => {
				canvas.style.cursor = !looking && !e.buttons && overBed(e) ? 'zoom-in' : '';
			};
			const onDown = (e: PointerEvent) => (downAt = [e.clientX, e.clientY]);
			const onUp = (e: PointerEvent) => {
				const moved = downAt ? Math.hypot(e.clientX - downAt[0], e.clientY - downAt[1]) : 99;
				downAt = undefined;
				if (!looking && moved < 6 && overBed(e)) toggle(true);
			};
			const onLeave = () => (canvas.style.cursor = '');
			const onKey = (e: KeyboardEvent) => {
				if (e.key === 'Escape' && looking) toggle(false);
			};
			canvas.addEventListener('pointermove', onMove);
			canvas.addEventListener('pointerdown', onDown);
			canvas.addEventListener('pointerup', onUp);
			canvas.addEventListener('pointerleave', onLeave);
			window.addEventListener('keydown', onKey);

			dispose = () => {
				canvas.removeEventListener('pointermove', onMove);
				canvas.removeEventListener('pointerdown', onDown);
				canvas.removeEventListener('pointerup', onUp);
				canvas.removeEventListener('pointerleave', onLeave);
				window.removeEventListener('keydown', onKey);
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

<div class="viewer">
	<div
		bind:this={host}
		class="stage"
		role="img"
		aria-label="Τρισδιάστατο μοντέλο του 3D printer του εργαστηρίου, που τυπώνει τον Μανούσο Μικροτσιπάκη"
	></div>
	{#if focused}
		<div class="caption plate" role="group" aria-label="Μανούσος Μικροτσιπάκης">
			<p>Μανούσος Μικροτσιπάκης, Mascot του po/iw</p>
			<button type="button" class="key" onclick={() => back?.()}>← Πίσω (Esc)</button>
		</div>
	{/if}
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
	.caption {
		position: absolute;
		inset: auto 0.75rem 0.75rem;
		display: flex;
		flex-wrap: wrap;
		align-items: center;
		justify-content: space-between;
		gap: 0.75rem;
		padding: 0.75rem 1rem;
	}
</style>
