// Checks the data files the renderers read:
// - eyes/looks.json: required fields, hex colors, known pupil types, unique ids
// - prototype/scenes.json: unique ids, known looks and emotions, valid blink, head, gaze and particle fields
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
eachItem(LOOKS, looks?.looks, 'looks', (look, fail) => {
    if (!HEX.test(look.sclera ?? '')) fail('"sclera" must be a #RRGGBB color');
    if (!PUPIL_TYPES.includes(look.pupil)) fail(`"pupil" must be one of ${PUPIL_TYPES.join(', ')}`);
    if (!Array.isArray(look.pupilColor) || look.pupilColor.length !== 2 || !look.pupilColor.every((c) => HEX.test(c)))
        fail('"pupilColor" must be [left, right] #RRGGBB colors');
    if (typeof look.highlight !== 'boolean') fail('"highlight" must be true or false');
});

const SCENES = 'prototype/scenes.json';
const scenes = load(SCENES);
const lookIds = new Set((looks?.looks ?? []).map((l) => l.id));
eachItem(SCENES, scenes?.scenes, 'scenes', (scene, fail) => {
    if (!lookIds.has(scene.look)) fail(`"look" must be a look id from ${LOOKS}`);
    const emotion = scene.emotion ?? 'neutral';
    if (!(emotion in EMOTIONS)) fail(`"emotion" must be one of ${Object.keys(EMOTIONS).join(', ')}`);
    if (scene.blink !== undefined) {
        if (!BLINK_PHASES.includes(scene.blink.phase)) fail(`"blink.phase" must be one of ${BLINK_PHASES.join(', ')}`);
        if (!isNum(scene.blink.t, 0, 1)) fail('"blink.t" must be a number of seconds between 0 and 1');
    }
    for (const k of Object.keys(scene.head ?? {}))
        if (!['yaw', 'pitch', 'roll'].includes(k) || !isNum(scene.head[k], -1, 1))
            fail(`"head.${k}" must be yaw, pitch or roll, in radians between -1 and 1`);
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
console.log(`${LOOKS}: ${looks.looks.length} looks OK, ${SCENES}: ${scenes.scenes.length} scenes OK`);
