// Reference renders of the eyes, drawn by the WebAssembly engine, which gives the same pixels on every machine (no fused
// multiply-add, its own libm), unlike a host build: every reference scene as a PNG file, the end of each part of a
// scripted animation as one PNG to look at, and the whole animation (4 runs, a frame every 16 steps, inputs changing in
// the middle of movements) as a list of frame hashes, all in engine/tests/goldens/ and compared exactly.
// Usage: npm run goldens rewrites what changed, on purpose, when the eyes change; with --check (mise run engine:wasm, CI)
// nothing is written there: a difference fails, and what was drawn instead goes to engine/build/goldens/.
import { createHash } from 'node:crypto';
import { mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { loadEngine } from '../engine/wasm/engine.mjs';
import { SCENES, lookOf, readJson, swatchesById } from './data.mjs';
import { decodePng, differingPixels, encodePng, rgbOf } from './png.mjs';

const MODULE = 'engine/build/wasm/lilpet.mjs';
const DIR = 'engine/tests/goldens';
const FAILED = 'engine/build/goldens';
const HASHES = 'animation.txt';
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
    ['sad, the gaze lower behind blinks', 256, (e) => e.setEmotion('sad')],
    ['love, with hearts', 320, (e) => e.setEmotion('love')],
    ['petting, with sparkles', 64, (e) => e.react('tap')],
    ['frowning at someone', 96, (e) => e.react('expression_angry')],
    ['sad at a thumb down', 96, (e) => e.react('gesture_dislike')],
    ['surprised by a face', 64, (e) => e.react('face_seen')],
    ['squinting', 64, (e) => e.setEmotion('squint')],
    ['sleepy', 640, (e) => e.setEmotion('sleepy')]
];

const hash = (rgb) => createHash('sha256').update(rgb).digest('hex').slice(0, 16);
const pixelsDiffer = (n) => (n === 1 ? '1 pixel differs' : `${n} pixels differ`);

/** What the engine draws: { size, images: [{ name, label, height, rgb }], hashes: text, frames: Map(line → rgb) } */
export async function renders(modulePath = MODULE) {
    const engine = await loadEngine(pathToFileURL(resolve(modulePath)).href);
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
    const frames = new Map();
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
                frames.set(line, rgb);
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
    return { size: engine.size, images, hashes: `${lines.join('\n')}\n`, frames };
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

function main() {
    const check = process.argv.includes('--check');
    return renders().then(({ size, images, hashes, frames }) => {
        const problems = [];
        let rewritten = 0;
        mkdirSync(DIR, { recursive: true });
        const fail = (name, data, message) => {
            mkdirSync(FAILED, { recursive: true });
            writeFileSync(`${FAILED}/${name}`, data);
            problems.push(message);
        };
        for (const image of images) {
            const path = `${DIR}/${image.name}.png`;
            const kept = readImage(path);
            const sameSize = kept && kept.width === size && kept.height === image.height;
            if (sameSize && kept.rgb.equals(image.rgb)) {
                if (check) rmSync(`${FAILED}/${image.name}.png`, { force: true }); // left by an earlier failure
                continue;
            }
            const png = encodePng(size, image.height, image.rgb);
            if (!check) {
                writeFileSync(path, png);
                rewritten++;
                continue;
            }
            const what = sameSize ? pixelsDiffer(differingPixels(kept.rgb, image.rgb)) : 'no reference of this size';
            fail(`${image.name}.png`, png, `${path}: ${image.label}: ${what} (now: ${FAILED}/${image.name}.png)`);
        }
        const kept = readText(`${DIR}/${HASHES}`);
        if (kept !== hashes) {
            if (!check) {
                writeFileSync(`${DIR}/${HASHES}`, hashes);
                rewritten++;
            } else {
                // The first frame that differs, as an image, and the whole list as drawn now
                const before = new Set((kept ?? '').split('\n'));
                const first = hashes.split('\n').find((l) => l && !l.startsWith('#') && !before.has(l));
                const line = first?.slice(0, first.lastIndexOf(':'));
                if (line && frames.has(line)) {
                    mkdirSync(FAILED, { recursive: true });
                    writeFileSync(`${FAILED}/animation-first-difference.png`, encodePng(size, size, frames.get(line)));
                }
                fail(
                    HASHES,
                    hashes,
                    `${DIR}/${HASHES}: the animation differs from "${line ?? 'its start'}" on (now: ${FAILED}/${HASHES}, ` +
                        `its first different frame: ${FAILED}/animation-first-difference.png)`
                );
            }
        }
        const wanted = new Set([...images.map((i) => `${i.name}.png`), HASHES]);
        for (const file of readdirSync(DIR).filter((f) => !wanted.has(f))) {
            if (check) problems.push(`${DIR}/${file}: nothing draws it any more`);
            else rmSync(`${DIR}/${file}`);
        }
        if (problems.length) {
            for (const p of problems) console.error(p);
            console.error(
                'Reference renders differ: if the eyes changed on purpose, run npm run goldens and look at them'
            );
            process.exit(1);
        }
        const what = `${images.length} images and ${frames.size} animation frames`;
        console.log(`${DIR}: ${what} ${check ? 'match' : `up to date (${rewritten} files rewritten)`}`);
    });
}

if (import.meta.main) await main();
