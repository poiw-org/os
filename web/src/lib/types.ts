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
	/** The v1 key's old id, which its old token (and the door's entries) still carry. */
	legacyId: string | null;
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
	/** The phone a virtual key is set up on, and when; null until the app has bound one. */
	deviceName: string | null;
	deviceBoundAt: Date | null;
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
	/** The tag as unidentified_tags names it (key id, the token's jti, or "sn:" + serial). */
	tagId: string | null;
	/** Set when an admin opened the door from the web UI: who gave the command. */
	openedBy: string | null;
	/** Opened (or refused) over Bluetooth by a phone, not a tag. */
	viaPhone: boolean;
};

export type ActivityKind =
	| 'registered'
	| 'onboarded'
	| 'answers_edited'
	| 'email_changed'
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
	emailFrom?: string;
	emailTo?: string;
};

/** `label` is for reading and searching ("Full Name · email"); `email` is the address itself. */
export type UserOption = { value: string; label: string; email: string };

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
	/** Has a passkey or an authenticator app. */
	hasSecondFactor: boolean;
	/** Holds a non-expired key or is an admin, and has no second factor yet. */
	secondFactorRequired: boolean;
	/** The requirement can no longer be put off: an admin, or the 14 days are over. */
	secondFactorEnforced: boolean;
	/** When this member's keys will be suspended; unset until the 14 days have started. */
	keysSuspendAt?: Date;
	/** Signed in with a password but has not entered their authenticator code yet. */
	needsCode: boolean;
	/** Signed in with a password on an account with a passkey; an emailed code finishes it. */
	needsEmailCode: boolean;
	/** This session started with a password, which the emailed code can only complete. */
	usedPassword: boolean;
	/** Set while an email change waits for its confirmation link. */
	pendingEmail?: string;
	createdAt: Date;
	lastSignInAt?: Date;
};

/** What a tag's token said about itself, as the door decoded it. */
export type TagClaims = {
	jti: string;
	sub: string;
	aud: string;
	iat: number;
	nbf: number;
	exp: number;
	/** False when the signature did not verify: the claims only help identify the tag. */
	verified: boolean;
};

/** A tag tapped at the door that no existing key accounts for. */
export type UnidentifiedTag = {
	id: string;
	serialNumber: string | null;
	claims: TagClaims | null;
	firstSeen: Date;
	lastSeen: Date;
	sightings: number;
	lastReason: string | null;
	/** Whom an admin attached the tag to; null while it is still unidentified. */
	userId: string | null;
	attachedAt: Date | null;
};
