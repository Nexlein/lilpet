// Reference renders of the eyes: the WebAssembly engine draws every reference scene and a short scripted animation,
// kept as PNG files in engine/tests/goldens/ and compared pixel for pixel. The WebAssembly build gives the same pixels on
// every machine (no fused multiply-add, its own libm), unlike a host build, which may use FMA.
// Usage: npm run goldens rewrites the files whose pixels changed, on purpose, when the eyes change; with --check
// (mise run engine:wasm, CI) nothing is written there: a difference fails, its image goes to engine/build/goldens/.
import { mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { crc32, deflateSync, inflateSync } from 'node:zlib';
import { loadEngine } from '../engine/wasm/engine.mjs';

const MODULE = 'engine/build/wasm/lilpet.mjs';
const SCENES = 'engine/tests/scenes.json';
const LOOKS = 'eyes/looks.json';
const DIR = 'engine/tests/goldens';
const FAILED = 'engine/build/goldens';
const STEP = 1 / 64; // a step exact in float, as the engine's tests use

// --- PNG, RGB 8 bits, each row filtered with "Sub": enough for flat eye colors on black ----------------------------

const PNG_SIGNATURE = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]);

function chunk(type, data) {
    const head = Buffer.alloc(8);
    head.writeUInt32BE(data.length, 0);
    head.write(type, 4, 'latin1');
    const crc = Buffer.alloc(4);
    crc.writeUInt32BE(crc32(Buffer.concat([head.subarray(4), data])), 0);
    return Buffer.concat([head, data, crc]);
}

/** A PNG of `rgb` (width × height × 3 bytes). */
export function encodePng(width, height, rgb) {
    const stride = width * 3;
    const raw = Buffer.alloc((stride + 1) * height);
    for (let y = 0; y < height; y++) {
        const row = y * (stride + 1);
        raw[row] = 1; // Sub: each byte minus the same byte of the pixel to its left
        for (let i = 0; i < stride; i++)
            raw[row + 1 + i] = (rgb[y * stride + i] - (i >= 3 ? rgb[y * stride + i - 3] : 0)) & 0xff;
    }
    const header = Buffer.alloc(13);
    header.writeUInt32BE(width, 0);
    header.writeUInt32BE(height, 4);
    header.set([8, 2, 0, 0, 0], 8); // 8 bits, RGB, deflate, adaptive filters, no interlace
    return Buffer.concat([
        PNG_SIGNATURE,
        chunk('IHDR', header),
        chunk('IDAT', deflateSync(raw, { level: 9 })),
        chunk('IEND', Buffer.alloc(0))
    ]);
}

/** The pixels of a PNG written by encodePng: { width, height, rgb }. */
export function decodePng(file) {
    if (!file.subarray(0, 8).equals(PNG_SIGNATURE)) throw new Error('not a PNG file');
    let width = 0;
    let height = 0;
    const data = [];
    for (let at = 8; at < file.length;) {
        const length = file.readUInt32BE(at);
        const type = file.toString('latin1', at + 4, at + 8);
        const body = file.subarray(at + 8, at + 8 + length);
        if (type === 'IHDR') {
            width = body.readUInt32BE(0);
            height = body.readUInt32BE(4);
            if (body[8] !== 8 || body[9] !== 2 || body[12] !== 0) throw new Error('only 8-bit RGB, not interlaced');
        } else if (type === 'IDAT') data.push(body);
        at += 12 + length;
    }
    const raw = inflateSync(Buffer.concat(data));
    const stride = width * 3;
    const rgb = Buffer.alloc(stride * height);
    for (let y = 0; y < height; y++) {
        const filter = raw[y * (stride + 1)];
        if (filter > 1) throw new Error(`row ${y}: filter ${filter}, only None and Sub are read`);
        for (let i = 0; i < stride; i++) {
            const left = filter === 1 && i >= 3 ? rgb[y * stride + i - 3] : 0;
            rgb[y * stride + i] = (raw[y * (stride + 1) + 1 + i] + left) & 0xff;
        }
    }
    return { width, height, rgb };
}

/** The RGB bytes of RGBA pixels. */
export function rgbOf(rgba) {
    const rgb = Buffer.alloc((rgba.length / 4) * 3);
    for (let p = 0, q = 0; p < rgba.length; p += 4, q += 3) rgb.set(rgba.subarray(p, p + 3), q);
    return rgb;
}

/** How many pixels of two RGB images of the same size differ. */
export function differingPixels(a, b) {
    let count = 0;
    for (let i = 0; i < a.length; i += 3) if (a[i] !== b[i] || a[i + 1] !== b[i + 1] || a[i + 2] !== b[i + 2]) count++;
    return count;
}

const count = (n) => (n === 1 ? '1 pixel differs' : `${n} pixels differ`);

// --- What is drawn --------------------------------------------------------------------------------------------------

const makeLook = (swatch, pupil) => ({ sclera: swatch.sclera, pupil, pupilColor: swatch.pupilColor });

// The animation: one frame at the end of each part, from a fixed seed and fixed steps, so a failure replays
const ANIMATION = [
    ['idle, saccades and blinks', 320, () => {}],
    ['following a pointer', 128, (e) => e.setPointer({ x: 0.6, y: -0.3 })],
    ['object turned', 128, (e) => (e.setPointer(null), e.setGravity({ x: Math.sin(1.2), y: Math.cos(1.2) }))],
    ['upside down, angry', 128, (e) => e.setGravity({ x: 0.1, y: -0.99 })],
    ['shaken, dizzy', 96, (e) => (e.setGravity(null), e.setShaking(true))],
    ['calming down', 64, (e) => e.setShaking(false)],
    ['love, with hearts', 320, (e) => e.setEmotion('love')],
    ['petting, with sparkles', 64, (e) => e.react('tap')],
    ['frowning at someone', 96, (e) => e.react('expression_angry')],
    ['sad at a thumb down', 96, (e) => e.react('gesture_dislike')],
    ['surprised by a face', 64, (e) => e.react('face_seen')],
    ['sleepy', 640, (e) => e.setEmotion('sleepy')]
];
const ANIMATION_SEED = 1234;

/** Every reference image: { name, label, size, rgb }; the animation is one image, its frames one under the other. */
export async function renders(modulePath = MODULE) {
    const engine = await loadEngine(pathToFileURL(resolve(modulePath)).href);
    const swatches = new Map(JSON.parse(readFileSync(LOOKS, 'utf8')).swatches.map((s) => [s.id, s]));
    const out = [];
    for (const scene of JSON.parse(readFileSync(SCENES, 'utf8')).scenes) {
        const pixels = engine.renderScene(scene, makeLook(swatches.get(scene.swatch), scene.pupil));
        out.push({ name: `scene-${scene.id}`, label: `scene ${scene.id}`, height: engine.size, rgb: rgbOf(pixels) });
    }
    engine.init(ANIMATION_SEED);
    engine.setLook(makeLook(swatches.get('cherry'), 'dot'));
    const frames = [];
    for (const [, steps, start] of ANIMATION) {
        start(engine);
        for (let i = 0; i < steps; i++) engine.update(STEP);
        frames.push(rgbOf(engine.renderView()));
    }
    const labels = ANIMATION.map(([label], i) => `frame ${i + 1}: ${label}`).join('; ');
    out.push({
        name: 'animation',
        label: `animation (${labels})`,
        height: engine.size * frames.length,
        rgb: Buffer.concat(frames)
    });
    return { size: engine.size, images: out };
}

function main() {
    const check = process.argv.includes('--check');
    return renders().then(({ size, images }) => {
        const problems = [];
        let rewritten = 0;
        const wanted = new Set(images.map((i) => `${i.name}.png`));
        mkdirSync(DIR, { recursive: true });
        for (const image of images) {
            const path = `${DIR}/${image.name}.png`;
            let kept = null;
            try {
                kept = decodePng(readFileSync(path));
            } catch {
                kept = null; // missing or unreadable: written, or reported
            }
            const same = kept && kept.width === size && kept.height === image.height && kept.rgb.equals(image.rgb);
            if (same) {
                if (check) rmSync(`${FAILED}/${image.name}.png`, { force: true }); // an image left by an earlier failure
                continue;
            }
            const png = encodePng(size, image.height, image.rgb);
            if (check) {
                mkdirSync(FAILED, { recursive: true });
                writeFileSync(`${FAILED}/${image.name}.png`, png);
                const what =
                    kept && kept.height === image.height
                        ? `${count(differingPixels(kept.rgb, image.rgb))}`
                        : 'no matching reference';
                problems.push(`${path}: ${image.label}: ${what} (now: ${FAILED}/${image.name}.png)`);
            } else {
                writeFileSync(path, png);
                rewritten++;
            }
        }
        for (const file of readdirSync(DIR).filter((f) => f.endsWith('.png') && !wanted.has(f))) {
            if (check) problems.push(`${DIR}/${file}: no scene draws it any more`);
            else rmSync(`${DIR}/${file}`);
        }
        if (problems.length) {
            for (const p of problems) console.error(p);
            console.error(
                'Reference renders differ: if the eyes changed on purpose, run npm run goldens and look at the images'
            );
            process.exit(1);
        }
        console.log(
            `${DIR}: ${images.length} reference renders ${check ? 'match' : `up to date (${rewritten} rewritten)`}`
        );
    });
}

if (import.meta.main) await main();
