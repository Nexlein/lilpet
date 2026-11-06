// Checks the data files the renderers read:
// - eyes/looks.json: the swatches of the catalogue, with unique ids and hex colors
// - eyes/emotions.json: emotions and reactions, with known fields in their ranges; each reaction names a known emotion,
//   those the code asks for by name exist, and the ones held while their cause lasts bring no sparkles
// - prototype/scenes.json: unique ids, known swatches, pupil types and emotions, valid blink, head, gaze and particle
//   fields
// Usage: node scripts/validate-data.mjs
import { readFileSync } from 'node:fs';

const HEX = /^#[0-9A-Fa-f]{6}$/;
const ID = /^[a-z0-9-]+$/; // ends up in the pages' HTML
const NAME = /^[a-z][a-z0-9_]*$/; // ends up in a C enumerator
const SWATCH_COUNT = 100; // the catalogue: 100 colors and 4 pupils, 400 looks
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

const LOOKS = 'eyes/looks.json';
const looks = load(LOOKS);
eachItem(LOOKS, looks?.swatches, 'swatches', (swatch, fail) => {
    if (!isHex(swatch.sclera)) fail('"sclera" must be a #RRGGBB color');
    const colors = swatch.pupilColor;
    if (!Array.isArray(colors) || colors.length !== 2 || !colors.every(isHex))
        fail('"pupilColor" must be [left, right] #RRGGBB colors');
});
if (Array.isArray(looks?.swatches) && looks.swatches.length !== SWATCH_COUNT)
    errors.push(`${LOOKS}: ${looks.swatches.length} swatches, the catalogue has ${SWATCH_COUNT}`);

const EMOTIONS = 'eyes/emotions.json';
const emotionData = load(EMOTIONS);
const emotions = emotionData?.emotions ?? {};
// Expression fields (BASE in prototype/eyes.js) and their ranges
const EXPRESSION = {
    tl: [0, 1],
    ts: [-1, 1],
    bl: [0, 1],
    sq: [0, 0.95], // 1 squashes the eye flat: lp_affine_inverse would divide by zero
    cr: [0, 1],
    rot: [-1.2, 1.5], // lower turns the eye past a quarter turn with the 0.37 tilt: the lid would come up from below
    hl: [0, 1],
    ps: [0.2, 2],
    es: [0.5, 1.5],
    gy: [-1, 1],
    spin: [0, 9]
};
const EMIT_TYPES = ['heart', 'sparkle'];
const MAX_SPARKLES = 32; // LP_MAX_PARTICLES
const HELD = ['shake', 'upside_down']; // they repeat every frame while their cause lasts
eachRow(
    EMOTIONS,
    emotions,
    'emotions',
    ['neutral', 'sleepy'],
    ['label', 'emit', 'note', ...Object.keys(EXPRESSION)],
    (emotion, fail) => {
        if (typeof emotion.label !== 'string' || !emotion.label) fail('"label" must be a non-empty string');
        for (const [k, [min, max]] of Object.entries(EXPRESSION))
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
report(); // eyes.js cannot load without its emotions

const { BLINK_PHASES, Eyes } = await import('../prototype/eyes.js');
const { PUPIL_TYPES } = await import('../prototype/draw.js');

const SCENES = 'prototype/scenes.json';
const scenes = load(SCENES);
const swatchIds = new Set(looks.swatches.map((s) => s.id));
const SCENE_KEYS = ['id', 'swatch', 'pupil', 'emotion', 'blink', 'head', 'gaze', 'particles'];
const MAX_SEED = 2 ** 32 - 1; // a uint32_t in the engine: the C vectors would not compile past it
eachItem(SCENES, scenes?.scenes, 'scenes', (scene, fail) => {
    // A misspelled key would be ignored, and the scene would test less than it says
    for (const k of Object.keys(scene))
        if (!SCENE_KEYS.includes(k)) fail(`unknown key "${k}", expected ${SCENE_KEYS.join(', ')}`);
    for (const k of ['blink', 'head', 'gaze', 'particles'])
        if (scene[k] !== undefined && !isObject(scene[k])) fail(`"${k}" must be an object`);
    if (!swatchIds.has(scene.swatch)) fail(`"swatch" must be a swatch id from ${LOOKS}`);
    if (!PUPIL_TYPES.includes(scene.pupil)) fail(`"pupil" must be one of ${PUPIL_TYPES.join(', ')}`);
    const emotion = scene.emotion ?? 'neutral';
    const known = Object.hasOwn(emotions, emotion);
    if (!known) fail(`"emotion" must be one of ${Object.keys(emotions).join(', ')}`);
    if (isObject(scene.blink)) {
        if (!BLINK_PHASES.includes(scene.blink.phase)) fail(`"blink.phase" must be one of ${BLINK_PHASES.join(', ')}`);
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
        // Whole steps of 1/60 s, and exact in a float: JS and C replay as many of them
        const timeOk = isNum(scene.particles.time, 0.25, 10) && Number.isInteger(scene.particles.time * 4);
        if (!timeOk) fail('"particles.time" must be a multiple of 0.25 seconds, up to 10');
        if (known && !emotions[emotion].emit) fail(`emotion "${emotion}" emits no particles`);
        else if (known && timeOk) {
            const eyes = new Eyes(1);
            eyes.setScene(scene, null);
            if (eyes.particles.length === 0) fail('no particle is alive at "particles.time"');
        }
    }
});

report();
console.log(
    `${LOOKS}: ${looks.swatches.length} swatches OK, ` +
        `${EMOTIONS}: ${Object.keys(emotions).length} emotions and ${Object.keys(emotionData.reactions).length} reactions OK, ` +
        `${SCENES}: ${scenes.scenes.length} scenes OK`
);
