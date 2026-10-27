// Checks eyes/looks.json: required fields, hex colors, known pupil types, unique ids.
// Usage: node scripts/validate-looks.mjs [path]
import { readFileSync } from 'node:fs';

const PUPILS = ['dot', 'slit', 'acorn', 'round'];
const HEX = /^#[0-9A-Fa-f]{6}$/;
const path = process.argv[2] ?? 'eyes/looks.json';

const errors = [];
let data;
try {
    data = JSON.parse(readFileSync(path, 'utf8'));
} catch (err) {
    console.error(`${path}: ${err.message}`);
    process.exit(1);
}

if (!Array.isArray(data.looks) || data.looks.length === 0) errors.push('"looks" must be a non-empty array');

const seen = new Set();
for (const [i, look] of (data.looks ?? []).entries()) {
    const at = `looks[${i}]${look?.id ? ` (${look.id})` : ''}`;
    if (typeof look.id !== 'string' || !look.id) errors.push(`${at}: "id" must be a non-empty string`);
    else if (seen.has(look.id)) errors.push(`${at}: duplicate id`);
    else seen.add(look.id);
    if (!HEX.test(look.sclera ?? '')) errors.push(`${at}: "sclera" must be a #RRGGBB color`);
    if (!PUPILS.includes(look.pupil)) errors.push(`${at}: "pupil" must be one of ${PUPILS.join(', ')}`);
    if (!Array.isArray(look.pupilColor) || look.pupilColor.length !== 2 || !look.pupilColor.every((c) => HEX.test(c)))
        errors.push(`${at}: "pupilColor" must be [left, right] #RRGGBB colors`);
    if (typeof look.highlight !== 'boolean') errors.push(`${at}: "highlight" must be true or false`);
}

if (errors.length) {
    console.error(`${path}: ${errors.length} error(s)\n- ${errors.join('\n- ')}`);
    process.exit(1);
}
console.log(`${path}: ${data.looks.length} looks OK`);
