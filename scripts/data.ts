// Where the data files and the C tables generated from them are, what they hold, and the look a scene wears: shared by
// the scripts that read them (the validator, the table generators, the reference renders), the data tests and the
// commit hook.
import { readFileSync } from 'node:fs';
import type { Look, PupilType, Scene, Shape } from '../engine/wasm/engine.ts';

export const LOOKS = 'eyes/looks.json';
export const EMOTIONS = 'eyes/emotions.json';
export const SCENES = 'engine/tests/scenes.json';

export const EMOTIONS_TABLE = 'engine/src/emotions_table.h';
export const LOOKS_TABLE = 'engine/src/looks_table.h';
export const SCENES_TABLE = 'engine/tests/scenes_table.h';
// Each table and the script that generates it: npm run tables runs them all (scripts/tables.ts)
export const GENERATORS = {
    [EMOTIONS_TABLE]: 'scripts/gen-emotions-table.ts',
    [LOOKS_TABLE]: 'scripts/gen-looks-table.ts',
    [SCENES_TABLE]: 'scripts/gen-scenes-table.ts'
};

// The fields of an emotion's expression (lp_expression in engine/include/lilpet/lilpet.h), in the order of the struct:
// the value an emotion of eyes/emotions.json takes when it leaves a field out, and the range the engine accepts. One
// table for the C generator (scripts/gen-emotions-table.ts) and the data validator (scripts/validate-data.ts).
export const EXPRESSION = {
    tl: { neutral: 0, range: [0, 1] }, // top lid, down from the top of the eye
    ts: { neutral: 0, range: [-1, 1] }, // slope of the top lid
    bl: { neutral: 0, range: [0, 1] },
    sq: { neutral: 0, range: [0, 0.95] }, // 1 squashes the eye flat: lp_affine_inverse would divide by zero
    cr: { neutral: 0, range: [0, 1] },
    rot: { neutral: 0, range: [-1.2, 1.5] }, // lower turns the eye past a quarter turn with its tilt: the lid comes up
    hl: { neutral: 0, range: [0, 1] }, // above 0.5, the glint shows
    ps: { neutral: 1, range: [0.2, 2] }, // pupil size
    es: { neutral: 1, range: [0.5, 1.5] },
    gy: { neutral: 0, range: [-1, 1] }, // added to the vertical gaze
    spin: { neutral: 0, range: [0, 9] } // dizziness: the gaze circles, the head wobbles
} satisfies Record<string, { neutral: number; range: [number, number] }>;

export type ExpressionField = keyof typeof EXPRESSION;

// What the data files hold, once scripts/validate-data.ts has checked them (the generators and the reference renders
// read them as such)
export interface Swatch {
    id: string;
    sclera: string;
    pupilColor: [string, string];
}

// A pupil type, in fractions of the eye radii: circle is a disc (r), the others ellipses (rx, ry)
export interface PupilShape {
    r?: number;
    rx?: number;
    ry?: number;
    x: number;
    y: number;
}

export interface LooksFile {
    default: { swatch: string; pupil: PupilType };
    eye: Shape;
    pupilLean: number;
    pupils: Record<PupilType, PupilShape>;
    swatches: Swatch[];
}

export type Emotion = Partial<Record<ExpressionField, number>> & {
    label: string;
    note?: string;
    emit?: { type: string; rate: number };
};

export interface Reaction {
    emotion: string;
    duration: number;
    sparkles: number;
    note?: string;
}

export interface EmotionsFile {
    emotions: Record<string, Emotion>;
    reactions: Record<string, Reaction>;
}

// A reference scene: the state it freezes, the swatch and the pupil type it wears
export interface ReferenceScene extends Scene {
    id: string;
    swatch: string;
    pupil: PupilType;
}

export interface ScenesFile {
    scenes: ReferenceScene[];
}

// A data file as read, before scripts/validate-data.ts has checked it: nothing about its shape is assumed
export type Unchecked = any;

export const readJson = <T>(path: string): T => JSON.parse(readFileSync(path, 'utf8'));

/** The swatches of the catalogue by id. */
export const swatchesById = () => new Map(readJson<LooksFile>(LOOKS).swatches.map((s) => [s.id, s]));

/** The look a scene wears, as engine/wasm/engine.ts takes it: its swatch's colors and its pupil type. */
export function lookOf(swatches: Map<string, Swatch>, scene: { swatch: string; pupil: PupilType }): Look {
    const swatch = swatches.get(scene.swatch);
    if (!swatch) throw new Error(`unknown swatch "${scene.swatch}"`);
    return { sclera: swatch.sclera, pupil: scene.pupil, pupilColor: swatch.pupilColor };
}

/** An emotion of eyes/emotions.json with every expression field: the ones it leaves out at their neutral value. */
export const withNeutral = (emotion: Emotion) => ({
    ...(Object.fromEntries(Object.entries(EXPRESSION).map(([k, field]) => [k, field.neutral])) as Record<
        ExpressionField,
        number
    >),
    ...emotion
});
