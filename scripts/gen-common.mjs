// What the generators of the engine's C files share (gen-engine-vectors.mjs, gen-emotions-table.mjs)
import { readFileSync, writeFileSync } from 'node:fs';
import { BASE } from '../prototype/eyes.js';

export const EXPR = Object.keys(BASE); // the fields of lp_expression: a new one breaks the C build until ported
export const emotionEnum = (name) => `LP_EMOTION_${name.toUpperCase()}`;
export const eventEnum = (name) => `LP_EVENT_${name.toUpperCase()}`;
export const num = (v) => {
    if (!Number.isFinite(v)) throw new Error(`not a finite number: ${v}`);
    const s = Object.is(v, -0) ? '0' : String(+v.toPrecision(9));
    return /[.e]/.test(s) ? `${s}f` : `${s}.0f`;
};

function current(path) {
    try {
        return readFileSync(path, 'utf8');
    } catch {
        return ''; // missing file: reported as out of date
    }
}

// Writes a generated file, or with --check fails if the committed one is out of date
export function writeGenerated(path, text, summary) {
    if (!process.argv.includes('--check')) {
        writeFileSync(path, text);
        console.log(`${path}: ${summary}`);
    } else if (current(path) !== text) {
        console.error(`${path}: out of date, run npm run vectors`);
        process.exit(1);
    } else console.log(`${path}: up to date`);
}
