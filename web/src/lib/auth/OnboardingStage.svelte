<script lang="ts">
	import { onMount } from 'svelte';

	// A short story told across the first questions, in one long room the camera pans along: someone
	// waves hello, someone studies at a desk, two people chat until one of them brings up po/iw, someone
	// solders at a workbench, and a new account locks a padlock shut.
	let {
		scene,
		ended = false,
		knock = 0
	}: {
		scene: 0 | 1 | 2 | 3 | 4;
		/** Starts on the scene's last beat instead of playing it. */
		ended?: boolean;
		/** Increase to shake the room after a mistake. */
		knock?: number;
	} = $props();

	let host: HTMLDivElement;
	let loaded = $state(false);
	let unsupported = $state(false);
	let api: { set: (scene: number, knock: number) => void; dispose: () => void } | undefined;

	const SPACING = 10;
	const VIEW_FROM: [number, number, number] = [0.1, 0.2, 1];
	const LOOK_Y = 1.35;
	const PAN_S = 0.8;
	// The conversation, in seconds after the camera arrives.
	const FIRST_DOTS = 0.3;
	const REPLY_DOTS = 1.4;
	const SECOND_DOTS = 2.5;
	const LOGO = 3.6;
	const SURPRISE = 4.4;
	// The account scene, on the same clock.
	const EMAIL = 0.3;
	const PASSWORD = 1.4;
	const LOCK = 2.5;
	const CHECK = 2.9;
	const STORY_ENDS = 6;

	async function start(host: HTMLDivElement, firstScene: number, startAtEnd: boolean) {
		const [
			THREE,
			{ RoundedBoxGeometry },
			{ tokenColour, canvasTexture, createStage, spring, stepSprings, snapSprings, settled }
		] = await Promise.all([
			import('three'),
			import('three/examples/jsm/geometries/RoundedBoxGeometry.js'),
			import('#lib/three.ts')
		]);

		const distance = (aspect: number) => Math.max(6.7, 9 / aspect);
		const stage = createStage(host, { target: [0, LOOK_Y, 0], from: VIEW_FROM, distance });
		if (!stage) return undefined;
		const { world, camera, reduced } = stage;

		const solid = (colour: string | number, roughness = 0.65) =>
			new THREE.MeshStandardMaterial({
				color: typeof colour === 'string' ? tokenColour(colour) : colour,
				roughness
			});
		const box = (w: number, h: number, d: number, material: InstanceType<typeof THREE.Material>) =>
			new THREE.Mesh(new RoundedBoxGeometry(w, h, d, 2, Math.min(w, h, d) / 4), material);

		const room = new THREE.Group();
		world.add(room);

		const wall = new THREE.Mesh(new THREE.BoxGeometry(64, 6, 0.1), solid('--color-paper', 0.9));
		wall.position.set(SPACING * 2, 2, -2);
		const floor = new THREE.Mesh(new THREE.BoxGeometry(64, 0.06, 5), solid('--color-paper-2', 0.9));
		floor.position.set(SPACING * 2, -0.03, 0.4);
		room.add(wall, floor);

		// Picture frames between the scenes give the pan something to slide past.
		const frameMaterial = solid('--color-gray-400', 0.5);
		const artMaterials = ['--color-brand-text', '--color-yellow-500', '--color-green-700'].map(
			(colour) => solid(colour)
		);
		[-5, 5, 15, 25, 35, 45].forEach((x, i) => {
			const frame = box(1.3, 1.6, 0.06, frameMaterial);
			frame.position.set(x, 2, -1.92);
			const art = box(1.05, 1.35, 0.04, artMaterials[i % artMaterials.length]);
			art.position.set(x, 2, -1.88);
			room.add(frame, art);
		});

		const person = (shirt: string, skin: number, hair: number) => {
			const group = new THREE.Group();
			const shirtMaterial = solid(shirt);
			const skinMaterial = solid(skin, 0.75);
			const torso = new THREE.Mesh(new THREE.CapsuleGeometry(0.34, 0.75, 8, 24), shirtMaterial);
			torso.position.y = 0.75;

			const head = new THREE.Group();
			head.position.y = 1.72;
			const face = new THREE.Mesh(new THREE.SphereGeometry(0.33, 32, 24), skinMaterial);
			const hairCap = new THREE.Mesh(
				new THREE.SphereGeometry(0.345, 32, 16, 0, Math.PI * 2, 0, Math.PI * 0.47),
				solid(hair, 0.85)
			);
			hairCap.rotation.x = -0.25;
			const eyeMaterial = solid(0x1c1a24, 0.3);
			for (const x of [-0.11, 0.11]) {
				const eye = new THREE.Mesh(new THREE.SphereGeometry(0.035, 12, 12), eyeMaterial);
				eye.position.set(x, 0.03, 0.31);
				head.add(eye);
			}
			const smile = new THREE.Mesh(
				new THREE.TorusGeometry(0.09, 0.013, 8, 20, Math.PI * 0.8),
				eyeMaterial
			);
			smile.rotation.z = Math.PI + Math.PI * 0.1;
			smile.position.set(0, -0.06, 0.31);
			head.add(face, hairCap, smile);

			const arm = (side: number) => {
				const pivot = new THREE.Group();
				pivot.position.set(side * 0.42, 1.2, 0);
				const limb = new THREE.Mesh(new THREE.CapsuleGeometry(0.095, 0.5, 6, 16), shirtMaterial);
				limb.position.y = -0.3;
				const hand = new THREE.Mesh(new THREE.SphereGeometry(0.11, 16, 12), skinMaterial);
				hand.position.y = -0.66;
				pivot.add(limb, hand);
				group.add(pivot);
				return pivot;
			};

			group.add(torso, head);
			return { group, head, left: arm(-1), right: arm(1) };
		};

		// A speech bubble whose tail points at the person below it, drawn white in every theme.
		const bubbleShape = canvasTexture(320, 260, (c) => {
			c.fillStyle = 'rgb(255 255 255)';
			c.beginPath();
			c.roundRect(8, 8, 304, 196, 56);
			c.fill();
			c.beginPath();
			c.moveTo(128, 196);
			c.lineTo(160, 250);
			c.lineTo(192, 196);
			c.fill();
		});
		const bubble = (x: number, content: InstanceType<typeof THREE.Object3D>) => {
			const group = new THREE.Group();
			group.position.set(x, 2.62, 0.3);
			const back = new THREE.Mesh(
				new THREE.PlaneGeometry(1.15, 0.934),
				new THREE.MeshBasicMaterial({ map: bubbleShape, transparent: true, depthWrite: false })
			);
			content.position.set(0, 0.1, 0.01);
			group.add(back, content);
			group.visible = false;
			return { group, pop: spring(0, 0.3, 0.55) };
		};
		const flat = (map: InstanceType<typeof THREE.Texture>, w: number, h: number) =>
			new THREE.Mesh(
				new THREE.PlaneGeometry(w, h),
				new THREE.MeshBasicMaterial({ map, transparent: true, depthWrite: false })
			);
		const wording = (text: string, size: number) =>
			flat(
				canvasTexture(320, 200, (c) => {
					c.fillStyle = '#1c1a24';
					c.font = `800 ${size}px system-ui, sans-serif`;
					c.textAlign = 'center';
					c.textBaseline = 'middle';
					c.fillText(text, 160, 104);
				}),
				1.0,
				0.625
			);
		const dotsMaterial = new THREE.MeshBasicMaterial({ color: 0x1c1a24 });
		const typing = () => {
			const group = new THREE.Group();
			const dots = [-0.24, 0, 0.24].map((x) => {
				const dot = new THREE.Mesh(new THREE.CircleGeometry(0.075, 20), dotsMaterial);
				dot.position.x = x;
				group.add(dot);
				return dot;
			});
			return { group, dots };
		};

		// Scene 0: hello.
		const greeter = person('--color-brand-text', 0xe8b98f, 0x3a2a20);
		room.add(greeter.group);

		// Scene 1: at the desk, turning the pages of an open book.
		const studyAt = new THREE.Group();
		studyAt.position.x = SPACING;
		const reader = person('--color-yellow-500', 0x9c6b47, 0x17110d);
		reader.group.position.set(0, -0.08, -0.85);
		reader.head.rotation.x = 0.3;
		reader.left.rotation.set(-1.15, 0, 0.18);
		reader.right.rotation.set(-1.15, 0, -0.18);

		const wood = solid('--color-gray-400', 0.55);
		const top = box(2.9, 0.1, 1.25, wood);
		top.position.set(0, 0.95, -0.25);
		studyAt.add(reader.group, top);
		for (const [x, z] of [
			[-1.3, -0.75],
			[1.3, -0.75],
			[-1.3, 0.25],
			[1.3, 0.25]
		]) {
			const leg = box(0.09, 0.93, 0.09, wood);
			leg.position.set(x, 0.465, z);
			studyAt.add(leg);
		}

		const coverMaterial = solid('--color-brand-text');
		const pageMaterial = solid(0xf4f1e8, 0.9);
		const book = new THREE.Group();
		book.position.set(0, 1.01, 0.05);
		for (const side of [-1, 1]) {
			const cover = box(0.52, 0.03, 0.66, coverMaterial);
			cover.position.set(side * 0.27, 0, 0);
			cover.rotation.z = side * 0.06;
			const pages = box(0.47, 0.05, 0.6, pageMaterial);
			pages.position.set(side * 0.26, 0.035, 0);
			pages.rotation.z = side * 0.06;
			book.add(cover, pages);
		}
		const turningPage = new THREE.Group();
		turningPage.position.y = 0.075;
		const leaf = new THREE.Mesh(
			new THREE.BoxGeometry(0.46, 0.006, 0.58),
			new THREE.MeshStandardMaterial({ color: 0xffffff, roughness: 0.9 })
		);
		leaf.position.x = 0.24;
		turningPage.add(leaf);
		book.add(turningPage);
		studyAt.add(book);

		[
			['--color-green-700', 0],
			['--color-yellow-500', 0.11],
			['--color-gray-300', 0.22]
		].forEach(([colour, y]) => {
			const stacked = box(0.62, 0.1, 0.44, solid(colour as string));
			stacked.position.set(1.0, 1.05 + (y as number), -0.3);
			stacked.rotation.y = (y as number) * 2;
			studyAt.add(stacked);
		});
		room.add(studyAt);

		// Scene 2: the conversation. `bubble` positions are inside this group.
		const chatAt = new THREE.Group();
		chatAt.position.x = SPACING * 2;
		const speaker = person('--color-green-700', 0xf2d0ae, 0x7a4a24);
		speaker.group.position.x = -1.05;
		speaker.group.rotation.y = 0.65;
		const listener = person('--color-brand-text', 0xc58c5e, 0x24201e);
		listener.group.position.x = 1.05;
		listener.group.rotation.y = -0.65;

		const speakerDots = typing();
		const listenerDots = typing();
		const dotBubbles = [bubble(-1.25, speakerDots.group), bubble(1.25, listenerDots.group)];
		const logoContent = new THREE.Group();
		const logoBubble = bubble(-1.25, logoContent);
		new THREE.TextureLoader().load('/poiw.png', (map) => {
			map.colorSpace = THREE.SRGBColorSpace;
			const logo = flat(map, 0.62, 0.65);
			// The artwork is pale; multiplying by a deep violet makes it read on the white bubble.
			(logo.material as InstanceType<typeof THREE.MeshBasicMaterial>).color.set(0x3b1fa3);
			logoContent.add(logo);
			stage.wake();
		});
		const bangBubble = bubble(1.25, wording('!', 150));
		chatAt.add(
			speaker.group,
			listener.group,
			dotBubbles[0].group,
			dotBubbles[1].group,
			logoBubble.group,
			bangBubble.group
		);
		room.add(chatAt);

		// Scene 3: soldering at a workbench, with tools on a pegboard behind.
		const benchAt = new THREE.Group();
		benchAt.position.x = SPACING * 3;
		const maker = person('--color-yellow-500', 0xd8a274, 0x5a3a1c);
		maker.group.position.z = -0.85;
		maker.head.rotation.x = 0.3;
		maker.left.rotation.set(-1.4, 0, 0.2);
		maker.right.rotation.set(-1.5, 0, -0.1);

		const benchTop = box(3.2, 0.12, 1.25, wood);
		benchTop.position.set(0, 0.95, -0.25);
		benchAt.add(maker.group, benchTop);
		for (const [x, z] of [
			[-1.45, -0.75],
			[1.45, -0.75],
			[-1.45, 0.25],
			[1.45, 0.25]
		]) {
			const leg = box(0.1, 0.93, 0.1, wood);
			leg.position.set(x, 0.465, z);
			benchAt.add(leg);
		}

		const board = box(0.95, 0.04, 0.62, solid('--color-green-700'));
		board.position.set(0, 1.03, 0.05);
		const chipMaterial = solid(0x1c1a24, 0.4);
		for (const [x, z] of [
			[-0.28, -0.12],
			[-0.28, 0.2],
			[0.3, 0.18]
		]) {
			const chip = box(0.2, 0.05, 0.14, chipMaterial);
			chip.position.set(x, 1.07, z);
			benchAt.add(chip);
		}
		const ledMaterial = new THREE.MeshBasicMaterial({ color: 0x4a3b16 });
		const led = new THREE.Mesh(new THREE.SphereGeometry(0.045, 16, 12), ledMaterial);
		led.position.set(0.32, 1.09, -0.12);
		benchAt.add(board, led);

		const up = new THREE.Vector3(0, 1, 0);
		const rod = (
			from: InstanceType<typeof THREE.Vector3>,
			to: InstanceType<typeof THREE.Vector3>,
			radius: number,
			material: InstanceType<typeof THREE.Material>
		) => {
			const along = to.clone().sub(from);
			const mesh = new THREE.Mesh(
				new THREE.CylinderGeometry(radius, radius, along.length(), 12),
				material
			);
			mesh.position.copy(from).addScaledVector(along, 0.5);
			mesh.quaternion.setFromUnitVectors(up, along.normalize());
			return mesh;
		};
		const solderPoint = new THREE.Vector3(0.05, 1.06, 0.05);
		benchAt.add(
			rod(
				new THREE.Vector3(0.36, 1.26, -0.2),
				new THREE.Vector3(0.18, 1.14, -0.05),
				0.05,
				solid('--color-brand-text')
			),
			rod(new THREE.Vector3(0.18, 1.14, -0.05), solderPoint, 0.015, solid(0xc8ccd4, 0.3))
		);
		const sparkMaterial = new THREE.MeshBasicMaterial({ color: 0xffc94d });
		const sparks = [-0.7, -0.4, -0.15, 0.2, 0.5, 0.8].map((drift) => {
			const spark = new THREE.Mesh(new THREE.SphereGeometry(0.022, 8, 8), sparkMaterial);
			benchAt.add(spark);
			return { spark, drift };
		});

		const pegboard = box(3, 1.15, 0.05, solid('--color-gray-300'));
		pegboard.position.set(0, 2.55, -1.9);
		benchAt.add(pegboard);
		[
			[-1.05, 0.08, 0.72, '--color-brand-text'],
			[-0.65, 0.2, 0.5, '--color-yellow-500'],
			[-0.25, 0.06, 0.82, '--color-gray-400'],
			[0.2, 0.26, 0.42, '--color-green-700'],
			[0.68, 0.08, 0.7, '--color-yellow-500'],
			[1.08, 0.18, 0.56, '--color-brand-text']
		].forEach(([x, w, h, colour]) => {
			const tool = box(w as number, h as number, 0.05, solid(colour as string));
			tool.position.set(x as number, 2.82 - (h as number) / 2, -1.85);
			benchAt.add(tool);
		});
		room.add(benchAt);

		// Scene 4: the account. An email and a password go in, and the padlock clicks shut.
		const accountAt = new THREE.Group();
		accountAt.position.x = SPACING * 4;
		const signer = person('--color-green-700', 0xe8b98f, 0x2a1d14);
		signer.group.position.x = -1.2;
		signer.group.rotation.y = 0.4;

		const padlock = new THREE.Group();
		padlock.position.set(1.15, 0, 0);
		const unlockedColour = tokenColour('--color-yellow-500');
		const lockedColour = tokenColour('--color-green-700');
		const lockMaterial = solid('--color-yellow-500', 0.4);
		const lockBody = box(1.15, 1, 0.42, lockMaterial);
		lockBody.position.y = 1;
		const keyholeMaterial = solid(0x1c1a24, 0.4);
		const keyhole = new THREE.Mesh(new THREE.CircleGeometry(0.11, 24), keyholeMaterial);
		keyhole.position.set(0, 1.08, 0.215);
		const keySlot = new THREE.Mesh(new THREE.PlaneGeometry(0.08, 0.26), keyholeMaterial);
		keySlot.position.set(0, 0.92, 0.215);

		// Hinged at its left leg, so opening lifts it and swings the free leg out.
		const steel = solid(0xc8ccd4, 0.3);
		const shackle = new THREE.Group();
		shackle.position.set(-0.32, 1.45, 0);
		const arch = new THREE.Mesh(new THREE.TorusGeometry(0.32, 0.07, 12, 32, Math.PI), steel);
		arch.position.set(0.32, 0.3, 0);
		for (const x of [0, 0.64]) {
			const shackleLeg = new THREE.Mesh(new THREE.CylinderGeometry(0.07, 0.07, 0.34, 16), steel);
			shackleLeg.position.set(x, 0.13, 0);
			shackle.add(shackleLeg);
		}
		shackle.add(arch);
		padlock.add(lockBody, keyhole, keySlot, shackle);

		const emailBubble = bubble(-1.45, wording('@', 120));
		const passwordBubble = bubble(-1.45, wording('***', 120));
		const checkBubble = bubble(1.15, wording('✓', 130));
		accountAt.add(
			signer.group,
			padlock,
			emailBubble.group,
			passwordBubble.group,
			checkBubble.group
		);
		room.add(accountAt);

		const pan = spring(firstScene, 1.1);
		const lift = spring(-0.25, 0.6);
		const shake = spring(0, 0.24, 0.28);
		const raise = spring(0, 0.45, 0.7);
		const speakerHop = spring(0, 0.3, 0.45);
		const listenerHop = spring(0, 0.3, 0.45);
		const lockOpen = spring(startAtEnd ? 0 : 1, 0.3, 0.55);
		const lockBeat = spring(1, 0.22, 0.35);
		const bubbles = [
			...dotBubbles,
			logoBubble,
			bangBubble,
			emailBubble,
			passwordBubble,
			checkBubble
		];
		const springs = [
			pan,
			lift,
			shake,
			raise,
			speakerHop,
			listenerHop,
			lockOpen,
			lockBeat,
			...bubbles.map((b) => b.pop)
		];
		lift.target = 0;

		const NEVER = Number.NEGATIVE_INFINITY;
		let clock = 0;
		let current = -1;
		let knocked = 0;
		// When the current scene's own timeline began.
		let sceneAt = startAtEnd ? NEVER : 0;
		let hopped = 0;
		const view = new THREE.Vector3(...VIEW_FROM).normalize();

		stage.play((dt) => {
			// A frame right after waking can report a slightly negative time.
			const step = Math.max(dt, 0);
			clock += step;
			// Reduced motion draws each scene's finished picture.
			const elapsed = reduced || sceneAt === NEVER ? Number.POSITIVE_INFINITY : clock - sceneAt;
			const between = (from: number, to: number) =>
				current === 2 && elapsed >= from && elapsed < to;

			raise.target = current === 0 ? 1 : 0;
			dotBubbles[0].pop.target =
				between(FIRST_DOTS, REPLY_DOTS) || between(SECOND_DOTS, LOGO) ? 1 : 0;
			dotBubbles[1].pop.target = between(REPLY_DOTS, SECOND_DOTS) ? 1 : 0;
			logoBubble.pop.target = current === 2 && elapsed >= LOGO ? 1 : 0;
			bangBubble.pop.target = current === 2 && elapsed >= SURPRISE ? 1 : 0;

			const inAccount = (from: number, to: number) =>
				current === 4 && elapsed >= from && elapsed < to;
			emailBubble.pop.target = inAccount(EMAIL, PASSWORD) ? 1 : 0;
			passwordBubble.pop.target = inAccount(PASSWORD, LOCK) ? 1 : 0;
			checkBubble.pop.target = current === 4 && elapsed >= CHECK ? 1 : 0;
			lockOpen.target = current === 4 && elapsed >= LOCK ? 0 : 1;
			if (current === 4 && Number.isFinite(elapsed) && hopped < 1 && elapsed >= LOCK) {
				hopped = 1;
				lockBeat.velocity = 2.5;
			}

			if (current === 2 && Number.isFinite(elapsed)) {
				if (hopped < 1 && elapsed >= LOGO) {
					hopped = 1;
					speakerHop.velocity = 2;
				}
				if (hopped < 2 && elapsed >= SURPRISE) {
					hopped = 2;
					listenerHop.velocity = 6;
				}
			}

			if (reduced) snapSprings(springs);
			else stepSprings(springs, step);

			const d = distance(camera.aspect);
			const x = pan.value * SPACING;
			camera.position.set(x + view.x * d, LOOK_Y + view.y * d, view.z * d);
			camera.lookAt(x, LOOK_Y, 0);

			room.position.x = shake.value;
			room.position.y = lift.value;

			for (const b of bubbles) {
				b.group.visible = b.pop.value > 0.01;
				b.group.scale.setScalar(Math.max(b.pop.value, 0.001));
			}

			const animated = reduced ? 0 : clock;
			const moving = reduced ? 0 : 1;
			greeter.right.rotation.z = raise.value * (2.55 + moving * Math.sin(animated * 8) * 0.32);
			greeter.head.rotation.z = moving * Math.sin(animated * 2.2) * 0.06;
			greeter.group.position.y = moving * Math.abs(Math.sin(animated * 4)) * 0.03;

			// One page turns over every few seconds.
			const cycle = animated % 3.2;
			turningPage.rotation.z = cycle < 0.7 ? (cycle / 0.7) * Math.PI : 0;
			leaf.visible = cycle < 0.7;
			reader.head.rotation.y = moving * Math.sin(animated * 0.9) * 0.1;

			for (const { dots } of [speakerDots, listenerDots])
				dots.forEach((dot, i) => {
					dot.position.y = moving * Math.max(Math.sin(animated * 7 - i * 0.9), 0) * 0.07;
				});

			const speaking = between(FIRST_DOTS, REPLY_DOTS) || between(SECOND_DOTS, LOGO);
			const replying = between(REPLY_DOTS, SECOND_DOTS);
			speaker.right.rotation.x = speaking ? -0.7 + Math.sin(animated * 9) * 0.25 : 0;
			listener.left.rotation.x = replying ? -0.7 + Math.sin(animated * 9) * 0.25 : 0;
			const surprised = current === 2 && elapsed >= SURPRISE;
			listener.left.rotation.z = surprised ? -0.5 : 0;
			listener.right.rotation.z = surprised ? 0.5 : 0;
			speaker.group.position.y = Math.max(speakerHop.value, 0);
			listener.group.position.y = Math.max(listenerHop.value, 0);

			// Soldering in bursts: sparks fly while the iron touches, then the LED lights up.
			const soldering = moving === 1 && animated % 2.4 < 1.4;
			ledMaterial.color.set(soldering ? 0x4a3b16 : 0xffc94d);
			sparks.forEach(({ spark, drift }, i) => {
				const life = (animated * 1.5 + i / sparks.length) % 1;
				spark.visible = soldering;
				spark.position.set(
					solderPoint.x + drift * life * 0.55,
					solderPoint.y + life * 0.5 - life * life * 0.5,
					solderPoint.z + 0.05
				);
				spark.scale.setScalar(1 - life * 0.7);
			});
			maker.head.rotation.y = moving * Math.sin(animated * 0.8) * 0.08;

			const open = THREE.MathUtils.clamp(lockOpen.value, 0, 1.2);
			shackle.position.y = 1.45 + open * 0.3;
			shackle.rotation.y = open * -1.1;
			lockMaterial.color.copy(unlockedColour).lerp(lockedColour, 1 - Math.min(open, 1));
			padlock.scale.setScalar(lockBeat.value);
			const typingNow = moving === 1 && inAccount(EMAIL, LOCK);
			signer.right.rotation.x = typingNow ? -1 + Math.sin(animated * 14) * 0.12 : 0;
			signer.left.rotation.x = typingNow ? -1 - Math.sin(animated * 14) * 0.12 : 0;

			// Looping scenes keep going; the two with an ending rest once it has played out.
			return (
				current === 0 || current === 1 || current === 3 || elapsed < STORY_ENDS || !settled(springs)
			);
		});

		return {
			set(nextScene: number, nextKnock: number) {
				if (nextScene !== current) {
					const panned = current !== -1;
					current = nextScene;
					pan.target = nextScene;
					if (panned || !startAtEnd) {
						hopped = 0;
						sceneAt = clock + (panned ? PAN_S : 0);
					}
				}
				if (nextKnock !== knocked && !reduced) shake.velocity = 4;
				knocked = nextKnock;
				stage.wake();
			},
			dispose: stage.dispose
		};
	}

	onMount(() => {
		let gone = false;
		start(host, scene, ended)
			.then((started) => {
				if (!started) unsupported = true;
				else if (gone) started.dispose();
				else {
					api = started;
					started.set(scene, knock);
					loaded = true;
				}
			})
			// The scene only illustrates the questions; the forms work without it.
			.catch(() => (unsupported = true));
		return () => {
			gone = true;
			api?.dispose();
		};
	});

	$effect(() => {
		const [nextScene, nextKnock] = [scene, knock];
		api?.set(nextScene, nextKnock);
	});
</script>

<div
	bind:this={host}
	class="h-40 w-full overflow-hidden rounded-md border bg-paper-2 transition-opacity duration-300 sm:h-52
		{loaded ? 'opacity-100' : 'opacity-0'}"
	class:hidden={unsupported}
	data-scene={scene}
	aria-hidden="true"
></div>
