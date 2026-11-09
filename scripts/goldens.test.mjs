// Tests of how scripts/goldens.mjs compares what the engine drew with the reference renders, on small made-up images in
// a throwaway folder: what a check reports, writes apart and clears, and what npm run goldens rewrites and removes. The
// drawing itself is checked by mise run engine:goldens. Built-in test runner, no dependency.
// Usage: npm run check:scripts
import assert from 'node:assert/strict';
import { test } from 'node:test';
import { existsSync, mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { compare, strict } from './goldens.mjs';
import { decodePng, encodePng } from './png.mjs';

const SIZE = 2;
const HASHES = '# comment\nrun | 1 part | 16: 0000000000000001\nrun | 1 part | 32: 0000000000000002\n';
const image = (name, value, height = SIZE) => ({
    name,
    label: name.replace('-', ' '),
    height,
    rgb: Buffer.alloc(SIZE * height * 3, value)
});
// What renders gives: two scenes, the strip of the animation and its hashes
const drawn = (changes = {}) => ({
    size: SIZE,
    images: [image('scene-a', 10), image('scene-b', 20), image('animation', 30, 2 * SIZE)],
    hashes: HASHES,
    firstDifferent: null,
    ...changes
});

// Calls `use` with a folder of references written from `drawn()` and the folder where a check writes what differs
function withReferences(use) {
    const root = mkdtempSync(join(tmpdir(), 'lilpet-goldens-'));
    const dir = join(root, 'goldens');
    const failed = join(root, 'failed');
    try {
        assert.equal(compare(drawn(), { dir, failed, check: false }).rewritten, 4);
        return use(dir, failed);
    } finally {
        rmSync(root, { recursive: true, force: true });
    }
}

const files = (dir) => (existsSync(dir) ? readdirSync(dir).toSorted() : []);
const contents = (dir) => Object.fromEntries(files(dir).map((f) => [f, readFileSync(join(dir, f)).toString('hex')]));

test('writes every reference, which a check then finds the same', () =>
    withReferences((dir, failed) => {
        assert.deepEqual(files(dir), ['animation.png', 'animation.txt', 'scene-a.png', 'scene-b.png']);
        assert.ok(decodePng(readFileSync(join(dir, 'scene-a.png'))).rgb.equals(image('scene-a', 10).rgb));
        assert.equal(readFileSync(join(dir, 'animation.txt'), 'utf8'), HASHES);
        assert.deepEqual(compare(drawn(), { dir, failed, check: true }), { problems: [], rewritten: 0 });
        assert.deepEqual(files(failed), []);
    }));

test('a check reports what differs, writes what was drawn apart and leaves the references as they are', () =>
    withReferences((dir, failed) => {
        const a = image('scene-a', 10);
        a.rgb[0] = 11; // one pixel
        writeFileSync(join(dir, 'scene-a.png'), encodePng(SIZE, SIZE, a.rgb));
        writeFileSync(join(dir, 'scene-b.png'), encodePng(1, 1, Buffer.alloc(3)));
        rmSync(join(dir, 'animation.png'));
        writeFileSync(join(dir, 'scene-gone.png'), 'stale');
        const before = contents(dir);
        const frame = { line: 'run | 1 part | 32', rgb: Buffer.alloc(SIZE * SIZE * 3, 40) };

        const { problems } = compare(drawn({ hashes: HASHES.replace('0002', '0003'), firstDifferent: frame }), {
            dir,
            failed,
            check: true
        });
        assert.deepEqual(problems, [
            `${dir}/scene-a.png: scene a: 1 pixel differs (now: ${failed}/scene-a.png)`,
            `${dir}/scene-b.png: scene b: the reference has another size (now: ${failed}/scene-b.png)`,
            `${dir}/animation.png: animation: missing or unreadable (now: ${failed}/animation.png)`,
            `${dir}/animation.txt differs from "run | 1 part | 32" on (that frame: ${failed}/animation-first-difference.png) (now: ${failed}/animation.txt)`,
            `${dir}/scene-gone.png: nothing draws it any more`
        ]);
        assert.deepEqual(contents(dir), before);
        assert.deepEqual(files(failed), [
            'animation-first-difference.png',
            'animation.png',
            'animation.txt',
            'scene-a.png',
            'scene-b.png'
        ]);
        assert.ok(decodePng(readFileSync(join(failed, 'animation-first-difference.png'))).rgb.equals(frame.rgb));
    }));

test('a check tells frames no longer drawn from comments that changed', () =>
    withReferences((dir, failed) => {
        const fewer = HASHES.split('\n').slice(0, -2).join('\n') + '\n'; // without the last frame
        assert.deepEqual(compare(drawn({ hashes: fewer }), { dir, failed, check: true }).problems, [
            `${dir}/animation.txt ends with 1 frame that is not drawn any more (now: ${failed}/animation.txt)`
        ]);
        const comments = HASHES.replace('# comment', '# another comment');
        assert.deepEqual(compare(drawn({ hashes: comments }), { dir, failed, check: true }).problems, [
            `${dir}/animation.txt differs only in its comment or blank lines (now: ${failed}/animation.txt)`
        ]);
    }));

test('a check reports missing hashes, and no first different frame stays once every frame matches', () =>
    withReferences((dir, failed) => {
        const frame = { line: 'run | 1 part | 32', rgb: Buffer.alloc(SIZE * SIZE * 3, 40) };
        compare(drawn({ hashes: HASHES.replace('0002', '0003'), firstDifferent: frame }), { dir, failed, check: true });
        assert.ok(files(failed).includes('animation-first-difference.png'));
        compare(drawn({ hashes: HASHES.replace('# comment', '# another comment') }), { dir, failed, check: true });
        assert.ok(!files(failed).includes('animation-first-difference.png')); // its frame is right again
        rmSync(join(dir, 'animation.txt'));
        assert.deepEqual(compare(drawn(), { dir, failed, check: true }).problems, [
            `${dir}/animation.txt is missing (now: ${failed}/animation.txt)`
        ]);
    }));

test('a passing check removes what an earlier failure left for the references that match again', () =>
    withReferences((dir, failed) => {
        compare(
            drawn({ images: [image('scene-a', 99)], hashes: '', firstDifferent: { line: 'x', rgb: Buffer.alloc(12) } }),
            {
                dir,
                failed,
                check: true
            }
        );
        assert.deepEqual(files(failed), ['animation-first-difference.png', 'animation.txt', 'scene-a.png']);
        assert.deepEqual(compare(drawn(), { dir, failed, check: true }).problems, []);
        assert.deepEqual(files(failed), []);
    }));

test('npm run goldens rewrites only what changed and removes what nothing draws', () =>
    withReferences((dir, failed) => {
        const before = contents(dir);
        writeFileSync(join(dir, 'scene-a.png'), encodePng(1, 1, Buffer.alloc(3)));
        writeFileSync(join(dir, 'scene-gone.png'), 'stale');
        assert.deepEqual(compare(drawn(), { dir, failed, check: false }), { problems: [], rewritten: 1 });
        assert.deepEqual(contents(dir), before);
        assert.deepEqual(files(failed), []);
    }));

test('npm run goldens leaves alone what is not a reference render', () =>
    withReferences((dir, failed) => {
        writeFileSync(join(dir, 'notes.txt'), 'mine');
        assert.deepEqual(compare(drawn(), { dir, failed, check: false }), { problems: [], rewritten: 0 });
        assert.equal(readFileSync(join(dir, 'notes.txt'), 'utf8'), 'mine');
        assert.deepEqual(compare(drawn(), { dir, failed, check: true }).problems, [
            `${dir}/notes.txt: not a reference render`
        ]);
    }));

test('the animation refuses an emotion or an event the engine does not know, which it would ignore', () => {
    const called = [];
    const engine = strict(
        { emotions: ['sad'], setEmotion: (name) => called.push(name), react: (event) => called.push(event), size: 2 },
        ['tap']
    );
    engine.setEmotion('sad');
    engine.react('tap');
    assert.deepEqual(called, ['sad', 'tap']);
    assert.equal(engine.size, 2);
    assert.throws(() => engine.setEmotion('glad'), /unknown emotion: "glad"/);
    assert.throws(() => engine.react('gesture_dislik'), /unknown event: "gesture_dislik"/);
    assert.deepEqual(called, ['sad', 'tap']);
});
