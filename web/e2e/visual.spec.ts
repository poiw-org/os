import { test, expect, KEY_IDS, USERS } from './fixtures';

// Design-language regression baseline. Runs for the desktop and mobile projects.
// Update intentionally with: pnpm test:visual --update-snapshots

const screens: [name: string, path: string, ready: string][] = [
	['home', '/os', 'Door entries'],
	['user', `/os/users?id=${USERS[0].id}`, 'Recent entries'],
	['key-detail', `/os/items?id=${KEY_IDS.first}`, 'Log'],
	['create', '/os/items/create', 'Activate not before'],
	['renew', `/os/items/renew?id=${KEY_IDS.first}`, 'Activate not before']
];

for (const [name, path, ready] of screens) {
	test(`visual: ${name}`, async ({ page }) => {
		await page.goto(path);
		await expect(page.getByText(ready, { exact: true }).first()).toBeVisible();
		await page.evaluate(() => document.fonts.ready);
		await expect(page).toHaveScreenshot(`${name}.png`, { fullPage: true });
	});
}

test.describe('signed out', () => {
	test.use({ signedIn: false });

	test('visual: login', async ({ page }) => {
		await page.goto('/login');
		await expect(page.getByText('SIGN IN').first()).toBeVisible();
		// The lanyard panel only exists from the lg breakpoint up.
		if ((page.viewportSize()?.width ?? 0) >= 1024)
			await expect(page.locator('[data-ready="true"]')).toBeVisible();
		await page.evaluate(() => document.fonts.ready);
		await expect(page).toHaveScreenshot('login.png', { fullPage: true });
	});
});
