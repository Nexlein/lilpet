// scripts/validate-data.mjs refuses bad data: each case breaks a copy of the data files and reads the error.
import { spawnSync } from 'node:child_process';
import { cpSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
import { expect, test } from '@playwright/test';

const SCRIPT = resolve('scripts/validate-data.mjs');
const LOOKS = 'eyes/looks.json';
const EMOTIONS = 'eyes/emotions.json';
const SCENES = 'prototype/scenes.json';

// Runs the validator on a copy of the data, after `change(data)`; returns its exit code and error output
function validate(file, change) {
    const dir = mkdtempSync(join(tmpdir(), 'lilpet-data-'));
    try {
        for (const path of [LOOKS, EMOTIONS, SCENES]) cpSync(path, join(dir, path));
        if (file) {
            const data = JSON.parse(readFileSync(file, 'utf8'));
            change(data);
            writeFileSync(join(dir, file), JSON.stringify(data));
        }
        const { status, stderr } = spawnSync(process.execPath, [SCRIPT], { cwd: dir, encoding: 'utf8' });
        return { status, stderr };
    } finally {
        rmSync(dir, { recursive: true });
    }
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
    [SCENES, 'particles all gone', (d) => (sleepy(d).particles.time = 4), 'no particle is alive'],
    [EMOTIONS, 'no emotions', (d) => delete d.emotions, '"emotions" must be a non-empty object'],
    [EMOTIONS, 'an emotion that is null', (d) => (d.emotions.sad = null), 'emotions.sad: must be an object'],
    [EMOTIONS, 'no label', (d) => delete d.emotions.sad.label, '"label" must be'],
    [EMOTIONS, 'a misspelled field', (d) => (d.emotions.sad.tll = 0.2), 'unknown key "tll"'],
    [EMOTIONS, 'a field out of range', (d) => (d.emotions.sad.tl = 1.5), '"tl" must be between'],
    [EMOTIONS, 'a field that is not a number', (d) => (d.emotions.sad.ps = '1'), '"ps" must be between'],
    [EMOTIONS, 'an unknown particle', (d) => (d.emotions.love.emit.type = 'star'), '"emit.type" must be'],
    [EMOTIONS, 'no particle rate', (d) => delete d.emotions.love.emit.rate, '"emit.rate" must be'],
    [EMOTIONS, 'a name that cannot be a C enumerator', (d) => (d.emotions['so-sad'] = d.emotions.sad), 'must match'],
    [EMOTIONS, 'a reaction to an unknown emotion', (d) => (d.reactions.tap.emotion = 'bored'), '"emotion" must be'],
    [EMOTIONS, 'a reaction to toString', (d) => (d.reactions.tap.emotion = 'toString'), '"emotion" must be'],
    [EMOTIONS, 'a reaction that lasts no time', (d) => (d.reactions.tap.duration = 0), '"duration" must be'],
    [EMOTIONS, 'half a sparkle', (d) => (d.reactions.tap.sparkles = 1.5), '"sparkles" must be'],
    [EMOTIONS, 'too many sparkles', (d) => (d.reactions.tap.sparkles = 33), '"sparkles" must be'],
    [EMOTIONS, 'a misspelled reaction field', (d) => (d.reactions.tap.sparkle = 1), 'unknown key "sparkle"']
];
for (const [file, what, change, message] of CASES)
    test(`${file}: ${what} is refused`, () => {
        const { status, stderr } = validate(file, change);
        expect(stderr).toContain(message);
        expect(stderr).not.toContain('    at '); // an error message, not a stack trace
        expect(status).toBe(1);
    });
