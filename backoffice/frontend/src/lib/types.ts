export enum EntryType {
	FAILED = 'failed',
	SUCCESSFUL = 'successful'
}

export enum FailReason {
	UNKNOWN = 'Unknown',
	KEY_EXPIRED = 'KeyExpired',
	KEY_BLACKLISTED = 'KeyBlacklisted',
	KEY_NOT_YET_ACTIVATED = 'KeyNotYetActivated',
	ISSUING_DATE_IN_THE_FUTURE = 'IssuingDateInTheFuture',
	SERIAL_NUMBER_MISMATCH = 'SerialNumberMismatch'
}

export type KeyUser = { id: string; email: string };

export type UserSummary = { id: string; email: string; blocked: boolean };

export type KeySummary = {
	id: string;
	userId: string;
	serialNumber: string;
	notBefore: Date;
	expiresAt: Date;
	disabled: boolean;
};

export type KeyStatus = 'active' | 'disabled' | 'expired' | 'pending';

export type Key = {
	id: string;
	user: KeyUser;
	serialNumber: string;
	notBefore: Date;
	expiresAt: Date;
	issuedAt: Date;
	issuedBy: string;
	disabled: boolean;
	jwt: string;
};

export type Entry = {
	id: number;
	keyId: string | null;
	type: EntryType;
	reason?: FailReason;
	timestamp: Date;
	serialNumberFromTag?: string;
};

export type UserOption = { value: string; label: string };

export type CurrentUser = { id: string; email: string; nickname: string };
