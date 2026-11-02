// Checks the data files the renderers read:
// - eyes/looks.json: swatches with unique ids and hex colors
// - prototype/scenes.json: unique ids, known swatches, pupil types and emotions, valid blink, head, gaze and particle
//   fields
// Usage: node scripts/validate-data.mjs
import { readFileSync } from 'node:fs';
import { BLINK_PHASES, EMOTIONS, PUPIL_TYPES } from '../prototype/eyes.js';

const HEX = /^#[0-9A-Fa-f]{6}$/;
const errors = [];
const isNum = (v, min = -Infinity, max = Infinity) =>
    typeof v === 'number' && Number.isFinite(v) && v >= min && v <= max;

function load(path) {
    try {
        return JSON.parse(readFileSync(path, 'utf8'));
    } catch (err) {
        errors.push(`${path}: ${err.message}`);
        return null;
    }
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
        if (typeof item.id !== 'string' || !item.id) errors.push(`${at}: "id" must be a non-empty string`);
        else if (seen.has(item.id)) errors.push(`${at}: duplicate id`);
        else seen.add(item.id);
        check(item, (msg) => errors.push(`${at}: ${msg}`));
    }
}

const LOOKS = 'eyes/looks.json';
const looks = load(LOOKS);
eachItem(LOOKS, looks?.swatches, 'swatches', (swatch, fail) => {
    if (!HEX.test(swatch.sclera ?? '')) fail('"sclera" must be a #RRGGBB color');
    const colors = swatch.pupilColor;
    if (!Array.isArray(colors) || colors.length !== 2 || !colors.every((c) => HEX.test(c)))
        fail('"pupilColor" must be [left, right] #RRGGBB colors');
});

const SCENES = 'prototype/scenes.json';
const scenes = load(SCENES);
const swatchIds = new Set((looks?.swatches ?? []).map((s) => s.id));
const SCENE_KEYS = ['id', 'swatch', 'pupil', 'emotion', 'blink', 'head', 'gaze', 'particles'];
const isObject = (v) => typeof v === 'object' && v !== null && !Array.isArray(v);
eachItem(SCENES, scenes?.scenes, 'scenes', (scene, fail) => {
    // A misspelled key would be ignored, and the scene would test less than it says
    for (const k of Object.keys(scene))
        if (!SCENE_KEYS.includes(k)) fail(`unknown key "${k}", expected ${SCENE_KEYS.join(', ')}`);
    for (const k of ['blink', 'head', 'gaze', 'particles'])
        if (scene[k] !== undefined && !isObject(scene[k])) fail(`"${k}" must be an object`);
    if (!swatchIds.has(scene.swatch)) fail(`"swatch" must be a swatch id from ${LOOKS}`);
    if (!PUPIL_TYPES.includes(scene.pupil)) fail(`"pupil" must be one of ${PUPIL_TYPES.join(', ')}`);
    const emotion = scene.emotion ?? 'neutral';
    if (!(emotion in EMOTIONS)) fail(`"emotion" must be one of ${Object.keys(EMOTIONS).join(', ')}`);
    if (scene.blink !== undefined) {
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
    if (scene.particles !== undefined) {
        if (!Number.isInteger(scene.particles.seed) || scene.particles.seed < 0)
            fail('"particles.seed" must be a non-negative integer');
        if (!isNum(scene.particles.time, 0.01, 10)) fail('"particles.time" must be between 0.01 and 10 seconds');
        if (EMOTIONS[emotion] && !EMOTIONS[emotion].emit) fail(`emotion "${emotion}" emits no particles`);
    }
});

if (errors.length) {
    console.error(`${errors.length} data error(s)\n- ${errors.join('\n- ')}`);
    process.exit(1);
}
console.log(`${LOOKS}: ${looks.swatches.length} swatches OK, ${SCENES}: ${scenes.scenes.length} scenes OK`);
