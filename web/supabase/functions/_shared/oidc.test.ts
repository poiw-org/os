// Run with: node --test supabase/functions/_shared/oidc.test.ts   (from web/)
import assert from 'node:assert/strict';
import { createPublicKey, createVerify } from 'node:crypto';
import test from 'node:test';
import { jwks, KEY_ID, reissueIdToken } from './oidc.ts';

const SEED = 'unit-test-oidc-seed-not-a-secret-0123';
const ISSUER = 'https://example.supabase.co/functions/v1/oidc';
const part = (value: unknown) => Buffer.from(JSON.stringify(value)).toString('base64url');
const supabaseToken = (claims: Record<string, unknown>) =>
	`${part({ alg: 'ES256', kid: 'supabase' })}.${part(claims)}.not-checked`;

test("the re-signed ID token keeps Supabase's claims, adds roles, and verifies against the JWKS", async () => {
	const original = {
		sub: '11111111-2222-3333-4444-555555555555',
		aud: 'client',
		email: 'a@b.c',
		iss: 'supabase'
	};
	let askedFor = '';
	const token = await reissueIdToken(SEED, supabaseToken(original), ISSUER, async (id) => {
		askedFor = id;
		return ['admin', 'lab-access'];
	});
	const [h, p, s] = token.split('.');
	assert.equal(askedFor, original.sub);
	assert.deepEqual(JSON.parse(Buffer.from(h, 'base64url').toString()), {
		alg: 'RS256',
		typ: 'JWT',
		kid: KEY_ID
	});
	assert.deepEqual(JSON.parse(Buffer.from(p, 'base64url').toString()), {
		...original,
		iss: ISSUER,
		roles: ['admin', 'lab-access']
	});
	const [jwk] = jwks(SEED).keys;
	assert.equal(jwk.kid, KEY_ID);
	const key = createPublicKey({ key: { kty: 'RSA', n: jwk.n, e: jwk.e }, format: 'jwk' });
	assert.ok(
		createVerify('RSA-SHA256').update(`${h}.${p}`).verify(key, Buffer.from(s, 'base64url'))
	);
});

test('a token without a subject is refused rather than signed', async () => {
	await assert.rejects(
		reissueIdToken(SEED, supabaseToken({ aud: 'client' }), ISSUER, async () => [])
	);
});
