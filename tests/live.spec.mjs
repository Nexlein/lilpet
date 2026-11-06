// Live parity: driven by the same inputs side by side on the interactive page, the prototype and the C engine keep
// rendering the same image while they animate (parity.spec.mjs only checks still reference scenes). The test steps
// both renderers by hand, from a fixed seed, so it does not depend on the browser's frame timing and a failure replays.
// The rest of the file covers the page itself: what resynchronizing keeps, and what it says when a part cannot load.
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
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
        .map((r) => `${r.label}: mean ${r.mean.toFixed(2)}, ${r.big} big pixels, ${r.core} of them core`);
    expect(failing, 'moments where the two renderers differ').toEqual([]);
});

test('a seed replays the same animation', async ({ page }) => {
    await openSideBySide(page);
    const replay = () => page.evaluate(animate, RUNS.slice(0, 1));
    expect(await replay()).toEqual(await replay());
});

// Each setting away from its default, in the way the page sets it. A pointer shows what follow and head change.
const point = (page) => page.evaluate(() => window.live.setPointer({ x: 0.6, y: -0.3 }));
const SETTINGS = [
    ['an emotion', (page) => page.evaluate(() => window.live.setEmotion('love'))],
    ['a pointer', point],
    ['a gravity', (page) => page.evaluate(() => window.live.setGravity({ x: Math.sin(1.2), y: Math.cos(1.2) }))],
    ['being shaken', (page) => page.evaluate(() => window.live.setShaking(true))],
    ['a shape', (page) => page.locator('#rx').fill('60')],
    ['a head amount', (page) => point(page).then(() => page.locator('#head').fill('0'))],
    ['the pointer not followed', (page) => point(page).then(() => page.locator('#follow').uncheck())]
];

// Runs in the page: what createEyes carries over from one prototype renderer to the next
function settings() {
    const { shape, headAmount, follow, mouse, gravity, shaking, mood } = window.eyes;
    return { shape, headAmount, follow, mouse, gravity, shaking, mood };
}

for (const [what, set] of SETTINGS)
    test(`resynchronizing keeps ${what}, in both renderers`, async ({ page }) => {
        await openSideBySide(page);
        await set(page);
        const before = await page.evaluate(settings);
        const result = await page.evaluate(() => {
            window.live.resync(1234);
            window.live.step(64);
            return window.live.diff();
        });

        expect(await page.evaluate(settings)).toEqual(before);
        expect(result.pass, `mean ${result.mean}, ${result.big} big pixels, ${result.core} core`).toBe(true);
    });

const modeOn = (page) => page.locator('#modes button.on');

test('the prototype keeps up while only the C engine shows', async ({ page }) => {
    await page.goto('/prototype/index.html');
    await page.waitForFunction(() => window.live !== undefined);
    await page.evaluate(async () => {
        window.live.pause();
        await window.live.setMode('c');
        window.live.react('tap');
    });
    const highlighted = page.locator('#emotions button.on');
    await expect(highlighted).toHaveAttribute('data-e', 'happy');

    await page.evaluate(() => window.live.step(128)); // 2 s, longer than the reaction
    await expect(highlighted).toHaveAttribute('data-e', 'neutral');
    expect(await page.evaluate(() => window.eyes.emotion)).toBe('neutral');
});

test('the C engine missing is said, and stays said', async ({ page }) => {
    await page.route('**/lilpet.mjs', (route) => route.abort());
    await page.goto('/prototype/index.html');
    await page.locator('#modes button', { hasText: 'Moteur C' }).click();

    await expect(page.locator('#engineInfo')).toContainText('Moteur C absent');
    await expect(modeOn(page)).toHaveText('Prototype JS');
    await page.waitForTimeout(100); // a few frames: nothing clears it
    await expect(page.locator('#engineInfo')).toContainText('Moteur C absent');
});

test('the C engine failing after another mode was clicked is said', async ({ page }) => {
    let release;
    const released = new Promise((done) => (release = done));
    await page.route('**/lilpet.mjs', async (route) => {
        await released;
        await route.abort();
    });
    await page.goto('/prototype/index.html');
    await page.locator('#modes button', { hasText: 'Moteur C' }).click();
    await page.locator('#modes button', { hasText: 'Prototype JS' }).click();
    release();

    await expect(page.locator('#engineInfo')).toContainText('Moteur C absent');
    await expect(modeOn(page)).toHaveText('Prototype JS');
});

test('the last mode clicked wins over one waiting for the engine', async ({ page }) => {
    let release;
    const released = new Promise((done) => (release = done));
    await page.route('**/lilpet.mjs', async (route) => {
        await released;
        await route.continue();
    });
    await page.goto('/prototype/index.html');
    await page.locator('#modes button', { hasText: 'Moteur C' }).click();
    await page.locator('#modes button', { hasText: 'Prototype JS' }).click();
    const loaded = page.waitForResponse('**/lilpet.wasm');
    release();
    await loaded;

    await page.waitForTimeout(100); // the engine is up: the first click would act now
    await expect(modeOn(page)).toHaveText('Prototype JS');
    await expect(page.locator('#cView')).toBeHidden();
});

const swatch = { id: 'stale', sclera: '#000000', pupilColor: ['#000000', '#000000'] };
const BAD_LOOKS = [
    ['a missing file', { status: 404, body: 'gone' }],
    ['an error status, whatever the body', { status: 500, json: { swatches: [swatch] } }],
    ['a file with no swatches', { json: {} }]
];
for (const [what, reply] of BAD_LOOKS)
    test(`colors that cannot load are said: ${what}`, async ({ page }) => {
        await page.route('**/eyes/looks.json', (route) => route.fulfill(reply));
        await page.goto('/prototype/index.html');

        await expect(page.locator('#engineInfo')).toContainText('Couleurs absentes');
        await expect(page.locator('#swatchSelect option')).toHaveCount(1);
    });

test('an emotion label is text, not HTML', async ({ page }) => {
    await page.route('**/eyes/emotions.json', async (route) => {
        const json = await (await route.fetch()).json();
        json.emotions.sad.label = '<b>triste</b>';
        await route.fulfill({ json });
    });
    await page.goto('/prototype/index.html');

    const button = page.locator('#emotions button[data-e="sad"]');
    await expect(button).toContainText('<b>triste</b>');
    await expect(button.locator('kbd')).toHaveText('7');
});

for (const file of ['index.html', 'reference.html'])
    test(`${file} opened as a file says it needs a server`, async ({ page }) => {
        await page.goto(pathToFileURL(resolve('prototype', file)).href);
        await expect(page.locator('#serveHint')).toBeVisible();
        await expect(page.locator('#serveHint')).toContainText('npm run serve');
    });

for (const file of ['index.html', 'reference.html'])
    test(`${file} with a broken emotions.json points to the console`, async ({ page }) => {
        await page.route('**/eyes/emotions.json', (route) =>
            route.fulfill({ contentType: 'application/json', body: '{,}' })
        );
        await page.goto(`/prototype/${file}`);
        await expect(page.locator('#serveHint')).toBeVisible();
        await expect(page.locator('#serveHint')).toContainText('voir la console');
    });

test('the hint is gone once the page runs', async ({ page }) => {
    await page.goto('/prototype/index.html');
    await page.waitForFunction(() => window.live !== undefined);
    await expect(page.locator('#serveHint')).toBeHidden();
});
