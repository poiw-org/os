<script lang="ts" module>
	import type { WriteCardState } from '#lib/api.ts';

	export type TagScene = 'idle' | 'connecting' | WriteCardState;
</script>

<script lang="ts">
	import { onMount } from 'svelte';

	// The door's reader and the tag in 3D. The scene reports what the door is doing: the tag drifts
	// toward the reader while it waits, docks while it writes, is knocked back once for the wrong tag,
	// and a check pops when it is done. Travel is critically damped; only the check may bounce.
	let { scene, label }: { scene: TagScene; label: string } = $props();

	let host: HTMLDivElement;
	let loaded = $state(false);
	let unsupported = $state(false);
	let api: { set: (scene: TagScene) => void; dispose: () => void } | undefined;

	type Pose = { y: number; z: number; rx: number; rz: number; opacity: number };
	const POSES = {
		away: { y: 1.8, z: -0.5, rx: 0.4, rz: -0.18, opacity: 0.6 },
		far: { y: 1.25, z: 0, rx: 0.26, rz: -0.12, opacity: 0.9 },
		near: { y: 0.5, z: 0, rx: 0.08, rz: -0.035, opacity: 1 },
		docked: { y: 0.07, z: 0, rx: 0, rz: 0, opacity: 1 }
	} satisfies Record<string, Pose>;

	const TONES: Record<TagScene, string> = {
		idle: '--color-gray-400',
		connecting: '--color-gray-400',
		ready: '--color-ink',
		wrong_tag: '--color-ink',
		writing: '--color-yellow-500',
		done: '--color-green-700',
		failed: '--color-red-500'
	};

	// One slow approach-and-back while the door waits, in seconds.
	const DRIFT_S = 2.8;
	const CARD = { w: 2, h: 1.26, d: 0.05 };
	const PAD = { w: 3.4, h: 0.36, d: 2.2 };

	async function start(host: HTMLDivElement) {
		const [
			THREE,
			{ RoundedBoxGeometry },
			{
				token,
				tokenColour,
				canvasTexture,
				createStage,
				spring,
				stepSprings,
				snapSprings,
				settled,
				easeColour
			}
		] = await Promise.all([
			import('three'),
			import('three/examples/jsm/geometries/RoundedBoxGeometry.js'),
			import('#lib/three.ts')
		]);
		await document.fonts.load('400 64px "Space Mono"');

		const stage = createStage(host, {
			target: [0, 0.55, 0],
			from: [0, 4.45, 6.6],
			distance: (aspect) => Math.max(8.2, 14.5 / aspect)
		});
		if (!stage) return undefined;
		const { world, reduced } = stage;

		const white = 'rgb(255 255 255)';
		const outline = (c: CanvasRenderingContext2D, ...rect: [number, number, number, number]) => {
			c.beginPath();
			c.roundRect(...rect, 12);
			c.stroke();
		};
		const plane = (w: number, h: number, material: InstanceType<typeof THREE.Material>) => {
			const mesh = new THREE.Mesh(new THREE.PlaneGeometry(w, h), material);
			mesh.rotation.x = -Math.PI / 2;
			return mesh;
		};
		const basic = (options: ConstructorParameters<typeof THREE.MeshBasicMaterial>[0] = {}) =>
			new THREE.MeshBasicMaterial({ transparent: true, depthWrite: false, ...options });

		// The reader: a slate slab with its antenna coil printed on top and a status light.
		const coilMap = canvasTexture(1024, 640, (c) => {
			c.strokeStyle = white;
			c.lineWidth = 6;
			for (const inset of [0.16, 0.2, 0.24]) {
				c.globalAlpha = 1.05 - 2 * inset;
				outline(c, 1024 * inset, 640 * inset, 1024 * (1 - 2 * inset), 640 * (1 - 2 * inset));
			}
			c.globalAlpha = 0.8;
			c.fillStyle = white;
			c.font = '400 38px "Space Mono"';
			c.fillText('PN532', 44, 600);
		});
		const pad = new THREE.Mesh(
			new RoundedBoxGeometry(PAD.w, PAD.h, PAD.d, 4, 0.1),
			new THREE.MeshStandardMaterial({ color: tokenColour('--color-gray-300'), roughness: 0.6 })
		);
		pad.position.y = -PAD.h / 2;
		const coilMaterial = basic({ map: coilMap });
		const coil = plane(PAD.w - 0.2, PAD.d - 0.2, coilMaterial);
		coil.position.y = 0.003;
		const ledMaterial = basic();
		const led = new THREE.Mesh(new THREE.CircleGeometry(0.07, 24), ledMaterial);
		led.rotation.x = -Math.PI / 2;
		led.position.set(PAD.w / 2 - 0.25, 0.004, -PAD.d / 2 + 0.25);

		// A soft contact shadow that tightens and darkens as the tag comes down to the reader.
		const shadowMap = canvasTexture(256, 256, (c) => {
			const gradient = c.createRadialGradient(128, 128, 0, 128, 128, 128);
			gradient.addColorStop(0, 'rgb(0 0 0 / 1)');
			gradient.addColorStop(1, 'rgb(0 0 0 / 0)');
			c.fillStyle = gradient;
			c.fillRect(0, 0, 256, 256);
		});
		const shadowMaterial = basic({ map: shadowMap, color: 0 });
		const shadow = plane(CARD.w * 1.25, CARD.h * 1.35, shadowMaterial);
		world.add(pad, coil, shadow, led);

		// Contactless waves rising off the coil.
		const waves = [0, 1, 2].map(() => {
			const material = basic({ side: THREE.DoubleSide });
			const ring = new THREE.Mesh(new THREE.RingGeometry(0.9, 1, 96), material);
			ring.rotation.x = -Math.PI / 2;
			ring.visible = false;
			world.add(ring);
			return { ring, material };
		});

		// The tag: a light card, coloured by the state, with its chip and serial printed dark on top.
		const ink = token('--color-void');
		const faceMap = canvasTexture(1024, 645, (c) => {
			c.strokeStyle = c.fillStyle = ink;
			c.globalAlpha = 0.35;
			c.lineWidth = 6;
			outline(c, 18, 18, 988, 609);
			c.globalAlpha = 0.7;
			outline(c, 820, 70, 124, 92);
			c.globalAlpha = 1;
			c.lineCap = 'round';
			for (const radius of [26, 52, 78]) {
				c.beginPath();
				c.arc(96, 150, radius, -0.75, 0.75);
				c.stroke();
			}
			c.globalAlpha = 0.85;
			c.font = '400 64px "Space Mono"';
			c.fillText(label, 70, 572);
		});
		const sheenMap = canvasTexture(512, 4, (c) => {
			const gradient = c.createLinearGradient(0, 0, 512, 0);
			gradient.addColorStop(0.35, 'rgb(255 255 255 / 0)');
			gradient.addColorStop(0.5, white);
			gradient.addColorStop(0.65, 'rgb(255 255 255 / 0)');
			c.fillStyle = gradient;
			c.fillRect(0, 0, 512, 4);
		});
		sheenMap.center.set(0.5, 0.5);
		sheenMap.rotation = 0.3;

		const tag = new THREE.Group();
		const slabMaterial = new THREE.MeshStandardMaterial({ roughness: 0.45, transparent: true });
		const slab = new THREE.Mesh(
			new RoundedBoxGeometry(CARD.w, CARD.d, CARD.h, 2, 0.02),
			slabMaterial
		);
		const faceMaterial = basic({ map: faceMap });
		const face = plane(CARD.w, CARD.h, faceMaterial);
		face.position.y = CARD.d / 2 + 0.002;
		const sheen = plane(CARD.w - 0.06, CARD.h - 0.06, basic({ map: sheenMap, opacity: 0.6 }));
		sheen.position.y = CARD.d / 2 + 0.004;
		sheen.visible = false;
		tag.add(slab, face, sheen);
		world.add(tag);

		// Done: a check that pops over the docked tag.
		const badgeMap = canvasTexture(256, 256, (c) => {
			c.fillStyle = ink;
			c.strokeStyle = token('--color-green-700');
			c.lineWidth = 10;
			c.beginPath();
			c.arc(128, 128, 114, 0, Math.PI * 2);
			c.fill();
			c.stroke();
			c.lineWidth = 24;
			c.lineCap = c.lineJoin = 'round';
			c.beginPath();
			c.moveTo(76, 132);
			c.lineTo(112, 168);
			c.lineTo(182, 94);
			c.stroke();
		});
		const badge = new THREE.Sprite(
			new THREE.SpriteMaterial({ map: badgeMap, depthTest: false, transparent: true })
		);
		badge.renderOrder = 10;
		badge.scale.setScalar(0);
		world.add(badge);

		// The tag starts above its first pose, invisible, and settles in as the modal opens.
		const pose = Object.fromEntries(
			Object.entries(POSES.away).map(([key, value]) => [
				key,
				spring(key === 'opacity' ? 0 : key === 'y' ? value + 0.8 : value, 0.55)
			])
		) as Record<keyof Pose, ReturnType<typeof spring>>;
		const pop = spring(0, 0.4, 0.62);
		// Sideways knock for the wrong tag: a kick of velocity on a loose spring that rings out.
		const knock = spring(0, 0.24, 0.28);
		const springs = [...Object.values(pose), pop, knock];

		const tone = tokenColour(TONES.idle);
		const toneTarget = tone.clone();
		const toned = [coilMaterial, ledMaterial, slabMaterial, ...waves.map((w) => w.material)];

		let current: TagScene = 'idle';
		let since = 0; // when the current scene began, in seconds
		let clock = 0;
		const mix = (a: Pose, b: Pose, t: number) =>
			Object.fromEntries(
				Object.keys(a).map((key) => [
					key,
					a[key as keyof Pose] * (1 - t) + b[key as keyof Pose] * t
				])
			) as Pose;
		const poseFor = (scene: TagScene, elapsed: number): Pose => {
			switch (scene) {
				case 'connecting':
					return POSES.far;
				case 'ready':
					// Drift down toward the reader and ease back, as a hand offering the tag would.
					return reduced
						? POSES.near
						: mix(
								POSES.near,
								POSES.far,
								0.5 + 0.5 * Math.cos((elapsed / DRIFT_S) * Math.PI * 2 + 0.6)
							);
				case 'wrong_tag':
					return POSES.near;
				case 'writing':
				case 'done':
					return POSES.docked;
				default:
					return POSES.away;
			}
		};
		const waveMode = (scene: TagScene) =>
			scene === 'writing'
				? 'fast'
				: scene === 'ready' || scene === 'wrong_tag'
					? 'slow'
					: scene === 'done'
						? 'burst'
						: 'off';

		stage.play((dt) => {
			clock += dt;
			const elapsed = clock - since;
			const target = poseFor(current, elapsed);
			for (const key of Object.keys(pose) as (keyof Pose)[]) pose[key].target = target[key];
			pop.target = current === 'done' ? 0.7 : 0;
			let fading = false;
			if (reduced) {
				snapSprings(springs);
				tone.copy(toneTarget);
			} else {
				stepSprings(springs, dt);
				fading = easeColour(tone, toneTarget, dt);
			}

			tag.position.set(knock.value, pose.y.value, pose.z.value);
			tag.rotation.set(pose.rx.value, -knock.value * 0.4, pose.rz.value);
			slabMaterial.opacity = faceMaterial.opacity = pose.opacity.value;

			// 0 when the tag is high, 1 when it rests on the reader.
			const close = THREE.MathUtils.clamp(1 - (pose.y.value - POSES.docked.y) / 1.7, 0, 1);
			shadow.position.set(tag.position.x, 0.005, tag.position.z);
			shadow.scale.setScalar(1.45 - 0.45 * close);
			shadowMaterial.opacity = (0.12 + 0.5 * close * close) * pose.opacity.value;

			for (const material of toned) material.color.copy(tone);
			// The light breathes while the door waits and quickens while it writes; no hard strobe.
			const pulse = current === 'writing' ? 0.5 : 1.4;
			ledMaterial.opacity =
				reduced || (current !== 'ready' && current !== 'wrong_tag' && current !== 'writing')
					? 1
					: 0.35 + 0.65 * (0.5 + 0.5 * Math.cos((elapsed / pulse) * Math.PI * 2));

			sheen.visible = !reduced && current === 'writing';
			if (sheen.visible) sheenMap.offset.x = 1 - 2 * ((elapsed % 0.9) / 0.9);

			const mode = waveMode(current);
			waves.forEach(({ ring, material }, i) => {
				let phase = -1;
				if (reduced) phase = mode !== 'off' && i === 0 ? 0.45 : -1;
				else if (mode === 'slow') phase = (elapsed / 2.4 + i / 3) % 1;
				else if (mode === 'fast') phase = (elapsed / 0.9 + i / 3) % 1;
				else if (mode === 'burst') phase = (elapsed - i * 0.12) / 1.1;
				ring.visible = phase >= 0 && phase < 1;
				if (!ring.visible) return;
				const ease = 1 - (1 - phase) ** 3;
				ring.scale.setScalar(0.4 + 0.95 * ease);
				ring.position.y = 0.02 + 0.4 * ease;
				material.opacity = reduced ? 0.5 : 0.9 * (1 - phase) ** 1.5;
			});

			badge.scale.setScalar(Math.max(0, pop.value));
			badge.position.set(tag.position.x, tag.position.y + 0.55, tag.position.z + 0.1);

			// Keep animating only while something moves.
			return (
				mode === 'slow' ||
				mode === 'fast' ||
				(mode === 'burst' && elapsed < 1.5) ||
				!settled(springs) ||
				fading
			);
		});

		return {
			set(scene: TagScene) {
				if (scene === current) return;
				current = scene;
				since = clock;
				toneTarget.copy(tokenColour(TONES[scene]));
				if (scene === 'wrong_tag' && !reduced) knock.velocity = 5;
				stage.wake();
			},
			dispose: stage.dispose
		};
	}

	onMount(() => {
		let gone = false;
		start(host)
			.then((started) => {
				if (!started) unsupported = true;
				else if (gone) started.dispose();
				else {
					api = started;
					started.set(scene);
					loaded = true;
				}
			})
			// The scene only illustrates the steps; without it the modal still works.
			.catch(() => (unsupported = true));
		return () => {
			gone = true;
			api?.dispose();
		};
	});

	$effect(() => {
		const next = scene;
		api?.set(next);
	});
</script>

<div
	bind:this={host}
	class="mt-6 h-48 overflow-hidden rounded-md border bg-paper-2 transition-opacity duration-300 sm:h-60
		{loaded ? 'opacity-100' : 'opacity-0'}"
	class:hidden={unsupported}
	data-scene={scene}
	aria-hidden="true"
></div>
