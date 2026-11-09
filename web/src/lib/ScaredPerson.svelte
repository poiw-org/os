<script lang="ts">
	import { onMount } from 'svelte';

	// Someone shaking with their hands on their face in front of a black box nobody can see inside.
	// Drawn in the onboarding scenes' style. Reduced motion gets the same picture, held still.
	let host: HTMLDivElement;
	let loaded = $state(false);
	let unsupported = $state(false);

	async function start(host: HTMLDivElement) {
		const [
			THREE,
			{ RoundedBoxGeometry },
			{ canvasTexture, createStage, solid, person, spring, stepSprings, snapSprings }
		] = await Promise.all([
			import('three'),
			import('three/examples/jsm/geometries/RoundedBoxGeometry.js'),
			import('#lib/three.ts')
		]);

		const stage = createStage(host, {
			target: [0, 1.3, 0],
			from: [0.1, 0.2, 1],
			distance: (aspect) => Math.max(6.7, 9 / aspect)
		});
		if (!stage) return undefined;
		const { world, reduced } = stage;

		const flat = (text: string, colour: string, w: number) =>
			new THREE.Mesh(
				new THREE.PlaneGeometry(w, w),
				new THREE.MeshBasicMaterial({
					map: canvasTexture(200, 200, (c) => {
						c.fillStyle = colour;
						c.font = '800 170px system-ui, sans-serif';
						c.textAlign = 'center';
						c.textBaseline = 'middle';
						c.fillText(text, 100, 108);
					}),
					transparent: true,
					depthWrite: false
				})
			);

		const scared = person('--color-yellow-500', 0xe8b98f, 0x3a2a20);
		scared.group.position.x = -0.8;
		scared.group.rotation.y = 0.4;
		scared.left.rotation.set(-2.65, 0, 0.28);
		scared.right.rotation.set(-2.65, 0, -0.28);
		// The smile becomes a gasp, and the eyes go wide.
		scared.smile.visible = false;
		const gasp = new THREE.Mesh(new THREE.SphereGeometry(0.075, 16, 12), solid(0x1c1a24, 0.3));
		gasp.scale.set(0.8, 1.2, 0.3);
		gasp.position.set(0, -0.13, 0.3);
		scared.head.add(gasp);
		const white = solid(0xffffff, 0.4);
		for (const eye of scared.eyes) {
			eye.position.z = 0.345;
			const sclera = new THREE.Mesh(new THREE.SphereGeometry(0.065, 16, 12), white);
			sclera.position.set(eye.position.x, eye.position.y, 0.295);
			scared.head.add(sclera);
		}

		const sweatMaterial = new THREE.MeshBasicMaterial({ color: 0x7cc4ff });
		const sweat = [-0.42, 0.42, 0.3].map((x, i) => {
			const drop = new THREE.Mesh(new THREE.SphereGeometry(0.04, 10, 10), sweatMaterial);
			scared.group.add(drop);
			return { drop, x, phase: i / 3 };
		});

		const alarm = flat('!', '#e5484d', 0.9);
		alarm.position.set(-0.8, 2.62, 0.3);

		const binary = new THREE.Group();
		binary.position.set(1.1, 0, 0);
		const stand = new THREE.Mesh(
			new RoundedBoxGeometry(0.7, 0.75, 0.7, 2, 0.08),
			solid('--color-gray-400')
		);
		stand.position.y = 0.375;
		const blackBox = new THREE.Group();
		blackBox.position.y = 1.3;
		blackBox.rotation.y = -0.35;
		const shell = new THREE.Mesh(new RoundedBoxGeometry(1, 1, 1, 3, 0.12), solid(0x1c1a24, 0.35));
		const mark = flat('?', '#e5484d', 0.85);
		mark.position.z = 0.51;
		blackBox.add(shell, mark);
		binary.add(stand, blackBox);

		// Red washes in and out, slowly enough to stay well under any flashing threshold.
		const siren = new THREE.DirectionalLight(0xff2a2a, 1.4);
		siren.position.set(0, 3, 5);
		world.add(scared.group, alarm, binary, siren);

		const pop = spring(0, 0.3, 0.45);
		pop.target = 1;
		let clock = 0;

		stage.play((dt) => {
			clock += Math.max(dt, 0);
			if (reduced) snapSprings([pop]);
			else stepSprings([pop], Math.max(dt, 0));
			const moving = reduced ? 0 : 1;

			alarm.scale.setScalar(Math.max(pop.value, 0.001) * (1 + moving * Math.sin(clock * 6) * 0.08));
			scared.group.position.x = -0.8 + moving * Math.sin(clock * 47) * 0.014;
			scared.head.rotation.z = moving * Math.sin(clock * 31) * 0.035;
			scared.head.rotation.x = 0.05;
			scared.left.rotation.x = -2.65 + moving * Math.sin(clock * 41) * 0.03;
			scared.right.rotation.x = -2.65 + moving * Math.sin(clock * 37 + 1) * 0.03;
			blackBox.position.y = 1.3 + moving * Math.sin(clock * 1.6) * 0.05;
			siren.intensity = moving ? 0.8 + Math.sin(clock * 4) * 0.8 : 0.8;

			for (const { drop, x, phase } of sweat) {
				const life = moving ? (clock * 1.1 + phase) % 1 : phase + 0.2;
				drop.position.set(x + Math.sign(x) * life * 0.2, 2 - life * 0.75, 0.25);
				drop.scale.setScalar(1 - life * 0.6);
			}
			return true;
		});
		return stage;
	}

	onMount(() => {
		let gone = false;
		let dispose: (() => void) | undefined;
		start(host)
			.then((stage) => {
				if (!stage) unsupported = true;
				else if (gone) stage.dispose();
				else {
					dispose = stage.dispose;
					loaded = true;
				}
			})
			// The picture only illustrates the warning; the words work without it.
			.catch(() => (unsupported = true));
		return () => {
			gone = true;
			dispose?.();
		};
	});
</script>

<div
	bind:this={host}
	class="h-40 w-full overflow-hidden rounded-md border bg-paper-2 transition-opacity duration-300 sm:h-52
		{loaded ? 'opacity-100' : 'opacity-0'}"
	class:hidden={unsupported}
	data-scene="scared"
	aria-hidden="true"
></div>
