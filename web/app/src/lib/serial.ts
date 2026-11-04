/**
 * Tag serial numbers (NFC UIDs) are stored in one form: lowercase hex bytes joined by ':'
 * ("04:a2:b3:c4"), as the door and Web NFC report them. Accepts the forms people type
 * ("04A2B3C4", "04-a2-b3-c4", "0x04 0xa2 ..."). Returns undefined unless the input is a
 * 4, 7 or 10 byte UID.
 */
export function canonicalSerial(input: string): string | undefined {
	const hex = input
		.trim()
		.toLowerCase()
		.split(/[\s:-]+/)
		.map((part) => (part.startsWith('0x') ? part.slice(2) : part))
		.join('');
	if (!/^[0-9a-f]+$/.test(hex) || ![8, 14, 20].includes(hex.length)) return undefined;
	return hex.match(/../g)!.join(':');
}

/**
 * Whether two stored or typed serial numbers name the same tag. A value that is not a 4, 7 or
 * 10 byte UID (a legacy row) still matches its exact spelling, as before canonicalisation.
 */
export const sameSerial = (a: string | null | undefined, b: string | null | undefined) => {
	if (!a || !b) return false;
	const left = canonicalSerial(a);
	const right = canonicalSerial(b);
	if (left !== undefined && right !== undefined) return left === right;
	return a.trim().toLowerCase() === b.trim().toLowerCase();
};

/** Hex digits only, for loose matching while searching. */
export const serialDigits = (input: string) => input.toLowerCase().replace(/0x|[^0-9a-f]/g, '');
