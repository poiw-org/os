import { toast } from '#lib/notify.svelte.ts';

/** Web NFC exists only in Chrome on Android. */
export const nfcSupported = typeof window !== 'undefined' && 'NDEFReader' in window;

/**
 * Starts a Web NFC scan and calls `onTag` for every tag read. Failures are shown as toasts and
 * reported through `onStop`, which also runs when the scan is stopped. Resolves a stop function,
 * or undefined when the scan could not start.
 */
export async function scanTag(
	onTag: (serialNumber: string, ndef: NDEFReader, stop: () => void) => void | Promise<void>,
	onStop: () => void = () => {}
): Promise<(() => void) | undefined> {
	if (!nfcSupported) {
		toast.error(
			'Scanning only works in Chrome on Android. Switch browsers or enter the S/N manually.'
		);
		onStop();
		return;
	}

	const ndef = new NDEFReader();
	const controller = new AbortController();
	const stop = () => {
		controller.abort();
		onStop();
	};

	try {
		await ndef.scan({ signal: controller.signal });
	} catch (error) {
		toast.error(`Scan failed to start: ${error}.`);
		onStop();
		return;
	}

	ndef.onreadingerror = () => {
		toast.error('Cannot read data from the NFC tag. Try another one?');
		stop();
	};
	ndef.onreading = (event) => void onTag(event.serialNumber, ndef, stop);
	return stop;
}
