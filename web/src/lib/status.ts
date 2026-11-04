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

export const statusLabel: Record<KeyStatus, string> = {
	active: 'ENABLED',
	disabled: 'DISABLED',
	expired: 'EXPIRED',
	pending: 'NOT YET ACTIVE'
};
