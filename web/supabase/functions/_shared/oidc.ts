// The OIDC ID token apps such as Cloudflare Access read: Supabase's own, re-signed with the user's
// roles added, because Supabase's ID token has a fixed set of claims and no hook reaches it.
// Signed with a key pair derived from OIDC_KEY_SEED, never the door's RSA_KEY_SEED.
import { Buffer } from 'node:buffer';
import { publicModulus, signRs256 } from './rsa.ts';

export const KEY_ID = 'poiw-oidc-1';

const base64Url = (value: unknown) => Buffer.from(JSON.stringify(value)).toString('base64url');

/** The public key apps verify ID tokens with, as a JWKS. */
export const jwks = (seed: string) => ({
	keys: [{ kty: 'RSA', use: 'sig', alg: 'RS256', kid: KEY_ID, n: publicModulus(seed), e: 'AQAB' }]
});

/**
 * Supabase's ID token with `roles` added and `iss` set to ours. Its signature is not checked: the
 * token comes straight from Supabase's token endpoint in the same request, over TLS.
 */
export async function reissueIdToken(
	seed: string,
	idToken: string,
	issuer: string,
	rolesOf: (userId: string) => Promise<string[]>
) {
	const claims = JSON.parse(Buffer.from(idToken.split('.')[1] ?? '', 'base64url').toString());
	if (typeof claims?.sub !== 'string') throw new Error('ID token has no subject');
	const roles = await rolesOf(claims.sub);
	const signed = `${base64Url({ alg: 'RS256', typ: 'JWT', kid: KEY_ID })}.${base64Url({ ...claims, iss: issuer, roles })}`;
	return `${signed}.${await signRs256(seed, signed)}`;
}
