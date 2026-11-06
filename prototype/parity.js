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

// Mean difference per channel (0-255), and two counts of big pixels: pixels with a channel further than `big`, so a
// wrong color shows even if the other channels are right. Core pixels are big ones, like their 4 neighbours:
// anti-aliased edges are one pixel wide, so they never make a core, however bright the look; a sparkle that is missing
// or half its size does. A line 2 pixels wide makes no core, but it adds to all the big pixels, edges included: at most
// 609 over every look and scene, 515 while animating, so maxBig leaves a margin for small changes of the shapes.
export const PARITY = { maxMean: 1.5, big: 32, maxCore: 3, maxBig: 760 };

const far = new Uint8Array(SIZE * SIZE); // 1 for a big pixel

// Compares two RGBA images of SIZE × SIZE pixels
export function compare(a, b) {
    const n = SIZE * SIZE;
    far.fill(0);
    let sum = 0;
    let big = 0;
    for (let p = 0; p < n; p++) {
        const red = Math.abs(a[4 * p] - b[4 * p]);
        const green = Math.abs(a[4 * p + 1] - b[4 * p + 1]);
        const blue = Math.abs(a[4 * p + 2] - b[4 * p + 2]);
        sum += (red + green + blue) / 3;
        if (Math.max(red, green, blue) > PARITY.big) {
            far[p] = 1;
            big++;
        }
    }
    let core = 0;
    for (let y = 1; y < SIZE - 1; y++)
        for (let x = 1; x < SIZE - 1; x++) {
            const p = y * SIZE + x;
            if (far[p] && far[p - 1] && far[p + 1] && far[p - SIZE] && far[p + SIZE]) core++;
        }
    const mean = sum / n;
    return { mean, core, big, pass: mean <= PARITY.maxMean && core <= PARITY.maxCore && big <= PARITY.maxBig };
}

let flatCtx = null;

// Compares the state of the prototype at time t with the pixels of the C engine, at screen resolution (a visible canvas
// follows the device pixel ratio)
export function compareFrame(eyes, t, pixels) {
    flatCtx ??= setupCanvas(document.createElement('canvas'), 1);
    drawEyes(flatCtx, eyes, t);
    return compare(flatCtx.getImageData(0, 0, SIZE, SIZE).data, pixels);
}
