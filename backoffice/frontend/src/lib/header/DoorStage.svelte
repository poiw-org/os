<script lang="ts" module>
	/** What the remote unlock is doing, as the door scene shows it. */
	export type DoorScene =
		| 'idle'
		| 'connecting'
		| 'sending'
		| 'warning'
		| 'opening'
		| 'opened'
		| 'failed_net'
		| 'failed_door';
</script>

<script lang="ts">
	import { onMount } from 'svelte';

	// The door, its NFC terminal and the two cables in 3D, in the same language as the tag stage.
	// The internet lights up and sends packets down its cable to the terminal; the terminal beats
	// like a speaker while the warning tone plays; a packet runs down the lock cable, the bolt slides
	// back and the door swings open onto a lit doorway. A failure knocks the side that failed.
	let { scene }: { scene: DoorScene } = $props();

	let host: HTMLDivElement;
	let loaded = $state(false);
	let unsupported = $state(false);
	let api: { set: (scene: DoorScene) => void; dispose: () => void } | undefined;

	const IDLE = '--color-gray-400';
	const BUSY = '--color-brand-text';
	const OK = '--color-green-700';
	const BAD = '--color-red-500';
	// Colour of each part per scene: the internet side, the terminal, the lock.
	const TONES: Record<DoorScene, [net: string, terminal: string, lock: string]> = {
		idle: [IDLE, IDLE, IDLE],
		connecting: [BUSY, IDLE, IDLE],
		sending: [BUSY, BUSY, IDLE],
		warning: [BUSY, '--color-yellow-500', IDLE],
		opening: [BUSY, OK, OK],
		opened: [OK, OK, OK],
		failed_net: [BAD, IDLE, IDLE],
		failed_door: [BUSY, BAD, IDLE]
	};

	// The warning tone's beat, in seconds.
	const BEEP_S = 0.6;
	// When the door is told to open: the packet reaches the lock, the bolt slides, the door swings.
	const BOLT_AT = 0.35;
	const SWING_AT = 0.55;

	async function start(host: HTMLDivElement) {
		const [
			THREE,
			{ RoundedBoxGeometry },
			{
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

		const stage = createStage(host, {
			target: [-1.25, 1.3, 0],
			from: [0.22, 0.2, 1],
			distance: (aspect) => Math.max(7, 11.2 / aspect)
		});
		if (!stage) return undefined;
		const { world, reduced } = stage;

		const v = (x: number, y: number, z: number) => new THREE.Vector3(x, y, z);
		const solid = (colour: string, roughness = 0.6) =>
			new THREE.MeshStandardMaterial({ color: tokenColour(colour), roughness });
		const basic = (options: ConstructorParameters<typeof THREE.MeshBasicMaterial>[0] = {}) =>
			new THREE.MeshBasicMaterial({ transparent: true, depthWrite: false, ...options });
		const box = (w: number, h: number, d: number, material: InstanceType<typeof THREE.Material>) =>
			new THREE.Mesh(new RoundedBoxGeometry(w, h, d, 2, Math.min(w, h, d) / 4), material);

		const rig = new THREE.Group();
		world.add(rig);

		// The wall, and the doorway behind the door: dark while shut, lit once it opens.
		const wall = new THREE.Mesh(new THREE.BoxGeometry(8, 4, 0.1), solid('--color-paper', 0.9));
		wall.position.set(-1.3, 1.4, -0.06);
		const dark = tokenColour('--color-void');
		const doorwayMaterial = new THREE.MeshBasicMaterial({ color: dark.clone() });
		const doorway = new THREE.Mesh(new THREE.PlaneGeometry(1.5, 2.6), doorwayMaterial);
		doorway.position.set(0, 1.3, 0.001);
		const floor = new THREE.Mesh(
			new THREE.BoxGeometry(8, 0.06, 1.4),
			solid('--color-paper-2', 0.9)
		);
		floor.position.set(-1.3, -0.03, 0.6);
		rig.add(wall, doorway, floor);

		// The frame, and the door on its hinge at the right jamb.
		const frameMaterial = solid('--color-gray-400', 0.5);
		for (const [w, h, x, y] of [
			[0.12, 2.72, -0.81, 1.36],
			[0.12, 2.72, 0.81, 1.36],
			[1.74, 0.12, 0, 2.66]
		]) {
			const part = box(w, h, 0.24, frameMaterial);
			part.position.set(x, y, 0.07);
			rig.add(part);
		}
		const hinge = new THREE.Group();
		hinge.position.set(0.75, 0, 0.05);
		const leaf = box(1.5, 2.58, 0.07, solid('--color-gray-300'));
		leaf.position.set(-0.75, 1.3, 0);
		const panelMaterial = solid('--color-gray-200');
		for (const y of [1.9, 0.72]) {
			const panel = box(1.1, 0.9, 0.02, panelMaterial);
			panel.position.set(-0.75, y, 0.04);
			hinge.add(panel);
		}
		const metal = new THREE.MeshStandardMaterial({
			color: tokenColour('--color-ink'),
			metalness: 0.6,
			roughness: 0.3
		});
		const handle = box(0.06, 0.26, 0.07, metal);
		handle.position.set(-1.3, 1.2, 0.07);
		hinge.add(leaf, handle);
		rig.add(hinge);

		// The lock on the left jamb, with a bolt across the door's edge and a status light.
		const lock = box(0.22, 0.38, 0.1, solid('--color-gray-200'));
		lock.position.set(-0.81, 1.2, 0.24);
		const bolt = box(0.22, 0.07, 0.06, metal);
		bolt.position.set(-0.6, 1.14, 0.24);
		const lockLedMaterial = basic();
		const lockLed = new THREE.Mesh(new THREE.CircleGeometry(0.035, 20), lockLedMaterial);
		lockLed.position.set(-0.81, 1.31, 0.292);
		rig.add(lock, bolt, lockLed);

		// The terminal on the wall: an NFC mark on its face, a speaker grille and a light.
		const white = 'rgb(255 255 255)';
		const faceMap = canvasTexture(330, 510, (c) => {
			c.strokeStyle = c.fillStyle = white;
			c.lineWidth = 8;
			c.globalAlpha = 0.5;
			c.beginPath();
			c.roundRect(34, 34, 262, 262, 18);
			c.stroke();
			c.globalAlpha = 1;
			c.lineCap = 'round';
			for (const radius of [26, 52, 78]) {
				c.beginPath();
				c.arc(120, 165, radius, -0.8, 0.8);
				c.stroke();
			}
			c.globalAlpha = 0.6;
			for (let row = 0; row < 3; row++)
				for (let col = 0; col < 5; col++) {
					c.beginPath();
					c.arc(105 + col * 30, 370 + row * 30, 6, 0, Math.PI * 2);
					c.fill();
				}
		});
		const terminal = new THREE.Group();
		terminal.position.set(-1.75, 1.35, 0.08);
		const terminalFace = basic({ map: faceMap });
		const face = new THREE.Mesh(new THREE.PlaneGeometry(0.5, 0.78), terminalFace);
		face.position.z = 0.062;
		const terminalLedMaterial = basic();
		const terminalLed = new THREE.Mesh(new THREE.CircleGeometry(0.03, 20), terminalLedMaterial);
		terminalLed.position.set(0.17, 0.33, 0.063);
		terminal.add(box(0.56, 0.86, 0.12, solid('--color-gray-300')), face, terminalLed);
		rig.add(terminal);

		// Sound leaving the speaker while the warning tone plays.
		const sounds = [0, 1, 2].map(() => {
			const material = basic({ side: THREE.DoubleSide });
			const ring = new THREE.Mesh(new THREE.RingGeometry(0.3, 0.335, 64), material);
			ring.position.set(-1.75, 1.1, 0.16);
			ring.visible = false;
			rig.add(ring);
			return { ring, material, born: -1 };
		});

		// The internet: a wireframe globe up and to the left.
		const globe = new THREE.Group();
		globe.position.set(-3.15, 2.05, 0.35);
		const globeMaterial = new THREE.LineBasicMaterial({ transparent: true });
		globe.add(
			new THREE.Mesh(new THREE.SphereGeometry(0.33, 24, 16), solid('--color-paper')),
			new THREE.LineSegments(
				new THREE.EdgesGeometry(new THREE.IcosahedronGeometry(0.35, 1)),
				globeMaterial
			)
		);
		rig.add(globe);

		// The cables, and the packets that travel along them.
		const cableMaterial = solid('--color-gray-400', 0.45);
		const cable = (points: InstanceType<typeof THREE.Vector3>[]) => {
			const curve = new THREE.CatmullRomCurve3(points);
			rig.add(new THREE.Mesh(new THREE.TubeGeometry(curve, 64, 0.026, 8), cableMaterial));
			return curve;
		};
		const netCable = cable([
			v(-2.03, 1.55, 0.08),
			v(-2.4, 1.36, 0.15),
			v(-2.85, 1.45, 0.25),
			v(-3.15, 1.71, 0.35)
		]);
		const lockCable = cable([
			v(-1.47, 1.2, 0.08),
			v(-1.3, 0.92, 0.12),
			v(-1.05, 0.96, 0.2),
			v(-0.92, 1.13, 0.24)
		]);
		const packet = () => {
			const material = basic({ depthTest: false });
			const mesh = new THREE.Mesh(new THREE.SphereGeometry(0.055, 16, 12), material);
			mesh.renderOrder = 5;
			mesh.visible = false;
			rig.add(mesh);
			return { mesh, material };
		};
		const netPackets = [packet(), packet(), packet()];
		const lockPacket = packet();
		// Places a packet a fraction `t` along a cable (0 = terminal end); hidden outside 0–1.
		const travel = (
			p: ReturnType<typeof packet>,
			curve: InstanceType<typeof THREE.CatmullRomCurve3>,
			t: number
		) => {
			p.mesh.visible = !reduced && t >= 0 && t <= 1;
			if (!p.mesh.visible) return;
			curve.getPointAt(t, p.mesh.position);
			p.mesh.scale.setScalar(Math.sin(Math.PI * t) ** 0.5);
		};

		// Springs. The rig rises into place as the modal opens; the door swings with a little give,
		// as a real door does; everything else settles without overshoot.
		const lift = spring(-0.3, 0.6);
		const swing = spring(0, 0.8, 0.72);
		const slide = spring(0, 0.25);
		const beat = spring(1, 0.2, 0.35);
		const spin = spring(0, 0.8);
		const netKnock = spring(0, 0.24, 0.28);
		const doorKnock = spring(0, 0.24, 0.28);
		const springs = [lift, swing, slide, beat, spin, netKnock, doorKnock];
		lift.target = 0;

		const tones = TONES.idle.map((name) => tokenColour(name));
		const toneTargets = tones.map((c) => c.clone());
		const [netTone, terminalTone, lockTone] = tones;
		const lit = tokenColour(OK);

		let current: DoorScene = 'idle';
		let since = 0; // when the current scene began, in seconds
		let clock = 0;
		let lastBeep = -1;
		let angle = 0;

		stage.play((dt) => {
			clock += dt;
			const elapsed = clock - since;
			const open = current === 'opening' || current === 'opened';
			const late = reduced || current === 'opened';
			swing.target = open && (late || elapsed > SWING_AT) ? 1.05 : 0;
			slide.target = open && (late || elapsed > BOLT_AT) ? 1 : 0;
			spin.target =
				current === 'connecting' || current === 'sending' || current === 'opening' ? 1.4 : 0;

			// The warning tone: each beep pushes the terminal out a touch and sends a ring of sound.
			if (current === 'warning' && !reduced) {
				const count = Math.floor(elapsed / BEEP_S);
				if (count !== lastBeep) {
					lastBeep = count;
					beat.velocity = 1.6;
					sounds[count % sounds.length].born = elapsed;
				}
			}

			let moving = false;
			if (reduced) {
				snapSprings(springs);
				tones.forEach((c, i) => c.copy(toneTargets[i]));
			} else {
				stepSprings(springs, dt);
				moving = tones.map((c, i) => easeColour(c, toneTargets[i], dt)).some(Boolean);
			}

			rig.position.y = lift.value;
			hinge.rotation.y = swing.value;
			bolt.position.x = -0.6 - 0.2 * slide.value;
			terminal.scale.setScalar(beat.value);
			terminal.position.x = -1.75 + doorKnock.value;
			globe.position.x = -3.15 + netKnock.value;
			angle += spin.value * dt;
			globe.rotation.set(0.35, angle, 0);

			globeMaterial.color.copy(netTone);
			terminalFace.color.copy(terminalTone);
			terminalLedMaterial.color.copy(terminalTone);
			lockLedMaterial.color.copy(lockTone);
			doorwayMaterial.color.copy(dark).lerp(lit, 0.45 * THREE.MathUtils.clamp(swing.value, 0, 1));
			// The terminal's light breathes while it waits for the door's answer.
			terminalLedMaterial.opacity =
				current === 'sending' && !reduced
					? 0.35 + 0.65 * (0.5 + 0.5 * Math.cos(elapsed * Math.PI * 2))
					: 1;

			// Packets: down from the internet while the command goes out, one back up when the door
			// confirms, and one along the lock cable when it opens.
			netPackets.forEach((p, i) => {
				p.material.color.copy(netTone);
				let t = -1;
				if (current === 'sending') t = 1 - ((elapsed / 1.1 + i / 3) % 1);
				else if (current === 'warning' && i === 0) t = elapsed / 0.6;
				travel(p, netCable, t);
			});
			lockPacket.material.color.copy(lockTone);
			travel(lockPacket, lockCable, current === 'opening' ? elapsed / BOLT_AT : -1);

			sounds.forEach((s, i) => {
				s.material.color.copy(terminalTone);
				// Reduced motion keeps one still ring so the tone still reads.
				const phase =
					current !== 'warning'
						? -1
						: reduced
							? i === 0
								? 0.4
								: -1
							: s.born >= 0
								? (elapsed - s.born) / 0.9
								: -1;
				s.ring.visible = phase >= 0 && phase < 1;
				if (!s.ring.visible) return;
				const ease = 1 - (1 - phase) ** 3;
				s.ring.scale.setScalar(0.6 + 1.3 * ease);
				s.material.opacity = reduced ? 0.5 : 0.9 * (1 - phase) ** 1.5;
			});

			return (
				current === 'connecting' ||
				current === 'sending' ||
				current === 'warning' ||
				(current === 'opening' && elapsed < 2) ||
				!settled(springs) ||
				moving
			);
		});

		return {
			set(scene: DoorScene) {
				if (scene === current) return;
				current = scene;
				since = clock;
				lastBeep = -1;
				for (const s of sounds) s.born = -1;
				TONES[scene].forEach((name, i) => toneTargets[i].copy(tokenColour(name)));
				if (!reduced && scene === 'failed_net') netKnock.velocity = 4;
				if (!reduced && scene === 'failed_door') doorKnock.velocity = 4;
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
			// The scene only illustrates the command; without it the modal still works.
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
	class="h-52 w-full overflow-hidden rounded-md border bg-paper-2 transition-opacity duration-300
		{loaded ? 'opacity-100' : 'opacity-0'}"
	class:hidden={unsupported}
	data-scene={scene}
	aria-hidden="true"
></div>
