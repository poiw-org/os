import { defineConfig, devices } from '@playwright/test';

// PW_CHROMIUM_PATH lets a machine without the exact bundled browser revision point at another Chromium.
const executablePath = process.env.PW_CHROMIUM_PATH;

export default defineConfig({
	testDir: 'e2e',
	snapshotPathTemplate: '{testDir}/baseline/{arg}-{projectName}{ext}',
	fullyParallel: true,
	reporter: 'list',
	expect: { toHaveScreenshot: { maxDiffPixelRatio: 0.002 } },
	use: {
		baseURL: 'http://localhost:4173',
		timezoneId: 'Europe/Athens', // dates render in local time; pin it so baselines are portable
		...devices['Desktop Chrome'],
		reducedMotion: 'reduce', // screenshots must not catch rows mid-slide; also exercises that path
		launchOptions: executablePath ? { executablePath } : {}
	},
	projects: [
		{ name: 'desktop', use: { viewport: { width: 1280, height: 900 } } },
		{ name: 'mobile', use: { viewport: { width: 375, height: 812 } } }
	],
	webServer: {
		command: 'pnpm build && pnpm preview --port 4173 --strictPort',
		url: 'http://localhost:4173',
		reuseExistingServer: true,
		timeout: 180_000
	}
});
