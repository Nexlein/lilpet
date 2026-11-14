// Tests of scripts/validate-data.mjs and of the generators of the engine's C tables (gen-*-table.mjs), on copies of the
// data: each case breaks a copy of a data file and reads the error. Built-in test runner, no dependency; the copies are
// checked side by side.
// Usage: npm run check:scripts
import assert from 'node:assert/strict';
import { execFile } from 'node:child_process';
import { cpSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { availableParallelism, tmpdir } from 'node:os';
import { join } from 'node:path';
import { describe, test } from 'node:test';
import { EMOTIONS, EMOTIONS_TABLE, GENERATORS, LOOKS, LOOKS_TABLE, SCENES, SCENES_TABLE } from './data.mjs';

const SCRIPT = 'scripts/validate-data.mjs';
const EMOTIONS_SCRIPT = GENERATORS[EMOTIONS_TABLE];
const SCENES_SCRIPT = GENERATORS[SCENES_TABLE];
const PARALLEL = { concurrency: availableParallelism() }; // the options of a suite whose tests run side by side

// Calls `use` with a copy of the data and of the scripts, with the generated files and the engine's names the validator
// imports (engine/wasm/engine.mjs).
async function withCopy(use) {
    const dir = mkdtempSync(join(tmpdir(), 'lilpet-data-'));
    try {
        for (const path of [
            'package.json',
            'scripts',
            'eyes',
            'engine/wasm/engine.mjs',
            'engine/include/lilpet/screen.h',
            SCENES,
            ...Object.keys(GENERATORS)
        ])
            cpSync(path, join(dir, path), { recursive: true });
        return await use(dir);
    } finally {
        rmSync(dir, { recursive: true });
    }
}

// Runs a script in a copy; gives its exit code (null if it was stopped) and error output
function run(dir, ...command) {
    return new Promise((done) =>
        execFile(process.execPath, command, { cwd: dir, timeout: 10_000 }, (error, _stdout, stderr) =>
            done({ status: error ? (typeof error.code === 'number' ? error.code : null) : 0, stderr })
        )
    );
}

function changeFile(dir, file, edit) {
    const data = JSON.parse(readFileSync(file, 'utf8'));
    edit(data);
    writeFileSync(join(dir, file), JSON.stringify(data));
}

// Runs the validator on a copy of the data, after `edit(data)`
function validate(file, edit) {
    return withCopy((dir) => {
        if (file) changeFile(dir, file, edit);
        return run(dir, SCRIPT);
    });
}

// An error message that says `message`, not a stack trace, and a failure
function assertRefused({ status, stderr }, message) {
    assert.ok(stderr.includes(message), `expected ${JSON.stringify(message)} in:\n${stderr}`);
    assert.ok(!stderr.includes('    at '), `a stack trace:\n${stderr}`);
    assert.equal(status, 1);
}

test('the data files are valid', async () => {
    assert.deepEqual(await validate(), { status: 0, stderr: '' });
});

const sleepy = (d) => d.scenes.find((s) => s.id === 'emotion-sleepy');

const CASES = [
    [LOOKS, 'a color that is not a string', (d) => (d.swatches[0].sclera = ['#AE0002']), '"sclera" must be'],
    [LOOKS, 'a pupil color that is not a string', (d) => (d.swatches[0].pupilColor[1] = ['#AE0002']), '"pupilColor"'],
    [LOOKS, 'one pupil color', (d) => d.swatches[0].pupilColor.pop(), '"pupilColor"'],
    [LOOKS, 'no swatches', (d) => delete d.swatches, '"swatches" must be a non-empty array'],
    [LOOKS, 'a swatch less than the catalogue', (d) => d.swatches.pop(), '99 swatches'],
    [
        LOOKS,
        'a swatch more than the catalogue',
        (d) => d.swatches.push({ ...d.swatches[0], id: 'more' }),
        '101 swatches'
    ],
    [LOOKS, 'HTML in an id', (d) => (d.swatches[0].id = '<img src=x>'), '"id" must match'],
    [LOOKS, 'an id used twice', (d) => (d.swatches[1].id = d.swatches[0].id), 'duplicate id'],
    [LOOKS, 'a swatch that is null', (d) => (d.swatches[0] = null), 'must be an object'],
    [LOOKS, 'an unknown default swatch', (d) => (d.default.swatch = 'plaid'), '"default.swatch" must be'],
    [LOOKS, 'an unknown default pupil', (d) => (d.default.pupil = 'star'), '"default.pupil" must be'],
    [LOOKS, 'no eye shape', (d) => delete d.eye, '"eye.rx" must be'],
    [LOOKS, 'an eye wider than the screen', (d) => (d.eye.rx = 300), '"eye.rx" must be'],
    [LOOKS, 'more tilt than measured', (d) => (d.eye.tilt = 0.5), '"eye.tilt" must be'],
    [LOOKS, 'no pupil lean', (d) => delete d.pupilLean, '"pupilLean" must be'],
    [LOOKS, 'a pupil type missing', (d) => delete d.pupils.cat, '"pupils.cat" is missing'],
    [LOOKS, 'an unknown pupil type', (d) => (d.pupils.star = d.pupils.dot), 'pupils.star: must be one of'],
    [LOOKS, 'a misspelled pupil key', (d) => (d.pupils.dot.rxx = 0.1), 'unknown key "rxx"'],
    [LOOKS, 'a pupil without a radius', (d) => delete d.pupils.dot.ry, '"ry" must be between'],
    [LOOKS, 'a circle that is not a disc', (d) => (d.pupils.circle.rx = 0.5), '"rx" is not used'],
    [LOOKS, 'a pupil resting outside its eye', (d) => (d.pupils.acorn.y = 2), '"y" must be between'],
    [SCENES, 'no scenes', (d) => (d.scenes = []), '"scenes" must be a non-empty array'],
    [SCENES, 'a scene that is null', (d) => (d.scenes[0] = null), 'must be an object'],
    [SCENES, 'a misspelled key', (d) => (d.scenes[0].emotions = 'sad'), 'unknown key "emotions"'],
    [SCENES, 'an unknown swatch', (d) => (d.scenes[0].swatch = 'plaid'), '"swatch" must be'],
    [SCENES, 'an unknown pupil', (d) => (d.scenes[0].pupil = 'star'), '"pupil" must be'],
    [SCENES, 'an emotion inherited from Object', (d) => (d.scenes[0].emotion = 'toString'), '"emotion" must be'],
    [SCENES, 'a blink that is null', (d) => (d.scenes[0].blink = null), '"blink" must be an object'],
    [SCENES, 'an unknown blink phase', (d) => (d.scenes[0].blink = { phase: 'wink', t: 0 }), '"blink.phase" must be'],
    [SCENES, 'a blink too long', (d) => (d.scenes[0].blink = { phase: 'open', t: 2 }), '"blink.t" must be'],
    [SCENES, 'a head that is not an object', (d) => (d.scenes[0].head = 1), '"head" must be an object'],
    [SCENES, 'an unknown head angle', (d) => (d.scenes[0].head = { tilt: 0 }), '"head.tilt" must be'],
    [SCENES, 'a yaw out of range', (d) => (d.scenes[0].head = { yaw: 1.5 }), '"head.yaw" must be'],
    [SCENES, 'a roll past a half turn', (d) => (d.scenes[0].head = { roll: 3.5 }), '"head.roll" must be'],
    [SCENES, 'a gaze that is not an object', (d) => (d.scenes[0].gaze = 1), '"gaze" must be an object'],
    [SCENES, 'an unknown gaze axis', (d) => (d.scenes[0].gaze = { z: 0 }), '"gaze.z" must be'],
    [SCENES, 'a gaze out of range', (d) => (d.scenes[0].gaze = { x: 1.5 }), '"gaze.x" must be'],
    [SCENES, 'particles that are null', (d) => (d.scenes[0].particles = null), '"particles" must be an object'],
    [SCENES, 'a seed past 32 bits', (d) => (sleepy(d).particles.seed = 2 ** 32), '"particles.seed" must be'],
    [SCENES, 'a negative seed', (d) => (sleepy(d).particles.seed = -1), '"particles.seed" must be'],
    [SCENES, 'half a seed', (d) => (sleepy(d).particles.seed = 1.5), '"particles.seed" must be'],
    [
        SCENES,
        'particles without an emitter',
        (d) => (d.scenes[0].particles = { seed: 1, time: 1 }),
        'emits no particles'
    ],
    [SCENES, 'a time too long to simulate', (d) => (sleepy(d).particles.time = 1e9), '"particles.time" must be'],
    [
        SCENES,
        'a time that is not exact in a float',
        (d) => (sleepy(d).particles.time = 0.525),
        '"particles.time" must be'
    ],
    [EMOTIONS, 'no emotions', (d) => delete d.emotions, '"emotions" must be a non-empty object'],
    [EMOTIONS, 'no reactions', (d) => delete d.reactions, '"reactions" must be a non-empty object'],
    [EMOTIONS, 'no neutral emotion', (d) => delete d.emotions.neutral, '"emotions.neutral" is missing'],
    [EMOTIONS, 'no sleepy emotion', (d) => delete d.emotions.sleepy, '"emotions.sleepy" is missing'],
    [EMOTIONS, 'no shake reaction', (d) => delete d.reactions.shake, '"reactions.shake" is missing'],
    [EMOTIONS, 'no upside_down reaction', (d) => delete d.reactions.upside_down, '"reactions.upside_down" is missing'],
    [EMOTIONS, 'no tap reaction', (d) => delete d.reactions.tap, '"reactions.tap" is missing'],
    [EMOTIONS, 'no face_seen reaction', (d) => delete d.reactions.face_seen, '"reactions.face_seen" is missing'],
    [EMOTIONS, 'an emotion that is null', (d) => (d.emotions.sad = null), 'emotions.sad: must be an object'],
    [EMOTIONS, 'no label', (d) => delete d.emotions.sad.label, '"label" must be'],
    [EMOTIONS, 'a misspelled field', (d) => (d.emotions.sad.tll = 0.2), 'unknown key "tll"'],
    [EMOTIONS, 'a field out of range', (d) => (d.emotions.sad.tl = 1.5), '"tl" must be between'],
    [EMOTIONS, 'an eye squashed flat', (d) => (d.emotions.sad.sq = 1), '"sq" must be between'],
    [EMOTIONS, 'an eye turned past a quarter turn', (d) => (d.emotions.sad.rot = -1.25), '"rot" must be between'],
    [EMOTIONS, 'a note that ends its C comment', (d) => (d.emotions.sad.note = 'a */ b'), '"note" must be'],
    [EMOTIONS, 'a note that starts a C comment', (d) => (d.reactions.tap.note = 'a /* b'), '"note" must be'],
    [EMOTIONS, 'a note that is not a string', (d) => (d.emotions.sad.note = 1), '"note" must be'],
    [EMOTIONS, 'a field that is not a number', (d) => (d.emotions.sad.ps = '1'), '"ps" must be between'],
    [EMOTIONS, 'an unknown particle', (d) => (d.emotions.love.emit.type = 'star'), '"emit.type" must be'],
    [EMOTIONS, 'no particle rate', (d) => delete d.emotions.love.emit.rate, '"emit.rate" must be'],
    [EMOTIONS, 'a particle rate too high', (d) => (d.emotions.love.emit.rate = 31), '"emit.rate" must be'],
    [EMOTIONS, 'a name that cannot be a C enumerator', (d) => (d.emotions['so-sad'] = d.emotions.sad), 'must match'],
    [EMOTIONS, 'a reaction to an unknown emotion', (d) => (d.reactions.tap.emotion = 'bored'), '"emotion" must be'],
    [EMOTIONS, 'a reaction to toString', (d) => (d.reactions.tap.emotion = 'toString'), '"emotion" must be'],
    [EMOTIONS, 'a reaction that lasts no time', (d) => (d.reactions.tap.duration = 0), '"duration" must be'],
    [EMOTIONS, 'half a sparkle', (d) => (d.reactions.tap.sparkles = 1.5), '"sparkles" must be'],
    [EMOTIONS, 'too many sparkles', (d) => (d.reactions.tap.sparkles = 33), '"sparkles" must be'],
    [EMOTIONS, 'a misspelled reaction field', (d) => (d.reactions.tap.sparkle = 1), 'unknown key "sparkle"'],
    [EMOTIONS, 'sparkles on a shake', (d) => (d.reactions.shake.sparkles = 1), '"sparkles" must be 0'],
    [EMOTIONS, 'sparkles upside down', (d) => (d.reactions.upside_down.sparkles = 1), '"sparkles" must be 0']
];
describe('the validator refuses', PARALLEL, () => {
    for (const [file, what, change, message] of CASES)
        test(`${file}: ${what}`, async () => assertRefused(await validate(file, change), message));

    for (const file of [LOOKS, EMOTIONS, SCENES])
        test(`${file}: a file that is not JSON`, async () => {
            const result = await withCopy((dir) => {
                writeFileSync(join(dir, file), 'not JSON');
                return run(dir, SCRIPT);
            });
            assertRefused(result, `${file}: Unexpected token`);
        });
});

describe('the table generators', PARALLEL, () => {
    test('npm run tables runs every generator, even after one fails', () =>
        withCopy(async (dir) => {
            changeFile(dir, EMOTIONS, (d) => (d.emotions.sad.tl = 0.5)); // the first generator fails
            writeFileSync(join(dir, SCENES), 'not JSON'); // and the last one
            const { status, stderr } = await run(dir, 'scripts/tables.mjs', '--check');
            assert.equal(status, 1);
            assert.ok(stderr.includes(`${EMOTIONS_TABLE}: out of date`), stderr);
            assert.ok(stderr.includes('Unexpected token'), stderr);
        }));

    test('a fault in scenes.json stops the scene table, not the emotion tables', () =>
        withCopy(async (dir) => {
            assert.equal((await run(dir, SCENES_SCRIPT, '--check')).status, 0); // only the fault can stop it
            writeFileSync(join(dir, SCENES), 'not JSON');
            assert.notEqual((await run(dir, SCENES_SCRIPT, '--check')).status, 0);
            assert.deepEqual(await run(dir, EMOTIONS_SCRIPT, '--check'), { status: 0, stderr: '' });
        }));

    test('an unknown default swatch stops the looks table with a message', () =>
        withCopy(async (dir) => {
            changeFile(dir, LOOKS, (d) => (d.default.swatch = 'plaid'));
            assertRefused(await run(dir, GENERATORS[LOOKS_TABLE], '--check'), 'the default swatch "plaid"');
        }));

    test('a changed emotion makes the tables out of date, until generated again', () =>
        withCopy(async (dir) => {
            changeFile(dir, EMOTIONS, (d) => (d.emotions.sad.tl = 0.5));
            const stale = await run(dir, EMOTIONS_SCRIPT, '--check');
            assert.equal(stale.status, 1);
            assert.ok(stale.stderr.includes(`${EMOTIONS_TABLE}: out of date, run npm run tables`), stale.stderr);

            await run(dir, EMOTIONS_SCRIPT);
            assert.equal((await run(dir, EMOTIONS_SCRIPT, '--check')).status, 0);
        }));
});
