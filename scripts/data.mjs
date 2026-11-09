// Where the data files are, and the look a scene wears: shared by the scripts that read them (the validator, the table
// generators, the reference renders) and by the data tests.
import { readFileSync } from 'node:fs';

export const LOOKS = 'eyes/looks.json';
export const EMOTIONS = 'eyes/emotions.json';
export const SCENES = 'engine/tests/scenes.json';

export const readJson = (path) => JSON.parse(readFileSync(path, 'utf8'));

/** The swatches of the catalogue by id. */
export const swatchesById = () => new Map(readJson(LOOKS).swatches.map((s) => [s.id, s]));

/** The look a scene wears, as engine/wasm/engine.mjs takes it: its swatch's colors and its pupil type. */
export function lookOf(swatches, scene) {
    const swatch = swatches.get(scene.swatch);
    return { sclera: swatch.sclera, pupil: scene.pupil, pupilColor: swatch.pupilColor };
}
