// Parity rule (notes/architecture.md): the C engine must render every reference scene like the prototype.
// prototype/reference.html does the rendering and scoring; this test reads its results.
import { readFileSync } from 'node:fs';
import { expect, test } from '@playwright/test';

const scenes = JSON.parse(readFileSync('prototype/scenes.json', 'utf8')).scenes;

test('the C engine renders every reference scene like the prototype', async ({ page }) => {
    const errors = [];
    page.on('pageerror', (err) => errors.push(err.message));
    page.on('console', (msg) => msg.type() === 'error' && errors.push(msg.text()));

    await page.goto('/prototype/reference.html');
    await page.waitForFunction(() => window.parityDone === true);
    const results = await page.evaluate(() => window.parity ?? []);

    expect(errors, 'console errors on the reference page').toEqual([]);
    expect(results.length, 'scenes compared (is the engine built? mise run engine:wasm)').toBe(scenes.length);
    const failing = results
        .filter((r) => !r.pass)
        .map((r) => `${r.id}: mean ${r.mean.toFixed(2)}, ${(r.bigShare * 100).toFixed(3)}% of pixels off`);
    expect(failing, 'scenes outside the tolerance').toEqual([]);
});
