// The built PC app (npm run app:build) opens, runs the C engine, picks looks and talks to a board over a fake serial
// port. npm run test:parity builds it first. Run alone, these tests are skipped until the app is built, and fail in CI
// if the build is missing.
import { existsSync } from 'node:fs';
import { expect, test as base } from '@playwright/test';
import { fakePort } from '../app/src/device/fake-port.ts';
import { collectErrors } from './page-errors.mjs';

const APP = '/app/dist/index.html';

// Every test ends with no error in the page
const test = base.extend({
    errors: [
        async ({ page }, use) => {
            const errors = collectErrors(page);
            await use(errors);
            expect(errors, 'console errors in the PC app').toEqual([]);
        },
        { auto: true }
    ]
});

test.skip(!existsSync('app/dist/index.html') && !process.env.CI, 'build the app first: npm run app:build');

// Runs in the page, on the fake port of the Vitest tests: a board on navigator.serial, driven through window.board
function plugBoard(makePort) {
    const board = {
        hello: 'lilpet fw=0.5.0 engine=0.13.0 board=atoms3r camera=0 motion=1',
        replies: { look: 'ok', emotion: 'ok', react: 'ok' }, // null: the board stays silent
        delay: 0
    };
    const device = makePort(
        (command) => {
            const word = command.split(' ')[1];
            return word === 'hello' ? board.hello : board.replies[word];
        },
        { delay: () => board.delay }
    );
    window.board = Object.assign(board, {
        sent: device.sent,
        log: device.log,
        unplug: () => device.readError({ fatal: true })
    });
    Object.defineProperty(navigator, 'serial', { configurable: true, value: { requestPort: async () => device.port } });
}

const fakeSerial = `(${plugBoard})(${fakePort})`;

// Runs in the page: counts the frames drawn on the canvas
function countFrames() {
    const { putImageData } = CanvasRenderingContext2D.prototype;
    window.drawn = 0;
    CanvasRenderingContext2D.prototype.putImageData = function draw(...args) {
        window.drawn++;
        putImageData.apply(this, args);
    };
}

// Runs in the page: what the screen shows, frame by frame. Eyes that blink are shut for less than 0.7 s, and a blink is
// over too soon to be seen from outside the page.
function watchScreen() {
    window.readScreen = () => {
        const canvas = document.querySelector('canvas.screen');
        const { data } = canvas.getContext('2d').getImageData(0, 0, canvas.width, canvas.height);
        const colors = new Map();
        let lit = 0;
        let rows = 0;
        for (let i = 0; i < data.length; i += 4) {
            const color = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
            if (color === 0) continue;
            lit++;
            rows += Math.floor(i / 4 / canvas.width);
            colors.set(color, (colors.get(color) ?? 0) + 1);
        }
        const [top] = [...colors].toSorted((a, b) => b[1] - a[1])[0] ?? [0];
        return {
            lit: lit / (canvas.width * canvas.height), // share of lit pixels
            height: rows / lit / canvas.height, // of the middle of the eyes: 0 at the top of the screen, 1 at the bottom
            color: [top >> 16, (top >> 8) & 255, top & 255] // the most common one
        };
    };

    window.watch = async (ms) => {
        const seen = [];
        const end = performance.now() + ms;
        while (performance.now() < end) {
            seen.push(window.readScreen());
            // oxlint-disable-next-line no-await-in-loop -- one frame after the other
            await new Promise(requestAnimationFrame);
        }
        return seen;
    };

    // From the first lit frame, with no wait for the test to get here: clicks on Cligner or not, then watches
    window.watchStart = async (click, ms) => {
        while (!document.querySelector('canvas.screen') || window.readScreen().lit === 0)
            // oxlint-disable-next-line no-await-in-loop -- one frame after the other
            await new Promise(requestAnimationFrame);
        if (click) [...document.querySelectorAll('button')].find((b) => b.textContent === 'Cligner').click();
        return window.watch(ms);
    };
}

// The engine takes its seed from Date.now(). Every start has a first blink at 1.5 s (next_blink in engine/src/eyes.c);
// with this seed none comes earlier, while others hide the first saccade behind a blink, as soon as the first frames.
const QUIET_SEED = 1;

const frames = (page) => page.evaluate(() => window.drawn);
const sent = (page) => page.evaluate(() => window.board.sent);
const looks = async (page) => (await sent(page)).filter((line) => line.startsWith('> look'));
const note = (page) => page.locator('.send-note');
const chip = (page, name) => page.locator('.grid .chip', { hasText: name });
const place = (locator) => locator.evaluate((el) => ({ x: el.offsetLeft, y: el.offsetTop }));
const picture = (page) => page.locator('canvas.screen').evaluate((canvas) => canvas.toDataURL());

// The screen with the eyes open: the frame with the most lit pixels of the time a blink takes
async function openEyes(page) {
    const seen = await page.locator('canvas.screen').evaluate(() => window.watch(700));
    return seen.reduce((best, frame) => (frame.lit > best.lit ? frame : best));
}

// Where the eyes come to rest: they ease toward their target, so two readings in a row must agree
async function settledHeight(page) {
    let before = Number.NaN;
    let height = Number.NaN;
    await expect
        .poll(
            async () => {
                before = height;
                ({ height } = await openEyes(page));
                return Math.abs(height - before);
            },
            { timeout: 15_000 }
        )
        .toBeLessThan(0.002);
    return height;
}

// The engine draws in RGB565 and shades the eyes: near the color of the swatch, not exactly it
const wears = async (page, [r, g, b]) => {
    const { color } = await openEyes(page);
    return Math.max(Math.abs(color[0] - r), Math.abs(color[1] - g), Math.abs(color[2] - b)) <= 8;
};

async function plugIn(page) {
    await page.getByRole('button', { name: 'Brancher lilpet' }).click();
    await expect(page.locator('.device [role=status]')).toContainText('lilpet est branché');
}

test('the PC app shows live eyes and picks looks', async ({ page }) => {
    await page.goto(APP);
    await expect(page.locator('.grid .chip')).toHaveCount(100);
    await expect(page.locator('canvas.screen')).toBeVisible();

    // The engine draws: some pixels of the screen are lit
    await expect
        .poll(() =>
            page.locator('canvas.screen').evaluate((canvas) => {
                const data = canvas.getContext('2d').getImageData(0, 0, canvas.width, canvas.height).data;
                let lit = 0;
                for (let i = 0; i < data.length; i += 4) if (data[i] + data[i + 1] + data[i + 2] > 0) lit++;
                return lit / (canvas.width * canvas.height);
            })
        )
        .toBeGreaterThan(0.05);

    // Every engine emotion has its French name, and the engine takes what the buttons send (its types are hand-written)
    await expect(page.locator('.moods button[aria-pressed]')).toHaveText([
        'Neutre',
        'Content',
        'Amoureux',
        'Fâché',
        'Plissé',
        'Renfrogné',
        'Triste',
        'Surpris',
        'Endormi',
        'Étourdi'
    ]);
    await page.getByRole('button', { name: 'Étourdi' }).click();
    await expect(page.getByRole('button', { name: 'Étourdi' })).toHaveAttribute('aria-pressed', 'true');
    await page.getByRole('button', { name: 'Cligner' }).click();
    await page.locator('canvas.screen').click();

    await chip(page, 'socks').click();
    await expect(page.locator('.look-name h1')).toHaveText('socks');
    await page.locator('.pupil', { hasText: 'Gland' }).click(); // the label: the radio itself is hidden
    await page.locator('.favorite').click();
    await expect(page.locator('.favorites .chip')).toHaveText(['socks gland']);

    // The choice survives a reload
    await page.reload();
    await expect(page.locator('.look-name h1')).toHaveText('socks');
    await expect(page.getByLabel('Gland')).toBeChecked();
});

test('Cligner shuts the eyes, which stay open at first without it', async ({ page }) => {
    await page.addInitScript(watchScreen);
    await page.addInitScript((seed) => (Date.now = () => seed), QUIET_SEED);
    const litAtStart = async (click) => {
        await page.goto(APP);
        return (await page.evaluate((press) => window.watchStart(press, 450), click)).map((frame) => frame.lit);
    };

    const alone = await litAtStart(false);
    expect(Math.min(...alone), 'no blink of their own').toBeGreaterThan(Math.max(...alone) * 0.8);
    const clicked = await litAtStart(true);
    expect(Math.min(...clicked), 'the eyes shut').toBeLessThan(Math.max(...clicked) / 2);
});

test('the live eyes wear the chosen look, show the mood, and answer a pet', async ({ page }) => {
    await page.addInitScript(watchScreen);
    await page.goto(APP);
    await expect.poll(() => wears(page, [0xae, 0x00, 0x02])).toBe(true);
    await chip(page, 'socks').click();
    await expect.poll(() => wears(page, [0xb8, 0x65, 0xa8])).toBe(true);

    const open = (await openEyes(page)).lit;
    const lit = async () => (await openEyes(page)).lit;
    await page.getByRole('button', { name: 'Endormi' }).click();
    await expect.poll(lit).toBeLessThan(open * 0.6);
    const sleepy = await lit();
    await page.getByRole('button', { name: 'Caresser' }).click();
    await expect.poll(lit, 'happy for a moment').toBeGreaterThan(sleepy * 1.3);
});

test('the live eyes follow the pointer, and let go when it leaves the page', async ({ page }) => {
    await page.addInitScript(watchScreen);
    await page.goto(APP);
    const screen = await page.locator('canvas.screen').boundingBox();
    const middle = screen.x + screen.width / 2;

    await page.mouse.move(middle, screen.y);
    const up = await settledHeight(page);
    await page.mouse.move(middle, screen.y + screen.height);
    const down = await settledHeight(page);
    // The pointer takes them across 0.13 of the screen. On their own they cover 0.083 at most: saccades reach 0.63 of it
    expect(down - up).toBeGreaterThan(0.1);

    await page.evaluate(() => document.documentElement.dispatchEvent(new PointerEvent('pointerleave')));
    await expect.poll(async () => (await openEyes(page)).height).toBeLessThan(up + (down - up) * 0.75);
});

test('plugs in by keyboard, sends a look, and claims nothing after an unplug', async ({ page }) => {
    await page.addInitScript(fakeSerial);
    await page.goto(APP);
    await page.evaluate(() => (window.board.delay = 200));

    const button = page.locator('.device button');
    const status = page.locator('.device [role=status]');
    await button.focus();
    await page.keyboard.press('Enter');
    await expect(status).toHaveText('Connexion à lilpet…');
    await expect(button).toHaveAttribute('aria-disabled', 'true');
    await expect(button).toBeFocused();
    await expect(status).toContainText('lilpet est branché');
    await expect(status).toContainText('atoms3r, firmware 0.5.0, moteur 0.13.0');
    await expect(button).toHaveText('Débrancher');
    await expect(button).toBeFocused();

    await page.getByRole('button', { name: 'Envoyer à lilpet' }).click();
    await expect(note(page)).toHaveText('cherry est sur lilpet, et y reste même éteint.');
    expect(await sent(page)).toContain('> look AE0002 FF5252 FF5252 dot');
    await chip(page, 'socks').click();
    await expect(note(page)).toBeEmpty();
    await chip(page, 'cherry').click();
    await expect(note(page)).toContainText('cherry est sur lilpet');

    await page.evaluate(() => window.board.unplug());
    await expect(button).toHaveText('Brancher lilpet');
    await expect(note(page)).toHaveText('Branche lilpet en USB pour lui envoyer ce look.');

    // Plugged in again, here a board with older firmware: nothing was sent to it
    await page.evaluate(() => (window.board.hello = 'lilpet fw=0.3.0 engine=0.10.0'));
    await plugIn(page);
    await expect(status).toContainText('firmware 0.3.0, moteur 0.10.0');
    await expect(status).not.toContainText('undefined');
    await expect(note(page)).toBeEmpty();

    await page.getByRole('button', { name: 'Débrancher' }).click();
    await expect(button).toHaveText('Brancher lilpet');
});

test('a second press while connecting changes nothing, and a mood picked meanwhile reaches the board', async ({
    page
}) => {
    await page.addInitScript(fakeSerial);
    await page.goto(APP);
    await page.evaluate(() => (window.board.hello = null)); // connecting lasts until the board answers

    const status = page.locator('.device [role=status]');
    await page.locator('.device button').focus();
    await page.keyboard.press('Enter');
    await expect(status).toHaveText('Connexion à lilpet…');
    await expect(note(page), 'no need to ask for what is under way').toBeEmpty();
    await page.keyboard.press('Enter');
    await page.getByRole('button', { name: 'Triste' }).click();
    await expect(status).toHaveText('Connexion à lilpet…');
    await expect(page.getByRole('alert')).toHaveCount(0);

    await page.evaluate(() => (window.board.hello = 'lilpet fw=0.5.0 engine=0.13.0'));
    await expect(status).toContainText('lilpet est branché');
    await expect.poll(async () => (await sent(page)).at(-1)).toBe('> emotion sad');
});

test('says when the flash refuses a look, and when the board does', async ({ page }) => {
    await page.addInitScript(fakeSerial);
    await page.goto(APP);
    await plugIn(page);
    const send = page.getByRole('button', { name: 'Envoyer à lilpet' });

    await page.evaluate(() => (window.board.replies.look = 'err shown, but cannot save the look in flash'));
    await send.click();
    await expect(note(page)).toHaveText('cherry est sur lilpet, mais ne sera pas gardé.');

    await page.evaluate(() => (window.board.replies.look = 'err malformed look'));
    await send.click();
    await expect(note(page)).toHaveText('cherry n’est pas envoyé : Erreur de lilpet : malformed look');
});

test('sends one look at a time and keeps a failure until the next send', async ({ page }) => {
    await page.addInitScript(fakeSerial);
    await page.goto(APP);
    await plugIn(page);

    const send = page.locator('.primary');
    await send.click();
    await expect(note(page)).toContainText('cherry est sur lilpet');

    await page.evaluate(() => (window.board.replies.look = null));
    await send.click();
    await expect(send).toHaveText('Envoi…');
    await expect(note(page), 'the last result is over').toBeEmpty();
    await chip(page, 'socks').click(); // another look, while the board keeps quiet
    await expect(send).toBeDisabled();
    await expect(send).toHaveText('Envoi…');

    const failure = 'cherry n’est pas envoyé : lilpet ne répond pas. Débranche-le, rebranche-le, puis réessaie.';
    await expect(note(page)).toHaveText(failure);
    await expect(send).toBeEnabled();
    await chip(page, 'kite').click();
    await expect(note(page)).toHaveText(failure);

    await page.evaluate(() => (window.board.replies.look = 'ok'));
    await send.click();
    await expect(note(page)).toHaveText('kite est sur lilpet, et y reste même éteint.');
});

test('the send button keeps the keyboard focus and only sends once connected, one at a time', async ({ page }) => {
    await page.addInitScript(fakeSerial);
    await page.goto(APP);
    const send = page.locator('.primary');
    await send.focus();
    await page.keyboard.press('Enter');
    await expect(note(page)).toHaveText('Branche lilpet en USB pour lui envoyer ce look.');

    await plugIn(page);
    await page.evaluate(() => (window.board.delay = 300));
    await send.focus();
    await page.keyboard.press('Enter');
    await expect(send).toHaveText('Envoi…');
    await expect(send).toBeFocused();
    await page.keyboard.press('Enter'); // ignored: one send is under way
    await expect(note(page)).toHaveText('cherry est sur lilpet, et y reste même éteint.');
    await expect(send).toBeFocused();
    expect(await looks(page)).toHaveLength(1);

    await page.keyboard.press('Enter');
    await expect.poll(() => looks(page)).toHaveLength(2);
});

test('a send that an unplug cuts short keeps its failure, until lilpet is back', async ({ page }) => {
    await page.addInitScript(fakeSerial);
    await page.goto(APP);
    await plugIn(page);
    await page.evaluate(() => (window.board.replies.look = null));
    await page.locator('.primary').click();
    await page.evaluate(() => window.board.unplug());
    await expect(note(page)).toHaveText('cherry n’est pas envoyé : lilpet n’est plus branché.');
    await plugIn(page);
    await expect(note(page)).toBeEmpty();
});

test('explains an unplug during hello in French, and can plug in again', async ({ page }) => {
    await page.addInitScript(fakeSerial);
    await page.goto(APP);
    await page.evaluate(() => (window.board.hello = null));
    await page.getByRole('button', { name: 'Brancher lilpet' }).click();
    await expect.poll(() => sent(page)).toEqual(['> hello']);
    await page.evaluate(() => window.board.unplug());
    await expect(page.getByRole('alert')).toHaveText('lilpet n’est plus branché.');
    await page.evaluate(() => (window.board.hello = 'lilpet fw=0.5.0 engine=0.13.0'));
    await plugIn(page);
    await expect(page.getByRole('alert')).toHaveCount(0);
});

test('moods and pets reach the board, and a failure only goes to the log', async ({ page }) => {
    await page.addInitScript(fakeSerial);
    await page.goto(APP);

    await page.getByRole('button', { name: 'Triste' }).click();
    await plugIn(page);
    await expect.poll(() => sent(page), 'the mood on screen reaches the board').toEqual(['> hello', '> emotion sad']);

    await page.getByRole('button', { name: 'Content' }).click();
    await page.locator('canvas.screen').click();
    await page.getByRole('button', { name: 'Caresser' }).press('Enter');
    await expect
        .poll(() => sent(page))
        .toEqual(['> hello', '> emotion sad', '> emotion happy', '> react tap', '> react tap']);

    await page.evaluate(() => (window.board.replies.emotion = 'err unknown emotion'));
    await page.getByRole('button', { name: 'Surpris' }).click();
    await page.locator('.log summary').click();
    await expect(page.locator('.log pre')).toContainText('> emotion surprised : Erreur de lilpet : unknown emotion');
    await expect(page.getByRole('alert')).toHaveCount(0);
    await expect(note(page)).toBeEmpty();
});

test('the log follows the last line, unless it was scrolled up', async ({ page }) => {
    await page.addInitScript(fakeSerial);
    await page.goto(APP);
    await plugIn(page);

    const log = page.locator('.log pre');
    const write = (from, to) =>
        page.evaluate(
            ([first, last]) => {
                for (let i = first; i < last; i++) window.board.log(`line ${i}`);
            },
            [from, to]
        );
    const fromEnd = () => log.evaluate((el) => el.scrollHeight - el.scrollTop - el.clientHeight);

    await write(0, 50); // while the panel is closed
    await page.locator('.log summary').click();
    await expect(log).toContainText('line 49');
    await expect.poll(fromEnd).toBeLessThan(2);

    await write(50, 60);
    await expect(log).toContainText('line 59');
    await expect.poll(fromEnd).toBeLessThan(2);

    await log.evaluate(
        (el) =>
            new Promise((scrolled) => {
                el.addEventListener('scrollend', scrolled, { once: true });
                el.scrollTop = 0;
            })
    );
    await write(60, 70);
    await expect(log).toContainText('line 69');
    expect(await log.evaluate((el) => el.scrollTop)).toBe(0);
});

test('the color grid is one stop of the Tab key, walked with the arrows', async ({ page }) => {
    await page.goto(APP);
    const chips = page.locator('.grid .chip');
    await expect(chips).toHaveCount(100);
    const stop = page.locator('.grid .chip[tabindex="0"]');
    await expect(stop).toHaveCount(1);
    await expect(stop).toContainText('cherry');

    const names = await chips.allInnerTexts();
    const next = names[names.indexOf('cherry') + 1];
    const focused = page.locator('.grid .chip:focus');
    await stop.focus();
    const first = await place(focused);
    await page.keyboard.press('ArrowRight');
    await expect(focused).toHaveText(next);
    await page.keyboard.press('ArrowLeft');
    await expect(focused).toHaveText('cherry');
    await page.keyboard.press('ArrowDown');
    const below = await place(focused);
    expect(below.x).toBe(first.x);
    expect(below.y).toBeGreaterThan(first.y);
    await page.keyboard.press('ArrowUp');
    expect((await place(focused)).y).toBe(first.y);

    await page.keyboard.press('ArrowRight');
    await page.keyboard.press('Enter');
    await expect(stop).toHaveAttribute('aria-current', 'true');
    await page.keyboard.press('Tab');
    await expect(focused).toHaveCount(0); // out of the grid at once

    // Coming back, Tab finds the color last walked to, not the chosen one
    await stop.focus();
    await page.keyboard.press('ArrowRight');
    const walked = await focused.innerText();
    await page.keyboard.press('Tab');
    await expect(focused).toHaveCount(0);
    await page.keyboard.press('Shift+Tab');
    await expect(focused).toHaveText(walked);

    // A favorite picked meanwhile takes the stop
    await page.locator('.favorite').click();
    await page.locator('.favorites .chip').click();
    await expect(stop).toHaveAttribute('aria-current', 'true');

    // With the chosen color filtered out, the first one left takes the stop
    await page.getByLabel('Chercher une couleur par son nom').fill('socks');
    await expect(stop).toContainText('socks');
});

test('the arrow keys do not scroll the page as well', async ({ page }) => {
    await page.setViewportSize({ width: 375, height: 700 });
    await page.goto(APP);
    await page.locator('.grid .chip').first().focus();
    const scrolled = await page.evaluate(() => window.scrollY);
    expect(scrolled).toBeGreaterThan(0);
    await page.keyboard.press('ArrowUp'); // no color above the first one
    await page.waitForTimeout(200);
    expect(await page.evaluate(() => window.scrollY)).toBe(scrolled);
});

test('the interface takes the color of the eyes, in an ink that reads on it', async ({ page }) => {
    await page.goto(APP);
    const mood = page.locator('.moods button[aria-pressed=true]');
    await expect(mood).toHaveCSS('background-color', 'rgb(174, 0, 2)');
    await expect(mood).toHaveCSS('color', 'rgb(255, 255, 255)');
    await chip(page, 'kite').click();
    await expect(mood).toHaveCSS('background-color', 'rgb(125, 181, 244)');
    await expect(mood).toHaveCSS('color', 'rgb(0, 0, 0)');
});

test('says when no color matches, in a status that is there before it speaks', async ({ page }) => {
    await page.goto(APP);
    const empty = page.locator('.empty[role=status]');
    await expect(empty).toBeEmpty();
    await page.getByLabel('Chercher une couleur par son nom').fill('no such color');
    await expect(empty).toHaveText('Aucune couleur ne correspond. Retire un filtre ou change ta recherche.');
    await expect(page.locator('.grid')).toHaveCount(0);
    await page.getByLabel('Chercher une couleur par son nom').fill('');
    await expect(empty).toBeEmpty();
    await expect(page.locator('.grid .chip')).toHaveCount(100);
});

test('with less motion asked, the eyes are a still frame that follows the choices', async ({ page }) => {
    await page.addInitScript(countFrames);
    await page.emulateMedia({ reducedMotion: 'reduce' });
    await page.goto(APP);
    await expect.poll(() => frames(page)).toBe(1);
    await page.waitForTimeout(200);
    expect(await frames(page)).toBe(1);
    await expect(page.getByRole('button', { name: 'Cligner' })).toHaveCount(0);
    const neutral = await picture(page);

    await page.getByRole('button', { name: 'Triste' }).click();
    await expect.poll(() => frames(page)).toBe(2);
    const sad = await picture(page);
    expect(sad).not.toBe(neutral);
    await chip(page, 'socks').click();
    await expect.poll(() => frames(page)).toBe(3);
    const socks = await picture(page);
    expect(socks).not.toBe(sad);
    await page.getByRole('button', { name: 'Caresser' }).click();
    await page.waitForTimeout(100);
    expect(await frames(page)).toBe(3);
    expect(await picture(page)).toBe(socks);

    await page.emulateMedia({ reducedMotion: 'no-preference' });
    await expect.poll(() => frames(page)).toBeGreaterThan(10);
    await expect(page.getByRole('button', { name: 'Cligner' })).toBeVisible();
});

test('on a short window, the send note is in reach', async ({ page }) => {
    await page.setViewportSize({ width: 1280, height: 560 });
    await page.goto(APP);
    const bottom = await note(page).evaluate((el) => el.getBoundingClientRect().bottom);
    await page.evaluate((y) => window.scrollTo(0, y - window.innerHeight), bottom);
    await expect(note(page)).toBeInViewport();
});

test('the animation pauses while the screen is out of sight', async ({ page }) => {
    await page.addInitScript(countFrames);
    await page.setViewportSize({ width: 375, height: 700 });
    await page.goto(APP);
    await expect.poll(() => frames(page)).toBeGreaterThan(3);

    await page.locator('.grid .chip').last().scrollIntoViewIfNeeded();
    await expect(page.locator('canvas.screen')).not.toBeInViewport();
    await page.waitForTimeout(100); // the last frames in flight
    const paused = await frames(page);
    await page.waitForTimeout(200);
    expect(await frames(page)).toBe(paused);

    await page.locator('canvas.screen').scrollIntoViewIfNeeded();
    await expect.poll(() => frames(page)).toBeGreaterThan(paused);
});

test('goes on when the browser blocks its storage', async ({ page }) => {
    await page.addInitScript(() =>
        Object.defineProperty(window, 'localStorage', {
            get() {
                throw new DOMException('Access is denied for this document.', 'SecurityError');
            }
        })
    );
    await page.goto(APP);
    await chip(page, 'socks').click();
    await expect(page.locator('.look-name h1')).toHaveText('socks');
    await page.locator('.favorite').click();
    await expect(page.locator('.favorites .chip')).toHaveCount(1);
});

test('goes on when the browser storage is full', async ({ page }) => {
    await page.addInitScript(() => {
        Storage.prototype.setItem = () => {
            throw new DOMException('The quota has been exceeded.', 'QuotaExceededError');
        };
    });
    await page.goto(APP);
    await chip(page, 'socks').click();
    await expect(page.locator('.look-name h1')).toHaveText('socks');
});

test('without Web Serial, says where to send from, once', async ({ page }) => {
    await page.addInitScript(() =>
        Object.defineProperty(Navigator.prototype, 'serial', { value: undefined, configurable: true })
    );
    await page.goto(APP);
    await expect(note(page)).toHaveText('Pour envoyer un look, ouvre l’app dans Chrome ou Edge sur ordinateur.');
    await expect(page.locator('.device')).toHaveCount(0);
    await expect(page.locator('.primary')).toBeDisabled();
});
