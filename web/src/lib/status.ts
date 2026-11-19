import { t } from '#lib/i18n.svelte.ts';
import type { KeyStatus, KeySummary } from '#lib/types.ts';

export function keyStatus(
	key: Pick<KeySummary, 'disabled' | 'notBefore' | 'expiresAt'>,
	now = new Date()
): KeyStatus {
	if (key.disabled) return 'disabled';
	if (key.expiresAt <= now) return 'expired';
	if (key.notBefore > now) return 'pending';
	return 'active';
}

/** True when the member has no key that works now or is about to start, so the OS invites them to ask for one. */
export const needsKey = (keys: KeySummary[], userId: string, now = new Date()) =>
	!keys.some((key) => key.userId === userId && ['active', 'pending'].includes(keyStatus(key, now)));

export const statusLabel = (status: KeyStatus) =>
	t(
		{ active: 'ENABLED', disabled: 'DISABLED', expired: 'EXPIRED', pending: 'NOT YET ACTIVE' }[
			status
		]
	);
