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

/** `fullName` is empty until the member has answered the questionnaire. */
export type KeyUser = { id: string; email: string; fullName: string };

export type UserSummary = { id: string; email: string; fullName: string; blocked: boolean };

export type KeySummary = {
	id: string;
	userId: string;
	/** Null for a virtual key: it has no tag. */
	serialNumber: string | null;
	notBefore: Date;
	expiresAt: Date;
	disabled: boolean;
	/** Imported from the Firestore setup; its card may still carry the old token. */
	v1: boolean;
};

export type KeyStatus = 'active' | 'disabled' | 'expired' | 'pending';

export type Key = {
	id: string;
	user: KeyUser;
	serialNumber: string | null;
	notBefore: Date;
	expiresAt: Date;
	issuedAt: Date;
	issuedBy: string;
	disabled: boolean;
	serialReleased: boolean;
	jwt: string;
	v1: boolean;
	// Signed with RSA (RS256) rather than the legacy shared secret.
	v2: boolean;
};

export type Entry = {
	id: number;
	keyId: string | null;
	type: EntryType;
	reason?: FailReason;
	timestamp: Date;
	serialNumberFromTag?: string;
	/** Set when an admin opened the door from the web UI: who gave the command. */
	openedBy: string | null;
};

export type ActivityKind =
	| 'registered'
	| 'onboarded'
	| 'answers_edited'
	| 'user_blocked'
	| 'user_unblocked'
	| 'admin_granted'
	| 'admin_revoked'
	| 'key_issued'
	| 'key_renewed'
	| 'key_disabled'
	| 'key_enabled'
	| 'key_tag_released'
	| 'key_deleted';

/** A change to a member or their keys, logged by the database when it happened. */
export type Activity = {
	id: number;
	timestamp: Date;
	userId: string;
	/** Who made the change; null for the member's own sign-up and automatic changes. */
	actorId: string | null;
	kind: ActivityKind;
	keyId: string | null;
	serialNumber: string | null;
	expiresAt?: Date;
};

export type UserOption = { value: string; label: string };

export type CurrentUser = {
	id: string;
	email: string;
	/** Empty until the member has answered the questionnaire. */
	fullName: string;
	nickname: string;
	/** Admins manage keys; everyone else sees and disables their own. */
	isAdmin: boolean;
	/** An admin, or a member holding an active key. */
	canUnlock: boolean;
	/** False until the member has answered the onboarding questions. */
	onboarded: boolean;
	/** Set while an email change waits for its confirmation link. */
	pendingEmail?: string;
	createdAt: Date;
	lastSignInAt?: Date;
};
