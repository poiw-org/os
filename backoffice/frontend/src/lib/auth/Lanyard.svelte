<script lang="ts">
	import { onMount } from 'svelte';
	import type { Material, Texture } from 'three';

	// A badge on a lanyard that swings and can be dragged. Plain three.js for drawing and Rapier for
	// the rope physics; both load on demand, and only where the auth layout shows this panel.
	let host: HTMLDivElement;

	const CARD = { w: 1.5 * 1.6, h: 1.5 * 2.25, d: 0.03 };
	const CLIP_Y = CARD.h / 2 + 0.38;
	const ROPE = 1.6;
	const ANCHOR_Y = 7;

	async function start(host: HTMLDivElement) {
		const [
			THREE,
			RAPIER,
			{ MeshLineGeometry, MeshLineMaterial },
			{ RoundedBoxGeometry },
			{ token, tokenColour, canvasTexture }
		] = await Promise.all([
			import('three'),
			import('@dimforge/rapier3d-compat'),
			import('meshline'),
			import('three/examples/jsm/geometries/RoundedBoxGeometry.js'),
			import('#lib/three.ts')
		]);
		await RAPIER.init();
		await document.fonts.load('700 48px "Space Mono"');
		await document.fonts.load('400 32px "Space Mono"');
		const logo = await new Promise<HTMLImageElement>((resolve, reject) => {
			const image = new Image();
			image.onload = () => resolve(image);
			image.onerror = reject;
			image.src = '/poiw-dark.png';
		});

		const reduced = matchMedia('(prefers-reduced-motion: reduce)').matches;

		const roundRect = (c: CanvasRenderingContext2D, w: number, h: number, r: number) => {
			c.beginPath();
			c.roundRect(0, 0, w, h, r);
		};

		const front = canvasTexture(800, 1125, (c) => {
			roundRect(c, 800, 1125, 56);
			c.clip();
			c.fillStyle = token('--color-ink');
			c.fillRect(0, 0, 800, 1125);
			c.fillStyle = token('--color-brand');
			c.fillRect(0, 0, 800, 18);
			const size = 480;
			c.drawImage(logo, (800 - size) / 2, 120, size, size * (logo.height / logo.width));
			c.fillStyle = token('--color-void');
			c.textAlign = 'center';
			c.font = '700 84px "Space Mono"';
			c.fillText('John Doe', 400, 800);
			c.fillStyle = token('--color-brand');
			c.font = '400 38px "Space Mono"';
			c.fillText('hackerspace member', 400, 870);
			c.fillStyle = token('--color-void');
			c.font = '700 36px "Space Mono"';
			c.fillText('Herakleion, Crete', 400, 1060);
		});

		const back = canvasTexture(800, 1125, (c) => {
			roundRect(c, 800, 1125, 56);
			c.clip();
			c.fillStyle = token('--color-ink');
			c.fillRect(0, 0, 800, 1125);
			c.fillStyle = token('--color-brand');
			c.fillRect(0, 0, 800, 18);
			c.fillStyle = token('--color-void');
			c.textAlign = 'center';
			c.font = '700 44px "Space Mono"';
			c.fillText('Open source', 400, 520);
			c.fillStyle = token('--color-brand');
			c.font = '400 32px "Space Mono"';
			c.fillText('github.com/poiw-org', 400, 580);
		});

		const band = canvasTexture(1024, 128, (c) => {
			c.fillStyle = token('--color-brand');
			c.fillRect(0, 0, 1024, 128);
			c.fillStyle = token('--color-ink');
			c.font = '700 56px "Space Mono"';
			c.textBaseline = 'middle';
			c.fillText('po/iw', 24, 66);
			c.fillText('po/iw', 536, 66);
		});
		band.wrapS = band.wrapT = THREE.RepeatWrapping;

		const renderer = new THREE.WebGLRenderer({ alpha: true, antialias: true });
		renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
		const canvas = renderer.domElement;
		canvas.style.cssText = 'display:block;width:100%;height:100%;touch-action:pan-y';
		host.appendChild(canvas);

		const scene = new THREE.Scene();
		const camera = new THREE.PerspectiveCamera(20, 1, 0.1, 100);
		camera.position.set(0, -0.3, 18);
		scene.add(new THREE.AmbientLight(0xffffff, 1.6));
		const light = new THREE.DirectionalLight(0xffffff, 2);
		light.position.set(3, 5, 6);
		scene.add(light);

		// Card: a dark slab with a printed face on each side and a metal clip.
		const cardGroup = new THREE.Group();
		const slab = new THREE.Mesh(
			new RoundedBoxGeometry(CARD.w, CARD.h, CARD.d, 4, 0.1),
			new THREE.MeshStandardMaterial({ color: tokenColour('--color-ink'), roughness: 0.6 })
		);
		const face = (map: Texture) =>
			new THREE.MeshStandardMaterial({
				map,
				transparent: true,
				alphaTest: 0.5,
				roughness: 0.55,
				metalness: 0.05
			});
		const frontFace = new THREE.Mesh(new THREE.PlaneGeometry(CARD.w, CARD.h), face(front));
		frontFace.position.z = CARD.d / 2 + 0.001;
		const backFace = new THREE.Mesh(new THREE.PlaneGeometry(CARD.w, CARD.h), face(back));
		backFace.position.z = -CARD.d / 2 - 0.001;
		backFace.rotation.y = Math.PI;
		const clip = new THREE.Mesh(
			new THREE.BoxGeometry(0.26, 0.3, 0.06),
			new THREE.MeshStandardMaterial({ color: 0xc9c9d6, metalness: 0.9, roughness: 0.3 })
		);
		clip.position.y = CARD.h / 2 + 0.2;
		cardGroup.add(slab, frontFace, backFace, clip);
		scene.add(cardGroup);

		const bandMaterial = new MeshLineMaterial({
			color: 0xffffff,
			map: band,
			useMap: 1,
			repeat: new THREE.Vector2(-3, 1),
			lineWidth: 0.45,
			resolution: new THREE.Vector2(1, 1)
		});
		const bandGeometry = new MeshLineGeometry();
		const bandMesh = new THREE.Mesh(bandGeometry, bandMaterial);
		bandMesh.frustumCulled = false;
		scene.add(bandMesh);

		// Physics: a fixed anchor, three rope segments and the card on a ball joint.
		const world = new RAPIER.World({ x: 0, y: -40, z: 0 });
		world.numSolverIterations = 12;
		const make = (x: number, y: number, fixed = false) => {
			const desc = (fixed ? RAPIER.RigidBodyDesc.fixed() : RAPIER.RigidBodyDesc.dynamic())
				.setTranslation(x, y, 0)
				.setLinearDamping(2)
				.setAngularDamping(2);
			return world.createRigidBody(desc);
		};
		const spread = reduced ? 0 : 0.12;
		const hang = reduced ? ROPE : 0;
		const fixedBody = make(0, ANCHOR_Y, true);
		const joints = [1, 2, 3].map((i) => make(i * spread, ANCHOR_Y - i * hang));
		for (const body of joints)
			world.createCollider(
				RAPIER.ColliderDesc.ball(0.1).setDensity(10).setCollisionGroups(0),
				body
			);
		const card = make(4 * spread, ANCHOR_Y - 3 * hang - CLIP_Y);
		world.createCollider(
			RAPIER.ColliderDesc.cuboid(CARD.w / 2, CARD.h / 2, CARD.d / 2)
				.setDensity(0.5)
				.setCollisionGroups(0),
			card
		);
		const origin = { x: 0, y: 0, z: 0 };
		const chain = [fixedBody, ...joints];
		for (let i = 0; i < 3; i++)
			world.createImpulseJoint(
				RAPIER.JointData.rope(ROPE, origin, origin),
				chain[i],
				chain[i + 1],
				true
			);
		world.createImpulseJoint(
			RAPIER.JointData.spherical(origin, { x: 0, y: CLIP_Y, z: 0 }),
			joints[2],
			card,
			true
		);

		const anchor = new THREE.Vector3();
		const render = () => {
			const t = card.translation();
			const q = card.rotation();
			cardGroup.position.set(t.x, t.y, t.z);
			cardGroup.quaternion.set(q.x, q.y, q.z, q.w);
			anchor.set(0, CLIP_Y, 0).applyQuaternion(cardGroup.quaternion).add(cardGroup.position);
			const points = chain.map((body) => {
				const p = body.translation();
				return new THREE.Vector3(p.x, p.y, p.z);
			});
			points.push(anchor.clone());
			bandGeometry.setPoints(new THREE.CatmullRomCurve3(points, false, 'chordal').getPoints(40));
			renderer.render(scene, camera);
		};

		const step = 1 / 60;
		let frame = 0;
		let last = performance.now();
		let carry = 0;
		let dragging = false;
		const target = new THREE.Vector3();
		const reach = 3 * ROPE + CLIP_Y - 0.4;
		const tick = (now: number) => {
			frame = requestAnimationFrame(tick);
			carry += Math.min((now - last) / 1000, 0.1);
			last = now;
			while (carry >= step) {
				if (dragging) {
					// Chase the pointer in small strokes so the ropes never see a sudden jump.
					const t = card.translation();
					const move = new THREE.Vector3(target.x - t.x, target.y - t.y, 0).multiplyScalar(0.35);
					move.clampLength(0, 0.4);
					card.setNextKinematicTranslation({ x: t.x + move.x, y: t.y + move.y, z: 0 });
				} else {
					// Ease the card back to facing front instead of spinning forever.
					const spin = card.angvel();
					const rot = card.rotation();
					card.setAngvel({ x: spin.x, y: spin.y - rot.y * 0.25, z: spin.z }, true);
				}
				world.step();
				carry -= step;
			}
			render();
		};
		const run = () => {
			if (frame) return;
			last = performance.now();
			frame = requestAnimationFrame(tick);
		};

		// Dragging the card: it follows the pointer on the z = 0 plane while kinematic.
		const raycaster = new THREE.Raycaster();
		const plane = new THREE.Plane(new THREE.Vector3(0, 0, 1), 0);
		const grab = new THREE.Vector3();
		const point = (event: PointerEvent) => {
			const rect = canvas.getBoundingClientRect();
			raycaster.setFromCamera(
				new THREE.Vector2(
					((event.clientX - rect.left) / rect.width) * 2 - 1,
					-((event.clientY - rect.top) / rect.height) * 2 + 1
				),
				camera
			);
			return raycaster.ray.intersectPlane(plane, new THREE.Vector3());
		};
		const ndc = (x: number, y: number) => {
			raycaster.setFromCamera(new THREE.Vector2(x, y), camera);
			return raycaster.ray.intersectPlane(plane, new THREE.Vector3());
		};
		const overCard = () => raycaster.intersectObject(cardGroup, true).length > 0;
		const onDown = (event: PointerEvent) => {
			const at = point(event);
			if (!at || !overCard()) return;
			dragging = true;
			grab.copy(at).sub(cardGroup.position);
			target.copy(cardGroup.position);
			canvas.setPointerCapture(event.pointerId);
			canvas.style.cursor = 'grabbing';
			card.setBodyType(RAPIER.RigidBodyType.KinematicPositionBased, true);
			run();
		};
		const onMove = (event: PointerEvent) => {
			const at = point(event);
			if (!at) return;
			if (dragging) {
				target.copy(at).sub(grab);
				// Stay within what the rope can reach from its fixed end.
				const away = new THREE.Vector3(target.x, target.y - ANCHOR_Y, 0);
				away.clampLength(0, reach);
				target.set(away.x, ANCHOR_Y + away.y, 0);
				// Keep the whole card inside the panel.
				const lo = ndc(-1, -1);
				const hi = ndc(1, 1);
				if (lo && hi) {
					target.x = THREE.MathUtils.clamp(target.x, lo.x + CARD.w * 0.6, hi.x - CARD.w * 0.6);
					target.y = THREE.MathUtils.clamp(target.y, lo.y + CARD.h * 0.6, hi.y - CARD.h * 0.6);
				}
			} else canvas.style.cursor = overCard() ? 'grab' : '';
		};
		const onUp = () => {
			if (!dragging) return;
			dragging = false;
			canvas.style.cursor = '';
			card.setBodyType(RAPIER.RigidBodyType.Dynamic, true);
			const v = card.linvel();
			const speed = Math.hypot(v.x, v.y);
			const cap = 6;
			card.setLinvel(
				speed > cap
					? { x: (v.x / speed) * cap, y: (v.y / speed) * cap, z: 0 }
					: { x: v.x, y: v.y, z: 0 },
				true
			);
			card.setAngvel({ x: 0, y: 0, z: 0 }, true);
		};
		canvas.addEventListener('pointerdown', onDown);
		canvas.addEventListener('pointermove', onMove);
		canvas.addEventListener('pointerup', onUp);
		canvas.addEventListener('pointercancel', onUp);

		const resize = () => {
			const { clientWidth: w, clientHeight: h } = host;
			if (!w || !h) return;
			renderer.setSize(w, h, false);
			camera.aspect = w / h;
			camera.setViewOffset(w, h, -w * 0.25, 0, w, h);
			camera.updateProjectionMatrix();
			bandMaterial.resolution.set(w, h);
			if (!frame) render();
		};
		const observer = new ResizeObserver(resize);
		observer.observe(host);
		resize();

		// Reduced motion: show the badge hanging still; physics only runs once it is grabbed.
		if (reduced) render();
		else run();
		host.dataset.ready = 'true';

		return () => {
			cancelAnimationFrame(frame);
			observer.disconnect();
			canvas.remove();
			for (const object of [slab, frontFace, backFace, clip, bandMesh]) {
				object.geometry.dispose();
				(object.material as Material).dispose();
			}
			[front, back, band].forEach((map) => map.dispose());
			renderer.dispose();
			world.free();
		};
	}

	onMount(() => {
		if (!matchMedia('(min-width: 1024px)').matches) return;
		let stop: (() => void) | undefined;
		let gone = false;
		start(host).then((dispose) => {
			if (gone) dispose();
			else stop = dispose;
		});
		return () => {
			gone = true;
			stop?.();
		};
	});
</script>

<div bind:this={host} class="h-full w-full" aria-hidden="true"></div>
