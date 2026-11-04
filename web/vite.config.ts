import tailwindcss from '@tailwindcss/vite';
import adapter from '@sveltejs/adapter-static';
import { sveltekit } from '@sveltejs/kit/vite';
import { defineConfig } from 'vite';

export default defineConfig({
	server: { port: 3000 },
	plugins: [
		tailwindcss(),
		sveltekit({
			compilerOptions: {
				// Force runes mode for the project, except for libraries. Can be removed in svelte 6.
				runes: ({ filename }) =>
					filename.split(/[/\\]/).includes('node_modules') ? undefined : true
			},
			// Absolute asset paths, so one app shell works at any URL depth (/os/items/:id).
			paths: { relative: false },
			// The :id pages are not prerendered; the fallback shell serves them.
			adapter: adapter({ fallback: '200.html' })
		})
	]
});
