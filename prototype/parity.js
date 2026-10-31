// Parity rule (notes/architecture.md): how far the C engine's image may be from the prototype's. Shared by the
// reference scenes (reference.js) and the side-by-side live view (ui.js).
import { SIZE } from './eyes.js';

// Mean difference per channel (0-255), and share of pixels that differ by more than BIG. Anti-aliasing and RGB565
// rounding stay well below them.
export const PARITY = { maxMean: 2, big: 32, maxBigShare: 0.005 };

// Compares two RGBA images of SIZE × SIZE pixels
export function compare(a, b) {
    const n = SIZE * SIZE;
    let sum = 0;
    let big = 0;
    for (let p = 0; p < n; p++) {
        const d =
            (Math.abs(a[4 * p] - b[4 * p]) +
                Math.abs(a[4 * p + 1] - b[4 * p + 1]) +
                Math.abs(a[4 * p + 2] - b[4 * p + 2])) /
            3;
        sum += d;
        if (d > PARITY.big) big++;
    }
    const mean = sum / n;
    const bigShare = big / n;
    return { mean, bigShare, pass: mean <= PARITY.maxMean && bigShare <= PARITY.maxBigShare };
}
