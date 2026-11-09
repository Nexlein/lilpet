// Checks the data files the renderers read:
// - eyes/looks.json: the swatches of the catalogue, with unique ids and hex colors; the default look, eye shape and
//   pupil types, which the engine's table is generated from
// - eyes/emotions.json: emotions and reactions, with known fields in their ranges; each reaction names a known emotion,
//   those the code asks for by name exist, and the ones held while their cause lasts bring no sparkles
// - engine/tests/scenes.json: unique ids, known swatches, pupil types and emotions, valid blink, head, gaze and particle
//   fields (that each scene with particles shows some is an engine test: it takes a simulation)
// Usage: node scripts/validate-data.mjs
import { readFileSync } from 'node:fs';
import { BLINK_PHASES, PUPILS } from '../engine/wasm/engine.mjs';
import { EMOTIONS, LOOKS, SCENES } from './data.mjs';
import { EXPRESSION } from './expression.mjs';

const HEX = /^#[0-9A-Fa-f]{6}$/;
const ID = /^[a-z0-9-]+$/; // ends up in a C string and in a file name (engine/tests/goldens)
const NAME = /^[a-z][a-z0-9_]*$/; // ends up in a C enumerator
const SWATCH_COUNT = 100; // the catalogue: 100 colors and 4 pupils, 400 looks
// From the engine's screen.h: the biggest eye radius and gap the drawing is made for, and the particles it shows
const SCREEN_H = readFileSync('engine/include/lilpet/screen.h', 'utf8');
function screenDefine(name) {
    const found = new RegExp(`#define ${name}\\s+(\\d+)`).exec(SCREEN_H);
    if (!found) throw new Error(`engine/include/lilpet/screen.h: no "#define ${name} <number>"`);
    return Number(found[1]);
}
const HALF_SCREEN = screenDefine('LP_SCREEN_SIZE') / 2;
const MAX_SPARKLES = screenDefine('LP_MAX_PARTICLES');
const MEASURED_TILT = 0.37; // radians: the emotions' turns were tuned for it (docs/architecture.md)
const errors = [];
const isHex = (v) => typeof v === 'string' && HEX.test(v);
const isNum = (v, min = -Infinity, max = Infinity) =>
    typeof v === 'number' && Number.isFinite(v) && v >= min && v <= max;
const isObject = (v) => typeof v === 'object' && v !== null && !Array.isArray(v);

function load(path) {
    try {
        return JSON.parse(readFileSync(path, 'utf8'));
    } catch (err) {
        errors.push(`${path}: ${err.message}`);
        return null;
    }
}

function report() {
    if (!errors.length) return;
    console.error(`${errors.length} data error(s)\n- ${errors.join('\n- ')}`);
    process.exit(1);
}

// Checks the shared "list of objects with a unique id" part and calls `check` on each item
function eachItem(path, list, name, check) {
    if (!Array.isArray(list) || list.length === 0) {
        errors.push(`${path}: "${name}" must be a non-empty array`);
        return;
    }
    const seen = new Set();
    for (const [i, item] of list.entries()) {
        const at = `${path} ${name}[${i}]${item?.id ? ` (${item.id})` : ''}`;
        if (!isObject(item)) {
            errors.push(`${at}: must be an object`);
            continue;
        }
        if (typeof item.id !== 'string' || !ID.test(item.id)) errors.push(`${at}: "id" must match ${ID}`);
        else if (seen.has(item.id)) errors.push(`${at}: duplicate id`);
        else seen.add(item.id);
        check(item, (msg) => errors.push(`${at}: ${msg}`));
    }
}

// Same for an object of named rows, the ones the code asks for by name, and each row's keys: a misspelled one would be
// ignored
function eachRow(path, rows, name, required, keys, check) {
    if (!isObject(rows) || Object.keys(rows).length === 0) {
        errors.push(`${path}: "${name}" must be a non-empty object`);
        return;
    }
    for (const id of required)
        if (!Object.hasOwn(rows, id)) errors.push(`${path}: "${name}.${id}" is missing, the eyes need it`);
    for (const [id, row] of Object.entries(rows)) {
        const fail = (msg) => errors.push(`${path} ${name}.${id}: ${msg}`);
        if (!NAME.test(id)) fail(`the name must match ${NAME}`);
        if (!isObject(row)) {
            fail('must be an object');
            continue;
        }
        for (const k of Object.keys(row))
            if (!keys.includes(k)) fail(`unknown key "${k}", expected ${keys.join(', ')}`);
        if (row.note !== undefined && (typeof row.note !== 'string' || /\/\*|\*\//.test(row.note)))
            fail('"note" must be a string without /* or */: it becomes a C comment');
        check(row, fail, id);
    }
}

const looks = load(LOOKS);
eachItem(LOOKS, looks?.swatches, 'swatches', (swatch, fail) => {
    if (!isHex(swatch.sclera)) fail('"sclera" must be a #RRGGBB color');
    const colors = swatch.pupilColor;
    if (!Array.isArray(colors) || colors.length !== 2 || !colors.every(isHex))
        fail('"pupilColor" must be [left, right] #RRGGBB colors');
});
if (Array.isArray(looks?.swatches) && looks.swatches.length !== SWATCH_COUNT)
    errors.push(`${LOOKS}: ${looks.swatches.length} swatches, the catalogue has ${SWATCH_COUNT}`);
if (!looks?.swatches?.some?.((s) => s?.id === looks?.default?.swatch))
    errors.push(`${LOOKS}: "default.swatch" must be a swatch id`);
if (!PUPILS.includes(looks?.default?.pupil))
    errors.push(`${LOOKS}: "default.pupil" must be one of ${PUPILS.join(', ')}`);
// The eye shape the drawing is made for
for (const [k, min, max] of [
    ['rx', 1, HALF_SCREEN],
    ['ry', 1, HALF_SCREEN],
    ['gap', 0, HALF_SCREEN],
    ['tilt', 0, MEASURED_TILT]
])
    if (!isNum(looks?.eye?.[k], min, max)) errors.push(`${LOOKS}: "eye.${k}" must be between ${min} and ${max}`);
if (!isNum(looks?.pupilLean, 0, 0.5)) errors.push(`${LOOKS}: "pupilLean" must be between 0 and 0.5 radians`);
// Every pupil type of the engine, in fractions of the eye radii; the renderer draws circle as a disc (r)
eachRow(LOOKS, looks?.pupils, 'pupils', PUPILS, ['rx', 'ry', 'r', 'x', 'y'], (pupil, fail, id) => {
    if (!PUPILS.includes(id)) fail(`must be one of ${PUPILS.join(', ')}`);
    const radii = id === 'circle' ? ['r'] : ['rx', 'ry'];
    for (const k of ['rx', 'ry', 'r'])
        if (radii.includes(k) ? !isNum(pupil[k], 0.01, 1.2) : pupil[k] !== undefined)
            fail(radii.includes(k) ? `"${k}" must be between 0.01 and 1.2` : `"${k}" is not used: ${radii.join(', ')}`);
    for (const k of ['x', 'y']) if (!isNum(pupil[k], -1, 1)) fail(`"${k}" must be between -1 and 1`);
});

const emotionData = load(EMOTIONS);
const emotions = emotionData?.emotions ?? {};
const EMIT_TYPES = ['heart', 'sparkle'];
const HELD = ['shake', 'upside_down']; // they repeat every frame while their cause lasts
eachRow(
    EMOTIONS,
    emotions,
    'emotions',
    ['neutral', 'sleepy'],
    ['label', 'emit', 'note', ...Object.keys(EXPRESSION)],
    (emotion, fail) => {
        if (typeof emotion.label !== 'string' || !emotion.label) fail('"label" must be a non-empty string');
        for (const [
            k,
            {
                range: [min, max]
            }
        ] of Object.entries(EXPRESSION))
            if (emotion[k] !== undefined && !isNum(emotion[k], min, max))
                fail(`"${k}" must be between ${min} and ${max}`);
        if (emotion.emit === undefined) return;
        if (!EMIT_TYPES.includes(emotion.emit?.type)) fail(`"emit.type" must be one of ${EMIT_TYPES.join(', ')}`);
        if (!isNum(emotion.emit?.rate, 0.01, 30)) fail('"emit.rate" must be between 0.01 and 30 particles per second');
    }
);
eachRow(
    EMOTIONS,
    emotionData?.reactions,
    'reactions',
    [...HELD, 'tap', 'face_seen'],
    ['emotion', 'duration', 'sparkles', 'note'],
    (reaction, fail, id) => {
        if (!Object.hasOwn(emotions, reaction.emotion))
            fail(`"emotion" must be one of ${Object.keys(emotions).join(', ')}`);
        if (!isNum(reaction.duration, 0.01, 60)) fail('"duration" must be between 0.01 and 60 seconds');
        if (!Number.isInteger(reaction.sparkles) || !isNum(reaction.sparkles, 0, MAX_SPARKLES))
            fail(`"sparkles" must be a whole number between 0 and ${MAX_SPARKLES}`);
        else if (HELD.includes(id) && reaction.sparkles !== 0) fail('"sparkles" must be 0: it repeats while held');
    }
);
report(); // the scenes are checked against the swatches and the emotions: they must be right first

const scenes = load(SCENES);
const swatchIds = new Set(looks.swatches.map((s) => s.id));
const SCENE_KEYS = ['id', 'swatch', 'pupil', 'emotion', 'blink', 'head', 'gaze', 'particles'];
const MAX_SEED = 2 ** 32 - 1; // a uint32_t in the engine: the C scene table would not compile past it
eachItem(SCENES, scenes?.scenes, 'scenes', (scene, fail) => {
    // A misspelled key would be ignored, and the scene would test less than it says
    for (const k of Object.keys(scene))
        if (!SCENE_KEYS.includes(k)) fail(`unknown key "${k}", expected ${SCENE_KEYS.join(', ')}`);
    for (const k of ['blink', 'head', 'gaze', 'particles'])
        if (scene[k] !== undefined && !isObject(scene[k])) fail(`"${k}" must be an object`);
    if (!swatchIds.has(scene.swatch)) fail(`"swatch" must be a swatch id from ${LOOKS}`);
    if (!PUPILS.includes(scene.pupil)) fail(`"pupil" must be one of ${PUPILS.join(', ')}`);
    const emotion = scene.emotion ?? 'neutral';
    const known = Object.hasOwn(emotions, emotion);
    if (!known) fail(`"emotion" must be one of ${Object.keys(emotions).join(', ')}`);
    if (isObject(scene.blink)) {
        const phases = BLINK_PHASES.filter(Boolean);
        if (!phases.includes(scene.blink.phase)) fail(`"blink.phase" must be one of ${phases.join(', ')}`);
        if (!isNum(scene.blink.t, 0, 1)) fail('"blink.t" must be a number of seconds between 0 and 1');
    }
    // Roll goes past a quarter turn when the object is turned (tilt)
    for (const k of Object.keys(scene.head ?? {}))
        if (
            !['yaw', 'pitch', 'roll'].includes(k) ||
            !isNum(scene.head[k], k === 'roll' ? -Math.PI : -1, k === 'roll' ? Math.PI : 1)
        )
            fail(`"head.${k}" must be yaw, pitch (radians, -1 to 1) or roll (radians, -pi to pi)`);
    for (const k of Object.keys(scene.gaze ?? {}))
        if (!['x', 'y'].includes(k) || !isNum(scene.gaze[k], -1, 1))
            fail(`"gaze.${k}" must be x or y, between -1 and 1`);
    if (isObject(scene.particles)) {
        if (!Number.isInteger(scene.particles.seed) || !isNum(scene.particles.seed, 0, MAX_SEED))
            fail(`"particles.seed" must be a whole number between 0 and ${MAX_SEED}`);
        // Exact in a float, so the engine replays a whole number of steps
        if (!isNum(scene.particles.time, 0.25, 10) || !Number.isInteger(scene.particles.time * 4))
            fail('"particles.time" must be a multiple of 0.25 seconds, up to 10');
        if (known && !emotions[emotion].emit) fail(`emotion "${emotion}" emits no particles`);
    }
});

report();
console.log(
    `${LOOKS}: ${looks.swatches.length} swatches OK, ` +
        `${EMOTIONS}: ${Object.keys(emotions).length} emotions and ${Object.keys(emotionData.reactions).length} reactions OK, ` +
        `${SCENES}: ${scenes.scenes.length} scenes OK`
);
