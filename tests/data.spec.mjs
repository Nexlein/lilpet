// scripts/validate-data.mjs refuses bad data: each case breaks a copy of the data files and reads the error. The
// generators of the C files work on such a copy too.
import { spawnSync } from 'node:child_process';
import { cpSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { expect, test } from '@playwright/test';

const SCRIPT = 'scripts/validate-data.mjs';
const TABLES_SCRIPT = 'scripts/gen-emotions-table.mjs';
const VECTORS_SCRIPT = 'scripts/gen-engine-vectors.mjs';
const TABLES = 'engine/src/emotions_table.h';
const VECTORS = 'engine/tests/js_vectors.h';
const LOOKS = 'eyes/looks.json';
const EMOTIONS = 'eyes/emotions.json';
const SCENES = 'prototype/scenes.json';

// Calls `use` with a copy of the data. The scripts and the prototype are copied too: eyes.js imports the emotions.json
// next to it, not the one of the working directory.
function withCopy(use) {
    const dir = mkdtempSync(join(tmpdir(), 'lilpet-data-'));
    try {
        for (const path of ['package.json', 'scripts', 'eyes', 'prototype', TABLES, VECTORS])
            cpSync(path, join(dir, path), { recursive: true });
        return use(dir);
    } finally {
        rmSync(dir, { recursive: true });
    }
}

// Runs a script in a copy; returns its exit code and error output
function run(dir, ...command) {
    const { status, stderr } = spawnSync(process.execPath, command, {
        cwd: dir,
        encoding: 'utf8',
        timeout: 10_000 // Playwright cannot stop a synchronous call
    });
    return { status, stderr };
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

test('the data files are valid', () => {
    expect(validate().stderr).toBe('');
    expect(validate().status).toBe(0);
});

const sleepy = (d) => d.scenes.find((s) => s.id === 'emotion-sleepy');

const CASES = [
    [LOOKS, 'a color that is not a string', (d) => (d.swatches[0].sclera = ['#AE0002']), '"sclera" must be'],
    [LOOKS, 'a pupil color that is not a string', (d) => (d.swatches[0].pupilColor[1] = ['#AE0002']), '"pupilColor"'],
    [LOOKS, 'a swatch less than the catalogue', (d) => d.swatches.pop(), '99 swatches'],
    [
        LOOKS,
        'a swatch more than the catalogue',
        (d) => d.swatches.push({ ...d.swatches[0], id: 'more' }),
        '101 swatches'
    ],
    [LOOKS, 'HTML in an id', (d) => (d.swatches[0].id = '<img src=x>'), '"id" must match'],
    [LOOKS, 'a swatch that is null', (d) => (d.swatches[0] = null), 'must be an object'],
    [SCENES, 'a scene that is null', (d) => (d.scenes[0] = null), 'must be an object'],
    [SCENES, 'an emotion inherited from Object', (d) => (d.scenes[0].emotion = 'toString'), '"emotion" must be'],
    [SCENES, 'a blink that is null', (d) => (d.scenes[0].blink = null), '"blink" must be an object'],
    [SCENES, 'particles that are null', (d) => (d.scenes[0].particles = null), '"particles" must be an object'],
    [SCENES, 'a seed past 32 bits', (d) => (sleepy(d).particles.seed = 2 ** 32), '"particles.seed" must be'],
    [SCENES, 'a negative seed', (d) => (sleepy(d).particles.seed = -1), '"particles.seed" must be'],
    [SCENES, 'half a seed', (d) => (sleepy(d).particles.seed = 1.5), '"particles.seed" must be'],
    [SCENES, 'particles all gone', (d) => (sleepy(d).particles.time = 4), 'no particle is alive'],
    [SCENES, 'a time too long to simulate', (d) => (sleepy(d).particles.time = 1e9), '"particles.time" must be'],
    [SCENES, 'a time JS and C round apart', (d) => (sleepy(d).particles.time = 0.525), '"particles.time" must be'],
    [EMOTIONS, 'no emotions', (d) => delete d.emotions, '"emotions" must be a non-empty object'],
    [EMOTIONS, 'no neutral emotion', (d) => delete d.emotions.neutral, '"emotions.neutral" is missing'],
    [EMOTIONS, 'no sleepy emotion', (d) => delete d.emotions.sleepy, '"emotions.sleepy" is missing'],
    [EMOTIONS, 'no shake reaction', (d) => delete d.reactions.shake, '"reactions.shake" is missing'],
    [EMOTIONS, 'no upside_down reaction', (d) => delete d.reactions.upside_down, '"reactions.upside_down" is missing'],
    [EMOTIONS, 'no tap reaction', (d) => delete d.reactions.tap, '"reactions.tap" is missing'],
    [EMOTIONS, 'no face_seen reaction', (d) => delete d.reactions.face_seen, '"reactions.face_seen" is missing'],
    [EMOTIONS, 'an emission too slow for a scene', (d) => (d.emotions.sleepy.emit.rate = 0.01), 'no particle is alive'],
    [EMOTIONS, 'an emotion that is null', (d) => (d.emotions.sad = null), 'emotions.sad: must be an object'],
    [EMOTIONS, 'no label', (d) => delete d.emotions.sad.label, '"label" must be'],
    [EMOTIONS, 'a misspelled field', (d) => (d.emotions.sad.tll = 0.2), 'unknown key "tll"'],
    [EMOTIONS, 'a field out of range', (d) => (d.emotions.sad.tl = 1.5), '"tl" must be between'],
    [EMOTIONS, 'an eye squashed flat', (d) => (d.emotions.sad.sq = 1), '"sq" must be between'],
    [EMOTIONS, 'an eye turned past a quarter turn', (d) => (d.emotions.sad.rot = -1.25), '"rot" must be between'],
    [EMOTIONS, 'a note that ends its C comment', (d) => (d.emotions.sad.note = 'a */ b'), '"note" must be'],
    [EMOTIONS, 'a note that starts a C comment', (d) => (d.reactions.tap.note = 'a /* b'), '"note" must be'],
    [EMOTIONS, 'a field that is not a number', (d) => (d.emotions.sad.ps = '1'), '"ps" must be between'],
    [EMOTIONS, 'an unknown particle', (d) => (d.emotions.love.emit.type = 'star'), '"emit.type" must be'],
    [EMOTIONS, 'no particle rate', (d) => delete d.emotions.love.emit.rate, '"emit.rate" must be'],
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
for (const [file, what, change, message] of CASES)
    test(`${file}: ${what} is refused`, () => {
        const { status, stderr } = validate(file, change);
        expect(stderr).toContain(message);
        expect(stderr).not.toContain('    at '); // an error message, not a stack trace
        expect(status).toBe(1);
    });

test('a fault in scenes.json stops the vectors, not the emotion tables', () => {
    withCopy((dir) => {
        expect(run(dir, VECTORS_SCRIPT, '--check').status).toBe(0); // only the fault can stop them
        writeFileSync(join(dir, SCENES), 'not JSON');
        expect(run(dir, VECTORS_SCRIPT, '--check').status).not.toBe(0);
        expect(run(dir, TABLES_SCRIPT, '--check')).toEqual({ status: 0, stderr: '' });
    });
});

test('a changed emotion makes the tables out of date, until generated again', () => {
    withCopy((dir) => {
        changeFile(dir, EMOTIONS, (d) => (d.emotions.sad.tl = 0.5));
        const stale = run(dir, TABLES_SCRIPT, '--check');
        expect(stale.status).toBe(1);
        expect(stale.stderr).toContain(`${TABLES}: out of date, run npm run vectors`);

        run(dir, TABLES_SCRIPT);
        expect(run(dir, TABLES_SCRIPT, '--check').status).toBe(0);
    });
});
