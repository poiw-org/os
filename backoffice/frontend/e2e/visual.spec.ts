import { test, expect, KEY_IDS, USERS } from './fixtures';

// Design-language regression baseline. Runs for the desktop and mobile projects.
// Update intentionally with: pnpm test:visual --update-snapshots

const screens: [name: string, path: string, ready: string][] = [
	['home', '/', 'Live entries'],
	['user', `/users?id=${USERS[0].id}`, 'RECENT ENTRIES'],
	['key-detail', `/items?id=${KEY_IDS.first}`, 'Logs:'],
	['create', '/items/create', 'Activate not before'],
	['renew', `/items/renew?id=${KEY_IDS.first}`, 'Activate not before']
];

for (const [name, path, ready] of screens) {
	test(`visual: ${name}`, async ({ page }) => {
		await page.goto(path);
		await expect(page.getByText(ready).first()).toBeVisible();
		await page.evaluate(() => document.fonts.ready);
		await expect(page).toHaveScreenshot(`${name}.png`, { fullPage: true });
	});
}

test.describe('signed out', () => {
	test.use({ signedIn: false });

	test('visual: login', async ({ page }) => {
		await page.goto('/login');
		await expect(page.getByText('SIGN IN').first()).toBeVisible();
		await page.evaluate(() => document.fonts.ready);
		await expect(page).toHaveScreenshot('login.png', { fullPage: true });
	});
});
