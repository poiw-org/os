import { CanvasTexture, Color, SRGBColorSpace } from 'three';

// Shared by the three.js scenes. Import this module dynamically, like three itself, so pages
// without a scene never download it.

let swatch: CanvasRenderingContext2D | undefined;

/** The raw value of a design token from `app.css`. */
export const token = (name: string) =>
	getComputedStyle(document.documentElement).getPropertyValue(name).trim();

/** A design token (any CSS colour, oklch included) as an sRGB colour three.js understands. */
export const tokenColour = (name: string) => {
	swatch ??= document.createElement('canvas').getContext('2d', { willReadFrequently: true })!;
	swatch.clearRect(0, 0, 1, 1);
	swatch.fillStyle = token(name);
	swatch.fillRect(0, 0, 1, 1);
	const [r, g, b] = swatch.getImageData(0, 0, 1, 1).data;
	return new Color().setRGB(r / 255, g / 255, b / 255, SRGBColorSpace);
};

/** A texture drawn once with the 2D canvas API. */
export const canvasTexture = (
	width: number,
	height: number,
	draw: (c: CanvasRenderingContext2D) => void
) => {
	const canvas = document.createElement('canvas');
	canvas.width = width;
	canvas.height = height;
	draw(canvas.getContext('2d')!);
	const map = new CanvasTexture(canvas);
	map.colorSpace = SRGBColorSpace;
	map.anisotropy = 8;
	return map;
};
