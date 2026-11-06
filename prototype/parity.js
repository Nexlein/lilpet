// Parity rule (notes/architecture.md): how far the C engine's image may be from the prototype's. Shared by the
// reference scenes (reference.js) and the side-by-side live view (ui.js).
import { drawEyes, setupCanvas } from './draw.js';
import { SIZE } from './eyes.js';

export const ENGINE_URL = new URL('../engine/build/wasm/lilpet.mjs', import.meta.url).href;

export async function fetchJson(url) {
    const res = await fetch(url);
    if (!res.ok) throw new Error(`${url}: HTTP ${res.status}`);
    return res.json();
}

// Mean difference per channel (0-255), and number of core pixels: pixels that differ by more than BIG, like their 4
// neighbours. Anti-aliased edges are one pixel wide, so they never make a core, however bright the look; a sparkle
// that is missing or half its size does.
export const PARITY = { maxMean: 1.5, big: 32, maxCore: 3 };

// Compares two RGBA images of SIZE × SIZE pixels
export function compare(a, b) {
    const n = SIZE * SIZE;
    const diff = new Float32Array(n);
    let sum = 0;
    for (let p = 0; p < n; p++) {
        diff[p] =
            (Math.abs(a[4 * p] - b[4 * p]) +
                Math.abs(a[4 * p + 1] - b[4 * p + 1]) +
                Math.abs(a[4 * p + 2] - b[4 * p + 2])) /
            3;
        sum += diff[p];
    }
    const big = (p) => diff[p] > PARITY.big;
    let core = 0;
    for (let y = 1; y < SIZE - 1; y++)
        for (let x = 1; x < SIZE - 1; x++) {
            const p = y * SIZE + x;
            if (big(p) && big(p - 1) && big(p + 1) && big(p - SIZE) && big(p + SIZE)) core++;
        }
    const mean = sum / n;
    return { mean, core, pass: mean <= PARITY.maxMean && core <= PARITY.maxCore };
}

let flatCtx = null;

// Compares the state of the prototype at time t with the pixels of the C engine, at screen resolution (a visible canvas
// follows the device pixel ratio)
export function compareFrame(eyes, t, pixels) {
    flatCtx ??= setupCanvas(document.createElement('canvas'), 1);
    drawEyes(flatCtx, eyes, t);
    return compare(flatCtx.getImageData(0, 0, SIZE, SIZE).data, pixels);
}
