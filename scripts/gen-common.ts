// What the generators of the engine's C files share (the gen-*-table.ts of scripts/data.ts)
import { readFileSync, writeFileSync } from 'node:fs';
export const emotionEnum = (name: string) => `LP_EMOTION_${name.toUpperCase()}`;
export const pupilEnum = (type: string) => `LP_PUPIL_${type.toUpperCase()}`;
export const hexColor = (hex: string) => `0x${hex.slice(1).toUpperCase()}u`; // "#AE0002" as 0xAE0002u
export const eventEnum = (name: string) => `LP_EVENT_${name.toUpperCase()}`;
export const num = (v: number | undefined) => {
    if (v === undefined || !Number.isFinite(v)) throw new Error(`not a finite number: ${v}`);
    const s = Object.is(v, -0) ? '0' : String(+v.toPrecision(9));
    return /[.e]/.test(s) ? `${s}f` : `${s}.0f`;
};

function current(path: string) {
    try {
        return readFileSync(path, 'utf8');
    } catch {
        return ''; // missing file: reported as out of date
    }
}

// Writes a generated file, or with --check fails if the committed one is out of date
export function writeGenerated(path: string, text: string, summary: string) {
    if (!process.argv.includes('--check')) {
        writeFileSync(path, text);
        console.log(`${path}: ${summary}`);
    } else if (current(path) !== text) {
        console.error(`${path}: out of date, run npm run tables`);
        process.exit(1);
    } else console.log(`${path}: up to date`);
}
