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

// Border/text pairs use the same palette as the key detail page.
export const statusClass: Record<KeyStatus, string> = {
	active: 'border-green-500/40 bg-green-500/10 text-green-500',
	disabled: 'border-red-500/40 bg-red-500/10 text-red-500',
	expired: 'border-yellow-500/40 bg-yellow-500/10 text-yellow-500',
	pending: 'border-blue-500/40 bg-blue-500/10 text-blue-500'
};
