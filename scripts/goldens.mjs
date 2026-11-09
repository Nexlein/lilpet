// Reference renders of the eyes, drawn by the WebAssembly engine, which gives the same pixels on every machine (no fused
// multiply-add, its own libm), unlike a host build: every reference scene as a PNG file, the end of each part of a
// scripted animation as one PNG to look at, and the whole animation (4 runs, a frame every 16 steps, inputs changing in
// the middle of movements) as a list of frame hashes, all in engine/tests/goldens/ and compared exactly.
// Usage: npm run goldens rewrites what changed, on purpose, when the eyes change; with --check (mise run
// engine:goldens, CI) nothing is written there: a difference fails, and what was drawn instead goes to
// engine/build/goldens/.
import { createHash } from 'node:crypto';
import { mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { loadEngine } from '../engine/wasm/engine.mjs';
import { EMOTIONS, SCENES, lookOf, readJson, swatchesById } from './data.mjs';
import { decodePng, differingPixels, encodePng, rgbOf } from './png.mjs';

const MODULE = 'engine/build/wasm/lilpet.mjs';
const DIR = 'engine/tests/goldens';
const FAILED = 'engine/build/goldens';
const HASHES = 'animation.txt';
const FIRST = 'animation-first-difference.png'; // the first frame of the animation that differs
const RENDER = /^(scene-[a-z0-9-]+\.png|animation\.(png|txt))$/; // what this script writes: the only files it removes
const STEP = 1 / 64; // exact in float, as the engine's tests step
const SAMPLE = 16; // steps between two hashed frames

// The runs of the animation (seed and look), each playing every part from a fresh start
const RUNS = [
    { seed: 1234, swatch: 'cherry', pupil: 'dot' },
    { seed: 99, swatch: 'popcorn', pupil: 'cat' },
    { seed: 2026, swatch: 'bumblebee', pupil: 'acorn' },
    { seed: 7, swatch: 'laser', pupil: 'circle' }
];
// [label, steps, input given at the start]
const PARTS = [
    ['idle, saccades and blinks', 320, () => {}],
    ['following a pointer', 128, (e) => e.setPointer({ x: 0.6, y: -0.3 })],
    ['object turned', 128, (e) => (e.setPointer(null), e.setGravity({ x: Math.sin(1.2), y: Math.cos(1.2) }))],
    ['nearly flat, the tilt trusted slowly', 128, (e) => e.setGravity({ x: 0.3, y: 0.335 })],
    ['upside down, angry', 128, (e) => e.setGravity({ x: 0.1, y: -0.99 })],
    ['shaken, dizzy', 96, (e) => (e.setGravity(null), e.setShaking(true))],
    ['calming down', 64, (e) => e.setShaking(false)],
    ['sad, the gaze lower behind blinks', 512, (e) => e.setEmotion('sad')],
    ['love, with hearts', 320, (e) => e.setEmotion('love')],
    ['petting, with sparkles', 64, (e) => e.react('tap')],
    ['frowning at someone', 96, (e) => e.react('expression_angry')],
    ['sad at a thumb down', 96, (e) => e.react('gesture_dislike')],
    ['surprised by a face', 64, (e) => e.react('face_seen')],
    ['squinting', 64, (e) => e.setEmotion('squint')],
    ['sleepy', 640, (e) => e.setEmotion('sleepy')]
];

for (const [label, steps] of PARTS)
    if (steps % SAMPLE !== 0) throw new Error(`part "${label}": ${steps} steps, not a multiple of ${SAMPLE}`);

const hash = (rgb) => createHash('sha256').update(rgb).digest('hex').slice(0, 16);
const pixelsDiffer = (n) => (n === 1 ? '1 pixel differs' : `${n} pixels differ`);

function requireKnown(names, name, what) {
    if (!names.includes(name)) throw new Error(`the reference animation names an unknown ${what}: "${name}"`);
}

/**
 * The engine, refusing an emotion or an event it does not know: it would ignore it, and a part of the animation would
 * test nothing while its frames still matched.
 */
export function strict(engine, events) {
    return {
        ...engine,
        setEmotion: (name) => (requireKnown(engine.emotions, name, 'emotion'), engine.setEmotion(name)),
        react: (event) => (requireKnown(events, event, 'event'), engine.react(event))
    };
}

/** The frame lines of the text of animation.txt, without its comments */
const framesOf = (text) => text.split('\n').filter((line) => line && !line.startsWith('#'));

/**
 * What the engine draws: { size, images: [{ name, label, height, rgb }], hashes: the text of animation.txt,
 * firstDifferent: the first frame whose line is not the one of `expected` at its place, { line, rgb } }. Only that
 * frame is kept, not the hundreds of others.
 */
async function renders(expected) {
    const engine = strict(
        await loadEngine(pathToFileURL(resolve(MODULE)).href),
        Object.keys(readJson(EMOTIONS).reactions)
    );
    const swatches = swatchesById();
    const images = [];
    for (const scene of readJson(SCENES).scenes) {
        const rgb = rgbOf(engine.renderScene(scene, lookOf(swatches, scene)));
        images.push({ name: `scene-${scene.id}`, label: `scene ${scene.id}`, height: engine.size, rgb });
    }
    const lines = [
        '# Frame hashes of the scripted animation of scripts/goldens.mjs (npm run goldens): do not edit.',
        '# run | part | steps into the part: first 16 hex digits of the SHA-256 of the RGB pixels'
    ];
    let firstDifferent = null;
    let sample = 0;
    const ends = [];
    for (const [r, run] of RUNS.entries()) {
        engine.init(run.seed);
        engine.setLook(lookOf(swatches, run));
        const name = `seed ${run.seed} ${run.swatch} ${run.pupil}`;
        for (const [p, [label, steps, start]] of PARTS.entries()) {
            start(engine);
            for (let s = 1; s <= steps; s++) {
                engine.update(STEP);
                if (s % SAMPLE !== 0) continue;
                const rgb = rgbOf(engine.renderView());
                const line = `${name} | ${p + 1} ${label} | ${s}`;
                lines.push(`${line}: ${hash(rgb)}`);
                if (!firstDifferent && expected[sample] !== lines.at(-1)) firstDifferent = { line, rgb };
                sample++;
                if (r === 0 && s === steps) ends.push(rgb);
            }
        }
    }
    const labels = PARTS.map(([label], i) => `${i + 1}: ${label}`).join('; ');
    images.push({
        name: 'animation',
        label: `end of each part of the first run (${labels})`,
        height: engine.size * ends.length,
        rgb: Buffer.concat(ends)
    });
    return { size: engine.size, images, hashes: `${lines.join('\n')}\n`, firstDifferent };
}

function readText(path) {
    try {
        return readFileSync(path, 'utf8');
    } catch {
        return null; // missing: written, or reported
    }
}

function readImage(path) {
    try {
        return decodePng(readFileSync(path));
    } catch {
        return null; // missing or unreadable: written, or reported
    }
}

/**
 * Compares what was drawn (as renders gives it) with the references of `dir`. A check writes nothing there: what
 * differs goes to `failed`, where what an earlier failure left for a reference that matches again is removed. Otherwise
 * the references that differ are rewritten and those that nothing draws any more are removed.
 * Returns { problems: what a check found, rewritten: how many files were written otherwise }.
 */
export function compare({ size, images, hashes, firstDifferent }, { dir, failed, check }) {
    const problems = [];
    let rewritten = 0;
    mkdirSync(dir, { recursive: true });
    const put = (name, data) => {
        mkdirSync(failed, { recursive: true });
        writeFileSync(`${failed}/${name}`, data);
    };
    const fail = (name, data, message) => {
        put(name, data);
        problems.push(message);
    };
    const clear = (name) => check && rmSync(`${failed}/${name}`, { force: true }); // left by an earlier failure
    for (const image of images) {
        const path = `${dir}/${image.name}.png`;
        const reference = readImage(path);
        const sameSize = reference && reference.width === size && reference.height === image.height;
        if (sameSize && reference.rgb.equals(image.rgb)) {
            clear(`${image.name}.png`);
            continue;
        }
        const png = encodePng(size, image.height, image.rgb);
        if (!check) {
            writeFileSync(path, png);
            rewritten++;
            continue;
        }
        const what = !reference
            ? 'missing or unreadable'
            : sameSize
              ? pixelsDiffer(differingPixels(reference.rgb, image.rgb))
              : 'the reference has another size';
        fail(`${image.name}.png`, png, `${path}: ${image.label}: ${what} (now: ${failed}/${image.name}.png)`);
    }
    const kept = readText(`${dir}/${HASHES}`);
    if (kept === hashes) {
        clear(HASHES);
        clear(FIRST);
    } else if (!check) {
        writeFileSync(`${dir}/${HASHES}`, hashes);
        rewritten++;
    } else {
        let what = 'is missing';
        clear(FIRST);
        if (kept !== null && firstDifferent) {
            put(FIRST, encodePng(size, size, firstDifferent.rgb));
            what = `differs from "${firstDifferent.line}" on (that frame: ${failed}/${FIRST})`;
        } else if (kept !== null) {
            // Every frame drawn is the one kept at its place: the kept file has more, or other comments
            const extra = framesOf(kept).length - framesOf(hashes).length;
            what =
                extra > 0
                    ? `ends with ${extra === 1 ? '1 frame that is' : `${extra} frames that are`} not drawn any more`
                    : 'differs only in its comment or blank lines';
        }
        fail(HASHES, hashes, `${dir}/${HASHES} ${what} (now: ${failed}/${HASHES})`);
    }
    const wanted = new Set([...images.map((i) => `${i.name}.png`), HASHES]);
    for (const file of readdirSync(dir).filter((f) => !wanted.has(f))) {
        const render = RENDER.test(file);
        if (check) problems.push(`${dir}/${file}: ${render ? 'nothing draws it any more' : 'not a reference render'}`);
        else if (render) rmSync(`${dir}/${file}`);
        else console.warn(`${dir}/${file}: not a reference render, left alone`);
    }
    return { problems, rewritten };
}

async function main() {
    const check = process.argv.includes('--check');
    const drawn = await renders(framesOf(readText(`${DIR}/${HASHES}`) ?? ''));
    const { problems, rewritten } = compare(drawn, { dir: DIR, failed: FAILED, check });
    if (problems.length) {
        for (const p of problems) console.error(p);
        console.error('Reference renders differ: if the eyes changed on purpose, run npm run goldens and look at them');
        process.exit(1);
    }
    const what = `${drawn.images.length} images and ${framesOf(drawn.hashes).length} animation frames`;
    console.log(`${DIR}: ${what} ${check ? 'match' : `up to date (${rewritten} files rewritten)`}`);
}

if (import.meta.main) await main();
