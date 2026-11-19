<script lang="ts">
	import { onMount } from 'svelte';
	import { t } from '#lib/i18n.svelte.ts';

	// Shown to anyone with no active key: a warm nudge to the Discord, where the team hands them out.
	// The picture is a small three.js scene in the style of the onboarding one: a few people wave
	// beside a door that is slightly open, a key floats toward it, and hearts drift up.
	let host: HTMLDivElement;
	let loaded = $state(false);
	let unsupported = $state(false);

	async function start(host: HTMLDivElement) {
		const [THREE, { RoundedBoxGeometry }, { canvasTexture, createStage, solid, person }] =
			await Promise.all([
				import('three'),
				import('three/examples/jsm/geometries/RoundedBoxGeometry.js'),
				import('#lib/three.ts')
			]);

		const stage = createStage(host, {
			target: [0, 1.5, 0],
			from: [0.1, 0.2, 1],
			distance: (aspect) => Math.max(7.6, 15.5 / aspect)
		});
		if (!stage) return undefined;
		const { world, reduced } = stage;

		const box = (w: number, h: number, d: number, material: InstanceType<typeof THREE.Material>) =>
			new THREE.Mesh(new RoundedBoxGeometry(w, h, d, 2, Math.min(w, h, d) / 4), material);

		const room = new THREE.Group();
		world.add(room);
		const wall = new THREE.Mesh(new THREE.BoxGeometry(40, 6, 0.1), solid('--color-paper', 0.9));
		wall.position.set(0, 2, -2);
		const floor = new THREE.Mesh(new THREE.BoxGeometry(40, 0.06, 5), solid('--color-paper-2', 0.9));
		floor.position.set(0, -0.03, 0.4);
		room.add(wall, floor);

		// The door: a dark frame, warm light behind it, and a panel hinged on its left edge.
		const DOOR_X = 2.7;
		const frame = box(1.8, 3.3, 0.12, solid('--color-gray-400', 0.5));
		frame.position.set(DOOR_X, 1.65, -1.9);
		const glow = new THREE.Mesh(
			new THREE.PlaneGeometry(1.45, 3.05),
			new THREE.MeshBasicMaterial({ color: 0xffc94d })
		);
		glow.position.set(DOOR_X, 1.62, -1.82);
		const hinge = new THREE.Group();
		hinge.position.set(DOOR_X - 0.72, 0, -1.78);
		const panel = box(1.44, 3.05, 0.08, solid('--color-brand-text', 0.5));
		panel.position.set(0.72, 1.62, 0);
		const knob = new THREE.Mesh(new THREE.SphereGeometry(0.07, 16, 12), solid(0xc8ccd4, 0.3));
		knob.position.set(1.25, 1.5, 0.08);
		hinge.add(panel, knob);
		room.add(frame, glow, hinge);

		// The welcoming committee.
		const crowd = [
			{ p: person('--color-yellow-500', 0xd8a274, 0x5a3a1c), x: -3.0, turn: 0.35, phase: 0 },
			{ p: person('--color-green-700', 0xf2d0ae, 0x7a4a24), x: -1.8, turn: 0.15, phase: 1.3 },
			{ p: person('--color-brand-text', 0xc58c5e, 0x24201e), x: -0.6, turn: -0.1, phase: 2.4 }
		];
		for (const { p, x, turn } of crowd) {
			p.group.position.x = x;
			p.group.rotation.y = turn;
			room.add(p.group);
		}

		// The key, tilted toward the door.
		const gold = solid('--color-yellow-500', 0.35);
		const keyGroup = new THREE.Group();
		const bow = new THREE.Mesh(new THREE.TorusGeometry(0.3, 0.09, 14, 32), gold);
		const shaft = new THREE.Mesh(new THREE.CylinderGeometry(0.07, 0.07, 1.1, 14), gold);
		shaft.rotation.z = Math.PI / 2;
		shaft.position.x = 0.8;
		keyGroup.add(bow, shaft);
		for (const x of [1.0, 1.25]) {
			const tooth = box(0.12, 0.3, 0.1, gold);
			tooth.position.set(x, -0.17, 0);
			keyGroup.add(tooth);
		}
		keyGroup.scale.setScalar(0.85);
		room.add(keyGroup);

		// Hearts, drawn white-on-nothing and tinted per material so they can fade on their own.
		const heartShape = canvasTexture(128, 128, (c) => {
			c.fillStyle = '#fff';
			c.beginPath();
			c.moveTo(64, 112);
			c.bezierCurveTo(8, 72, 14, 18, 64, 40);
			c.bezierCurveTo(114, 18, 120, 72, 64, 112);
			c.fill();
		});
		const hearts = crowd.map(({ x }, i) => {
			const material = new THREE.MeshBasicMaterial({
				map: heartShape,
				color: 0xff6b8a,
				transparent: true,
				depthWrite: false
			});
			const heart = new THREE.Mesh(new THREE.PlaneGeometry(0.42, 0.42), material);
			room.add(heart);
			return { heart, material, x, offset: i / crowd.length };
		});

		let clock = 0;
		stage.play((dt) => {
			clock += Math.max(dt, 0);
			// Reduced motion draws one still, friendly frame.
			const time = reduced ? 1.2 : clock;
			const moving = reduced ? 0 : 1;

			for (const { p, phase } of crowd) {
				p.right.rotation.z = 2.55 + Math.sin(time * 6 + phase) * 0.32 * moving;
				p.head.rotation.z = Math.sin(time * 2 + phase) * 0.06 * moving;
				p.group.position.y = Math.abs(Math.sin(time * 3.2 + phase)) * 0.04 * moving;
			}

			keyGroup.position.set(0.55, 2.05 + Math.sin(time * 1.6) * 0.12 * moving, 0.2);
			keyGroup.rotation.set(0, Math.sin(time * 1.1) * 0.7 * moving, -0.35);

			hinge.rotation.y = -(0.45 + Math.sin(time * 0.9) * 0.12 * moving);

			for (const { heart, material, x, offset } of hearts) {
				const life = reduced ? 0.5 + offset * 0.2 : (time * 0.45 + offset) % 1;
				heart.position.set(
					x + 0.35 + Math.sin(life * 6 + offset * 9) * 0.08,
					2.45 + life * 0.8,
					0.5
				);
				heart.scale.setScalar(0.55 + Math.sin(life * Math.PI) * 0.5);
				material.opacity = Math.sin(life * Math.PI);
			}
			return true;
		});

		return { dispose: stage.dispose };
	}

	onMount(() => {
		let gone = false;
		let api: { dispose: () => void } | undefined;
		start(host)
			.then((started) => {
				if (!started) unsupported = true;
				else if (gone) started.dispose();
				else {
					api = started;
					loaded = true;
				}
			})
			// The picture is decoration; the invitation reads fine without it.
			.catch(() => (unsupported = true));
		return () => {
			gone = true;
			api?.dispose();
		};
	});
</script>

<section
	class="flex flex-col overflow-hidden rounded-lg border bg-paper-2 sm:flex-row sm:items-stretch"
	aria-labelledby="key-invite-title"
>
	<div
		bind:this={host}
		class="h-44 w-full shrink-0 transition-opacity duration-500 sm:h-auto sm:min-h-64 sm:w-[46%]
			{loaded ? 'opacity-100' : 'opacity-0'}"
		class:hidden={unsupported}
		aria-hidden="true"
	></div>
	<div class="flex flex-1 flex-col justify-center gap-3 p-5 sm:p-8">
		<p class="eyebrow">{t('Welcome to po/iw')}</p>
		<h2 id="key-invite-title" class="text-xl font-bold md:text-2xl">
			{t('Come say hi, and get your key')}
		</h2>
		<p class="max-w-prose leading-relaxed text-gray-400">
			{t(
				"You don't have an active key yet. Hop into our Discord, meet the people behind the lab and ask for one."
			)}
		</p>
		<div class="flex flex-wrap items-center gap-x-5 gap-y-2 pt-1">
			<a
				href="https://discord.poiw.org"
				target="_blank"
				rel="noopener"
				class="btn btn-primary px-5 py-2.5">{t('Join discord.poiw.org')} ♥</a
			>
			<span class="text-xs text-gray-400"
				>{t('Your key shows up here as soon as someone issues it.')}</span
			>
		</div>
	</div>
</section>
