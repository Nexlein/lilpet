// The official swatches (eyes/looks.json) and the looks built from them: a look is a swatch worn with a pupil type.
import data from '../../../eyes/looks.json';
import type { Look, PupilType } from '../../../engine/wasm/engine.mjs';

export type { Look, PupilType };

export interface Swatch {
    id: string;
    sclera: string;
    pupilColor: readonly [string, string];
}

// Engine order (lp_pupil)
export const PUPIL_TYPES: readonly PupilType[] = ['dot', 'cat', 'acorn', 'circle'];

// Checked by scripts/validate-data.mjs (CI): two pupil colors each
export const SWATCHES: readonly Swatch[] = data.swatches as unknown as Swatch[];

// Same rule as makeLook in prototype/eyes.js
export function makeLook(swatch: Swatch, pupil: PupilType): Look {
    return { id: swatch.id, sclera: swatch.sclera, pupil, pupilColor: swatch.pupilColor };
}

export function findSwatch(id: string): Swatch | undefined {
    return SWATCHES.find((s) => s.id === id);
}

export function isPupilType(value: string): value is PupilType {
    return (PUPIL_TYPES as readonly string[]).includes(value);
}

export const hasTwoPupilColors = (s: Swatch): boolean => s.pupilColor[0] !== s.pupilColor[1];

// --- Color families, for the gallery filter ----------------------------------------------------------------------

export type Family = 'red' | 'orange' | 'yellow' | 'green' | 'blue' | 'purple' | 'pink' | 'neutral';
export const FAMILIES: readonly Family[] = ['red', 'orange', 'yellow', 'green', 'blue', 'purple', 'pink', 'neutral'];

// Hue in degrees, saturation and lightness in 0..1
export function hsl(hex: string): [number, number, number] {
    const [r, g, b] = [1, 3, 5].map((i) => Number.parseInt(hex.slice(i, i + 2), 16) / 255) as [number, number, number];
    const max = Math.max(r, g, b);
    const min = Math.min(r, g, b);
    const l = (max + min) / 2;
    const d = max - min;
    if (d === 0) return [0, 0, l];
    const s = d / (1 - Math.abs(2 * l - 1));
    let h: number;
    if (max === r) h = ((g - b) / d) % 6;
    else if (max === g) h = (b - r) / d + 2;
    else h = (r - g) / d + 4;
    return [(h * 60 + 360) % 360, s, l];
}

// Family of the eye color (sclera)
export function family(swatch: Swatch): Family {
    const [h, s, l] = hsl(swatch.sclera);
    if (s < 0.2 || l > 0.93 || l < 0.08) return 'neutral';
    if (h < 15 || h >= 345) return 'red';
    if (h < 40) return 'orange';
    if (h < 70) return 'yellow';
    if (h < 170) return 'green';
    if (h < 250) return 'blue';
    if (h < 290) return 'purple';
    return 'pink';
}
