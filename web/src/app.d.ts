// See https://svelte.dev/docs/kit/types#app.d.ts
declare global {
	namespace App {}

	// Web NFC (Chrome on Android) is not part of TypeScript's DOM lib.
	class NDEFReader {
		scan(options?: { signal?: AbortSignal }): Promise<void>;
		write(message: string): Promise<void>;
		onreading: ((event: { serialNumber: string }) => void) | null;
		onreadingerror: (() => void) | null;
	}
}

export {};
