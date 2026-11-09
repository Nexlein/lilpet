// The official swatches (eyes/looks.json) and the looks built from them: a look is a swatch worn with a pupil type.
import data from '../../../eyes/looks.json';
import { PUPILS, type Look, type PupilType } from '../../../engine/wasm/engine.mjs';

export type { Look, PupilType };

// What it takes to draw a look: any colors will do, from the catalogue or not
export type LookColors = Pick<Look, 'sclera' | 'pupilColor'>;

export interface Swatch {
    id: string;
    sclera: string;
    pupilColor: readonly [string, string];
}

// Checked by scripts/validate-data.mjs (CI): two pupil colors each
export const SWATCHES: readonly Swatch[] = data.swatches as unknown as Swatch[];

// The look worn until one is chosen, the engine's and the firmware's too (checked: a known swatch and pupil type)
export const DEFAULT_LOOK = { swatchId: data.default.swatch, pupil: data.default.pupil as PupilType };

// The eye shape and the pupil types the engine draws (engine/src/looks_table.h is generated from the same file): radii
// and resting position of a pupil in fractions of the eye radii, r for a disc (a fraction of rx)
export const EYE_SHAPE: { rx: number; ry: number; gap: number; tilt: number } = data.eye;
export const PUPIL_LEAN: number = data.pupilLean;
export type PupilShape = { x: number; y: number } & ({ rx: number; ry: number } | { r: number });
export const PUPIL_SHAPES = data.pupils as Record<PupilType, PupilShape>;

export function makeLook(swatch: Swatch, pupil: PupilType): Look {
    return { id: swatch.id, sclera: swatch.sclera, pupil, pupilColor: swatch.pupilColor };
}

export function findSwatch(id: string): Swatch | undefined {
    return SWATCHES.find((s) => s.id === id);
}

export function isPupilType(value: string): value is PupilType {
    return (PUPILS as readonly string[]).includes(value);
}

export const hasTwoPupilColors = (s: Swatch): boolean => s.pupilColor[0] !== s.pupilColor[1];

// --- Color families, for the gallery filter ----------------------------------------------------------------------

export type Family = 'red' | 'orange' | 'yellow' | 'green' | 'blue' | 'purple' | 'pink' | 'neutral';
export const FAMILIES: readonly Family[] = ['red', 'orange', 'yellow', 'green', 'blue', 'purple', 'pink', 'neutral'];

type Rgb = [number, number, number];

// Red, green and blue in 0..1
const rgb = (hex: string): Rgb => [1, 3, 5].map((i) => Number.parseInt(hex.slice(i, i + 2), 16) / 255) as Rgb;

// Hue in degrees, saturation and lightness in 0..1
export function hsl(hex: string): [number, number, number] {
    const [r, g, b] = rgb(hex);
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

export interface SwatchFilter {
    query: string;
    families: ReadonlySet<Family>; // empty: all of them
    twoColors: boolean;
}

export function filterSwatches({ query, families, twoColors }: SwatchFilter): readonly Swatch[] {
    const q = query.trim().toLowerCase();
    return SWATCHES.filter(
        (s) =>
            s.id.includes(q) && (families.size === 0 || families.has(family(s))) && (!twoColors || hasTwoPupilColors(s))
    );
}

// --- Ink on a color, for the buttons that wear the eye color ---------------------------------------------------------

const LIGHT_INK = '#FFFFFF';
const DARK_INK = '#000000';

// Relative luminance and contrast ratio, as WCAG defines them
function luminance(hex: string): number {
    const [r, g, b] = rgb(hex).map((c) => (c <= 0.04045 ? c / 12.92 : ((c + 0.055) / 1.055) ** 2.4)) as Rgb;
    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

export function contrast(a: string, b: string): number {
    const [dark, light] = [luminance(a), luminance(b)].toSorted((x, y) => x - y) as [number, number];
    return (light + 0.05) / (dark + 0.05);
}

export const inkOn = (hex: string): string =>
    contrast(hex, LIGHT_INK) >= contrast(hex, DARK_INK) ? LIGHT_INK : DARK_INK;
