// Where the data files and the C tables generated from them are, and the look a scene wears: shared by the scripts
// that read them (the validator, the table generators, the reference renders), the data tests and the commit hook.
import { readFileSync } from 'node:fs';

export const LOOKS = 'eyes/looks.json';
export const EMOTIONS = 'eyes/emotions.json';
export const SCENES = 'engine/tests/scenes.json';

export const EMOTIONS_TABLE = 'engine/src/emotions_table.h';
export const LOOKS_TABLE = 'engine/src/looks_table.h';
export const SCENES_TABLE = 'engine/tests/scenes_table.h';
// Each table and the script that generates it: npm run tables runs them all (scripts/tables.mjs)
export const GENERATORS = {
    [EMOTIONS_TABLE]: 'scripts/gen-emotions-table.mjs',
    [LOOKS_TABLE]: 'scripts/gen-looks-table.mjs',
    [SCENES_TABLE]: 'scripts/gen-scenes-table.mjs'
};

export const readJson = (path) => JSON.parse(readFileSync(path, 'utf8'));

/** The swatches of the catalogue by id. */
export const swatchesById = () => new Map(readJson(LOOKS).swatches.map((s) => [s.id, s]));

/** The look a scene wears, as engine/wasm/engine.mjs takes it: its swatch's colors and its pupil type. */
export function lookOf(swatches, scene) {
    const swatch = swatches.get(scene.swatch);
    return { sclera: swatch.sclera, pupil: scene.pupil, pupilColor: swatch.pupilColor };
}
