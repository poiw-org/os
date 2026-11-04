import { test, expect, KEY_IDS, KEYS, PASSWORD, USERS } from './fixtures';

const firstId = KEY_IDS.first;
const secondId = KEY_IDS.second;
const short = (id: string) => id.slice(0, 8);

test.describe('signed in', () => {
	test('avatar opens settings and saves the display name', async ({ page, api }) => {
		await page.goto('/os');
		await page.getByRole('button', { name: 'Settings' }).locator('visible=true').click();
		const dialog = page.getByRole('dialog');
		await expect(dialog.getByRole('tab', { name: 'Profile' })).toBeVisible();
		await dialog.getByLabel('Display name').fill('boss');
		await dialog.getByRole('button', { name: 'Save' }).click();
		await expect
			.poll(() => api.bodies.find((b) => b.call === 'PATCH /rest/v1/profiles')?.body)
			.toEqual({ nickname: 'boss' });
	});

	test('users -> keys -> entries drill-down', async ({ page }) => {
		await page.goto('/os');
		await expect(page.getByRole('heading', { name: 'Users' })).toBeVisible();
		await expect(
			page.locator('#results').getByRole('link', { name: /alice@example\.test/ })
		).toBeVisible();
		// Carol has no keys, so she is hidden until asked for.
		await expect(page.getByText('carol@example.test')).toBeHidden();
		await page.getByRole('button', { name: /Show 1 users without keys/ }).click();
		await expect(
			page.locator('#results').getByRole('link', { name: /carol@example\.test/ })
		).toBeVisible();

		await page
			.locator('#results')
			.getByRole('link', { name: /bob@example\.test/ })
			.click();
		await expect(page).toHaveURL(new RegExp(`/os/users\\?id=${USERS[1].id}`));
		await expect(page.getByText('1 key', { exact: true })).toBeVisible();
		await page.getByRole('link', { name: new RegExp(short(secondId)) }).click();
		await expect(page).toHaveURL(new RegExp(`/os/items\\?id=${secondId}`));
		await expect(page.getByText('ENABLED')).toBeVisible();
		await page
			.getByRole('navigation', { name: 'Breadcrumb' })
			.getByRole('link', { name: 'bob@example.test' })
			.click();
		await expect(page).toHaveURL(/\/os\/users\?id=/);
	});

	test('home search finds users by tag serial number', async ({ page }) => {
		await page.goto('/os');
		await page.getByPlaceholder('Search users').fill('04F6E5');
		await expect(
			page.locator('#results').getByRole('link', { name: /bob@example\.test/ })
		).toBeVisible();
		await expect(
			page.locator('#results').getByRole('link', { name: /alice@example\.test/ })
		).toBeHidden();
	});

	test('user page lists keys and recent entries and pre-selects the user for a new key', async ({
		page
	}) => {
		await page.goto(`/os/users?id=${USERS[0].id}`);
		await expect(page.getByText('Recent entries')).toBeVisible();
		await expect(page.getByRole('link', { name: new RegExp(short(firstId)) })).toBeVisible();
		await page.getByRole('main').getByRole('link', { name: 'Issue key' }).click();
		await expect(page.getByText('alice@example.test').first()).toBeVisible();
	});

	test('changes stream in without a reload', async ({ page, api }) => {
		await page.goto(`/os/items?id=${firstId}`);
		await page.locator('div.cursor-pointer').first().waitFor();
		const before = await page.locator('div.cursor-pointer').count();

		// There is no visible connection state, so keep pushing until the channel has joined.
		// Pushes are idempotent: rows are upserted by id.
		await expect(async () => {
			api.push('entries', 'INSERT', {
				id: 99,
				key_id: firstId,
				type: 'failed',
				reason: 'KeyBlacklisted',
				created_at: '2026-03-01T11:59:00.000Z',
				metadata: {}
			});
			await expect(page.locator('div.cursor-pointer')).toHaveCount(before + 1, { timeout: 500 });
		}).toPass();

		// ...and so does another admin disabling the key.
		await expect(async () => {
			api.push('keys', 'UPDATE', { ...api.keys[0], disabled: true });
			await expect(page.getByText('DISABLED')).toBeVisible({ timeout: 500 });
		}).toPass();
	});

	test('key detail shows access log', async ({ page }) => {
		await page.goto(`/os/items?id=${firstId}`);
		await expect(page.getByRole('heading', { name: 'Log' })).toBeVisible();
		// Log rows are icon-only until expanded and newest first, so the first one is the failed entry.
		await page.locator('div.cursor-pointer').first().click();
		await expect(page.getByText('Reason: KeyExpired')).toBeVisible();
		await expect(page.getByText('Serial Number from Tag: 04A1B2C3D4E5F6')).toBeVisible();
	});

	test('unknown key shows a message', async ({ page }) => {
		await page.goto('/os/items?id=00000000-0000-4000-8000-00000000dead');
		await expect(page.getByText('Key not found.')).toBeVisible();
	});

	test('disable then enable a key', async ({ page, api }) => {
		await page.goto(`/os/items?id=${firstId}`);
		await page.getByRole('button', { name: 'DISABLE' }).click();
		await page.getByRole('dialog').getByRole('button', { name: 'Disable key' }).click();
		await expect(page.getByText('DISABLED', { exact: true })).toBeVisible();
		await expect(page.getByRole('status')).toContainText('Key disabled.');
		expect(api.keys[0].disabled).toBe(true);
		await page.getByRole('button', { name: 'ENABLE' }).click();
		await page.getByRole('dialog').getByRole('button', { name: 'Enable key' }).click();
		await expect(page.getByText('ENABLED', { exact: true })).toBeVisible();
		expect(api.keys[0].disabled).toBe(false);
	});

	test('delete a key', async ({ page, api }) => {
		await page.goto(`/os/items?id=${secondId}`);
		await page.getByRole('button', { name: 'DELETE' }).click();
		await page.getByRole('dialog').getByRole('button', { name: 'Delete key' }).click();
		await expect(page).toHaveURL(/\/os$/);
		expect(api.calls).toContain('DELETE /rest/v1/keys');
		expect(api.keys.map((k) => k.id)).toEqual([firstId]);
	});

	test('issue a new key', async ({ page, api }) => {
		await page.goto('/os/items/create');
		await page.getByRole('combobox', { name: 'User' }).fill('bob');
		await page.keyboard.press('Enter');
		await page.getByPlaceholder('Tag S/N').fill('0499887766');
		await page.getByRole('button', { name: 'Issue key' }).click();
		await expect(page).toHaveURL(new RegExp(`/os/items\\?id=${KEY_IDS.created}`));
		const issue = api.bodies.find((b) => b.call === 'POST /functions/v1/keys')?.body;
		expect(issue).toMatchObject({
			action: 'issue',
			user_id: USERS[1].id,
			serial_number: '0499887766'
		});
	});

	test('issuing for a tag that is already bound shows the error', async ({ page }) => {
		await page.goto('/os/items/create');
		await page.getByRole('combobox', { name: 'User' }).fill('bob');
		await page.keyboard.press('Enter');
		await page.getByPlaceholder('Tag S/N').fill('DUPLICATE');
		await page.getByRole('button', { name: 'Issue key' }).click();
		await expect(page.getByRole('alert')).toContainText('This tag is already bound to a key');
		await expect(page).toHaveURL(/\/os\/items\/create/);
	});

	test('creating without a user is refused', async ({ page, api }) => {
		await page.goto('/os/items/create');
		await page.getByPlaceholder('Tag S/N').fill('0499887766');
		await page.getByRole('button', { name: 'Issue key' }).click();
		await expect(page.getByRole('alert')).toContainText('Select a user first.');
		expect(api.calls).not.toContain('POST /functions/v1/keys');
	});

	test('cancelling a confirmation changes nothing', async ({ page, api }) => {
		await page.goto(`/os/items?id=${secondId}`);
		await page.getByRole('button', { name: 'DELETE' }).click();
		const dialog = page.getByRole('dialog');
		await expect(dialog.getByText('Delete this key?')).toBeVisible();
		await page.keyboard.press('Escape');
		await expect(dialog).toBeHidden();
		expect(api.calls).not.toContain('DELETE /rest/v1/keys');
		await expect(page).toHaveURL(new RegExp(`/os/items\\?id=${secondId}`));
	});

	test('the user picker filters, supports the keyboard and can be changed', async ({ page }) => {
		await page.goto('/os/items/create');
		const picker = page.getByRole('combobox', { name: 'User' });
		await picker.click();
		await expect(page.getByRole('option')).toHaveCount(3);
		await picker.fill('car');
		await expect(page.getByRole('option')).toHaveCount(1);
		await picker.fill('zzz');
		await expect(page.getByText('No users match')).toBeVisible();
		await picker.fill('');
		await page.keyboard.press('ArrowDown');
		await page.keyboard.press('Enter');
		await expect(page.getByRole('button', { name: 'Change' })).toBeVisible();
		await page.getByRole('button', { name: 'Change' }).click();
		await expect(picker).toBeFocused();
	});

	test('validity presets set the expiry and the summary follows', async ({ page }) => {
		await page.goto('/os/items/create');
		await page.getByRole('button', { name: '6 months' }).click();
		await expect(page.getByRole('button', { name: '6 months' })).toHaveAttribute(
			'aria-pressed',
			'true'
		);
		await expect(page.getByText('184 days')).toBeVisible();
	});

	test('renew a key', async ({ page, api }) => {
		await page.goto(`/os/items/renew?id=${firstId}`);
		await expect(page.getByRole('heading', { name: `Renew ${short(firstId)}` })).toBeVisible();
		await expect(page.getByText('alice@example.test')).toBeVisible();
		await page.getByRole('button', { name: 'Renew' }).click();
		await expect(page).toHaveURL(new RegExp(`/os/items\\?id=${firstId}`));
		const renew = api.bodies.find((b) => b.call === 'POST /functions/v1/keys')?.body;
		expect(renew).toMatchObject({ action: 'renew', id: firstId });
	});

	test('force open is broadcast to the door, confirmed and logged as the admin', async ({
		page,
		api
	}) => {
		await page.goto('/os');
		await page.getByRole('button', { name: 'REMOTE UNLOCK' }).locator('visible=true').click();
		const dialog = page.getByRole('dialog');
		await dialog.getByRole('button', { name: 'Unlock', exact: true }).click();
		await expect(dialog.getByRole('status').filter({ hasText: 'Door unlocked' })).toBeVisible();
		await expect(dialog.getByRole('status')).toContainText('Recorded under your name');
		await expect(dialog.getByRole('progressbar')).toHaveAttribute('aria-valuenow', '100');
		const steps = dialog.getByRole('group');
		await steps.getByText('Steps').click();
		await expect(steps).toContainText('force_open_ack');
		await expect(steps).toContainText('Entry recorded under your name');
		expect(api.broadcasts).toHaveLength(1);
		expect(api.broadcasts[0]).toMatchObject({
			topic: 'realtime:door:front',
			event: 'force_open',
			payload: { id: expect.any(String), ts: expect.any(Number) }
		});
		const log = api.bodies.find((b) => b.call === 'POST /rest/v1/rpc/log_web_open')?.body;
		expect(log).toMatchObject({
			door: 'front',
			command_id: api.broadcasts[0].payload.id,
			latency_ms: expect.any(Number)
		});
		// Nothing is written to the doors table: a stored command could fire after an outage.
		expect(api.calls.filter((c) => c.startsWith('PATCH /rest/v1/doors'))).toEqual([]);
	});

	test('the door writes a key to a tag and the modal follows it', async ({ page, api }) => {
		await page.goto(`/os/items?id=${KEY_IDS.first}`);
		await page.getByRole('button', { name: 'Write at door' }).click();
		const dialog = page.getByRole('dialog');
		await dialog.getByRole('button', { name: 'Start writer mode' }).click();
		await expect(dialog.getByText('Hold the tag to the reader')).toBeVisible();
		await expect(dialog.getByText('Writing. Keep it still')).toBeVisible();
		await expect(dialog.getByText('Key written')).toBeVisible();
		const sent = api.broadcasts.find((b) => b.event === 'write_card');
		expect(sent?.payload).toMatchObject({
			id: expect.any(String),
			token: KEYS[0].jwt
		});
		await dialog.getByRole('button', { name: 'DONE' }).click();
		await expect(dialog).toBeHidden();
	});

	test('the door scans a tag and fills in its serial number', async ({ page, api }) => {
		await page.goto('/os/items/create');
		await page.getByRole('button', { name: 'Scan at door' }).click();
		const dialog = page.getByRole('dialog');
		await dialog.getByRole('button', { name: 'START SCANNING' }).click();
		await expect(dialog.getByText('Tag read', { exact: true })).toBeVisible();
		expect(api.broadcasts.some((b) => b.event === 'scan_card')).toBe(true);
		expect(api.broadcasts.some((b) => b.event === 'write_card')).toBe(false);
		await dialog.getByRole('button', { name: 'DONE' }).click();
		await expect(page.getByLabel('Tag S/N')).toHaveValue('04:aa:bb:cc:dd:ee:ff');
	});

	test('force open reports an offline door instead of pretending', async ({ page, api }) => {
		api.doorAnswers = false;
		await page.goto('/os');
		await page.getByRole('button', { name: 'REMOTE UNLOCK' }).locator('visible=true').click();
		const dialog = page.getByRole('dialog');
		await dialog.getByRole('button', { name: 'Unlock', exact: true }).click();
		await expect(dialog.getByRole('alert').filter({ hasText: 'did not answer' })).toBeVisible({
			timeout: 15_000
		});
		// Nothing was logged, and the user can try again.
		expect(api.calls).not.toContain('POST /rest/v1/rpc/log_web_open');
		await expect(dialog.getByRole('button', { name: 'TRY AGAIN' })).toBeEnabled();
	});

	test('settings changes the email and signs out everywhere', async ({ page, api }) => {
		await page.goto('/os');
		await page.getByRole('button', { name: 'Settings' }).locator('visible=true').click();
		const dialog = page.getByRole('dialog');
		await dialog.getByRole('tab', { name: 'Security' }).click();
		await dialog.getByLabel('Email').fill('new@example.test');
		await dialog.getByRole('button', { name: 'Update' }).click();
		await expect(dialog.getByText('Confirmation sent to new@example.test')).toBeVisible();
		const update = api.bodies.find((b) => b.call === 'PUT /auth/v1/user')?.body;
		expect(update).toMatchObject({ email: 'new@example.test' });

		await dialog.getByRole('tab', { name: 'Sessions' }).click();
		await dialog.getByRole('button', { name: 'Sign out all' }).click();
		await expect(page).toHaveURL(/\/login$/);
		expect(api.calls).toContain('POST /auth/v1/logout');
	});

	test('log out returns to the sign-in page', async ({ page, api }) => {
		await page.goto('/os');
		// Retry the click: under load it can land before the page has hydrated.
		await expect(async () => {
			await page.getByRole('button', { name: 'Log out' }).click({ timeout: 1000 });
			await expect(page).toHaveURL(/\/login$/, { timeout: 1500 });
		}).toPass();
		expect(api.calls).toContain('POST /auth/v1/logout');
	});

	test('already signed in: the sign-in page redirects home', async ({ page }) => {
		await page.goto('/login');
		await expect(page).toHaveURL(/\/os$/);
	});
});

test.describe('signed out', () => {
	test.use({ signedIn: false });

	test('protected pages redirect to sign-in', async ({ page }) => {
		await page.goto('/os/items/create');
		await expect(page).toHaveURL(/\/login$/);
		await expect(page.getByText('SIGN IN').first()).toBeVisible();
	});

	test('wrong password shows the error and stays', async ({ page }) => {
		await page.goto('/login');
		await page.getByLabel('Email').fill('tester@example.test');
		await page.getByLabel('Password', { exact: true }).fill('nope');
		await page.getByRole('button', { name: 'Sign in', exact: true }).click();
		await expect(page.getByText('Invalid login credentials')).toBeVisible();
		await expect(page).toHaveURL(/\/login$/);
	});

	test('admin signs in', async ({ page }) => {
		await page.goto('/login');
		await page.getByLabel('Email').fill('tester@example.test');
		await page.getByLabel('Password', { exact: true }).fill(PASSWORD);
		await page.getByRole('button', { name: 'Sign in', exact: true }).click();
		await expect(page).toHaveURL(/\/os$/);
		await expect(page.getByRole('heading', { name: 'Users' })).toBeVisible();
	});

	test('a member lands on their keys and gets no admin controls', async ({ page }) => {
		await page.goto('/login');
		await page.getByLabel('Email').fill('nobody@example.test');
		await page.getByLabel('Password', { exact: true }).fill(PASSWORD);
		await page.getByRole('button', { name: 'Sign in', exact: true }).click();
		await expect(page).toHaveURL(/\/os\/keys$/);
		await expect(page.getByRole('heading', { name: 'My keys' })).toBeVisible();
		await expect(page.getByText('Issue key')).toHaveCount(0);
		await expect(page.getByRole('button', { name: 'REMOTE UNLOCK' })).toHaveCount(0);
		await page.goto('/os/items/create');
		await expect(page).toHaveURL(/\/os\/keys$/);
	});

	test('forgot password sends a reset link', async ({ page, api }) => {
		await page.goto('/login');
		await page.getByRole('button', { name: 'Forgot password?' }).click();
		await page.getByLabel('Email').fill('tester@example.test');
		await page.getByRole('button', { name: 'Send reset link' }).click();
		await expect(page.getByText('reset link is on its way')).toBeVisible();
		expect(api.calls).toContain('POST /auth/v1/recover');
	});

	test('register checks the passwords, then asks to confirm the email', async ({ page, api }) => {
		await page.goto('/login');
		await page.getByRole('button', { name: 'Register' }).click();
		await page.getByLabel('Email').fill('new@example.test');
		await page.getByLabel('Password', { exact: true }).fill(PASSWORD);
		await page.getByLabel('Confirm password').fill('something-else');
		await page.getByRole('button', { name: 'Create account' }).click();
		await expect(page.getByText('The passwords do not match.')).toBeVisible();

		await page.getByLabel('Confirm password').fill(PASSWORD);
		await page.getByRole('button', { name: 'Create account' }).click();
		await expect(page.getByText('Check your inbox')).toBeVisible();
		expect(api.calls).toContain('POST /auth/v1/signup');
	});

	test('reset page without a session says the link is invalid', async ({ page }) => {
		await page.goto('/reset-password');
		await expect(page.getByText('invalid or has already been used')).toBeVisible();
	});
});

test.describe('reset link opened (session present)', () => {
	test('sets the new password and goes home', async ({ page, api }) => {
		await page.goto('/reset-password');
		await page.getByLabel('New password').fill('a-brand-new-password');
		await page.getByLabel('Confirm password').fill('a-brand-new-password');
		await page.getByRole('button', { name: 'Save password' }).click();
		await expect(page).toHaveURL(/\/os$/);
		const update = api.bodies.find((b) => b.call === 'PUT /auth/v1/user')?.body;
		expect(update).toMatchObject({ password: 'a-brand-new-password' });
	});

	test('a recovery link that lands on the Site URL still opens the form', async ({ page }) => {
		await page.goto(
			'/#access_token=x&refresh_token=y&expires_in=3600&token_type=bearer&type=recovery'
		);
		await expect(page).toHaveURL(/\/reset-password/);
		await expect(page.getByRole('heading', { name: 'New password' })).toBeVisible();
	});
});
