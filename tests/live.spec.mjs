// Live parity: driven by the same inputs side by side on the interactive page, the prototype and the C engine keep
// rendering the same image while they animate (parity.spec.mjs only checks still reference scenes). The test steps
// both renderers by hand, from a fixed seed, so it does not depend on the browser's frame timing and a failure replays.
import { expect, test } from '@playwright/test';
import { collectErrors } from './page-errors.mjs';

const RUNS = [
    { seed: 1234, swatch: 'cherry', pupil: 'dot' },
    { seed: 99, swatch: 'popcorn', pupil: 'cat' },
    { seed: 2026, swatch: 'bumblebee', pupil: 'acorn' },
    { seed: 7, swatch: 'laser', pupil: 'circle' }
];

// Runs in the page: plays the same inputs for each run, comparing the two renderers every half second
function animate(runs) {
    const { live } = window;
    const out = [];
    for (const { seed, swatch, pupil } of runs) {
        live.pickLook(swatch, pupil);
        live.setEmotion('neutral');
        live.setPointer(null);
        live.setGravity(null);
        live.setShaking(false);
        live.resync(seed);
        const run = (label, steps) => {
            for (let done = 0; done < steps; done += 32) {
                live.step(Math.min(32, steps - done));
                out.push({ label: `${swatch} ${pupil}, seed ${seed}: ${label}, step ${done}`, ...live.diff() });
            }
        };
        run('idle', 320);
        live.setPointer({ x: 0.6, y: -0.3 });
        run('following the pointer', 128);
        live.setPointer(null);
        live.setGravity({ x: Math.sin(1.2), y: Math.cos(1.2) });
        run('object turned', 128);
        live.setGravity({ x: 0.1, y: -0.99 });
        run('upside down, angry', 128);
        live.setGravity(null);
        live.setShaking(true);
        run('shaken, dizzy', 96);
        live.setShaking(false);
        run('calming down', 64);
        live.setEmotion('love');
        run('love, with hearts', 320);
        live.react('tap');
        run('petting, with sparkles', 64);
        live.react('expression_angry');
        run('frowning at someone', 96);
        live.react('gesture_dislike');
        run('sad at a thumb down', 96);
        live.react('face_seen');
        run('surprised by a face', 64);
        live.setEmotion('squint');
        run('squinting', 64);
        live.setEmotion('sleepy');
        run('sleepy', 640);
    }
    return out;
}

async function openSideBySide(page) {
    await page.goto('/prototype/index.html');
    await page.waitForFunction(() => window.live !== undefined);
    await page.evaluate(async () => {
        window.live.pause();
        await window.live.setMode('side');
    });
}

test('the C engine animates like the prototype, side by side', async ({ page }) => {
    const errors = collectErrors(page);
    await openSideBySide(page);
    const results = await page.evaluate(animate, RUNS);

    expect(errors, 'console errors on the interactive page').toEqual([]);
    const failing = results
        .filter((r) => !r.pass)
        .map((r) => `${r.label}: mean ${r.mean.toFixed(2)}, ${r.core} core pixels`);
    expect(failing, 'moments where the two renderers differ').toEqual([]);
});

test('a seed replays the same animation', async ({ page }) => {
    await openSideBySide(page);
    const replay = () => page.evaluate(animate, RUNS.slice(0, 1));
    expect(await replay()).toEqual(await replay());
});
