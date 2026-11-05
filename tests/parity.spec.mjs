// Parity rule (notes/architecture.md): the C engine must render every reference scene like the prototype.
// prototype/reference.html does the rendering and scoring; this test reads its results.
import { readFileSync } from 'node:fs';
import { expect, test } from '@playwright/test';
import { SIZE } from '../prototype/eyes.js';
import { PARITY, compare } from '../prototype/parity.js';
import { collectErrors } from './page-errors.mjs';

const scenes = JSON.parse(readFileSync('prototype/scenes.json', 'utf8')).scenes;

test('the C engine renders every reference scene like the prototype', async ({ page }) => {
    const errors = collectErrors(page);

    await page.goto('/prototype/reference.html');
    await page.waitForFunction(() => window.parityDone === true);
    const results = await page.evaluate(() => window.parity ?? []);

    expect(errors, 'console errors on the reference page').toEqual([]);
    await expect(page.locator('#serveHint')).toBeHidden();
    expect(results.length, 'scenes compared (is the engine built? mise run engine:wasm)').toBe(scenes.length);
    const failing = results
        .filter((r) => !r.pass)
        .map((r) => `${r.id}: mean ${r.mean.toFixed(2)}, ${r.core} core pixels`);
    expect(failing, 'scenes outside the tolerance').toEqual([]);
});

// --- The rule itself, on made-up images ---------------------------------------------------------------------------
const black = () => new Uint8ClampedArray(SIZE * SIZE * 4).fill(255).map((v, i) => (i % 4 === 3 ? v : 0));

// A white rectangle on black
function withBlock(x0, y0, width, height) {
    const image = black();
    for (let y = y0; y < y0 + height; y++)
        for (let x = x0; x < x0 + width; x++) image.fill(255, 4 * (y * SIZE + x), 4 * (y * SIZE + x) + 3);
    return image;
}

test('the same image passes', () => {
    expect(compare(black(), black())).toEqual({ mean: 0, core: 0, pass: true });
});

test('an edge drawn one or two pixels off has no core, however bright', () => {
    for (const width of [1, 2]) {
        const { core, pass } = compare(black(), withBlock(20, 100, width, 300));
        expect(core).toBe(0);
        expect(pass).toBe(true);
    }
});

test('a small spot under the limit passes, a bigger one fails', () => {
    const small = compare(black(), withBlock(100, 100, 3, 3));
    expect(small.core).toBe(1);
    expect(small.pass).toBe(true);

    const big = compare(black(), withBlock(100, 100, 5, 5));
    expect(big.core).toBe(9);
    expect(big.core).toBeGreaterThan(PARITY.maxCore);
    expect(big.pass).toBe(false);
});

test('the limit is on core pixels, whatever their share of the image', () => {
    const { core, pass, mean } = compare(black(), withBlock(100, 100, 3, 6)); // 18 pixels, 4 of them core
    expect(core).toBe(PARITY.maxCore + 1);
    expect(mean).toBeLessThan(PARITY.maxMean);
    expect(pass).toBe(false);
});

test('a difference on the rim of the image has neighbours missing, not wrapped around', () => {
    const { core } = compare(black(), withBlock(0, 0, SIZE, 1));
    expect(core).toBe(0);
});

test('a faint difference over the whole image fails on the mean', () => {
    const grey = black().map((v, i) => (i % 4 === 3 ? v : 20));
    const { core, mean, pass } = compare(black(), grey);
    expect(core).toBe(0);
    expect(mean).toBe(20);
    expect(pass).toBe(false);
});
