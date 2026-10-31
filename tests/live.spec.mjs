// Live parity: driven by the same inputs side by side on the interactive page, the prototype and the C engine keep
// rendering the same image while they animate (parity.spec.mjs only checks still reference scenes). The test steps
// both renderers by hand, so it does not depend on the browser's frame timing.
import { expect, test } from '@playwright/test';

test('the C engine animates like the prototype, side by side', async ({ page }) => {
    const errors = [];
    page.on('pageerror', (err) => errors.push(err.message));
    page.on('console', (msg) => msg.type() === 'error' && errors.push(msg.text()));

    await page.goto('/prototype/index.html');
    await page.waitForFunction(() => window.live !== undefined);
    const results = await page.evaluate(async () => {
        const { live } = window;
        live.pause();
        await live.setMode('side');
        const out = [];
        const check = (label) => out.push({ label, ...live.diff() });
        live.step(320);
        check('idle, 5 s');
        live.setPointer({ x: 0.6, y: -0.3 });
        live.step(128);
        check('following the pointer');
        live.setPointer(null);
        live.setEmotion('love');
        live.step(320);
        check('love, with hearts');
        live.pet();
        live.step(64);
        check('petting, with sparkles');
        live.setEmotion('sleepy');
        live.step(640);
        check('sleepy, 10 s');
        return out;
    });

    expect(errors, 'console errors on the interactive page').toEqual([]);
    const failing = results
        .filter((r) => !r.pass)
        .map((r) => `${r.label}: mean ${r.mean.toFixed(2)}, ${(r.bigShare * 100).toFixed(3)}% of pixels off`);
    expect(failing, 'moments where the two renderers differ').toEqual([]);
});
