// Tests of scripts/files.ts: a group that a command names but files.ts lacks, or that holds no file, would let the
// command pass on nothing (xargs -r runs nothing on no input). Also of the other lists of files: no JavaScript, and the
// data files of the commit hook (.lintstagedrc.ts). Built-in test runner, no dependency.
// Usage: npm run check:scripts
import assert from 'node:assert/strict';
import { execFileSync, spawnSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';
import { DATA } from '../.lintstagedrc.ts';
import { GROUPS, trackedFiles } from './files.ts';

const tracked = (...pathspecs: string[]) =>
    execFileSync('git', ['ls-files', '-z', '--', ...pathspecs], { encoding: 'utf8' })
        .split('\0')
        .filter(Boolean);

test('every group that a command names exists and holds tracked files', () => {
    const commands = readFileSync('package.json', 'utf8') + readFileSync('mise.toml', 'utf8');
    const named = [...commands.matchAll(/scripts\/files\.ts (\w+)/g)].map((m) => m[1] ?? '');
    assert.ok(named.length >= 8, named.join(' '));
    for (const group of named) {
        assert.ok(Object.hasOwn(GROUPS, group), `no group "${group}" in scripts/files.ts`);
        assert.ok(trackedFiles(group as keyof typeof GROUPS).length > 0, `the group "${group}" holds no tracked file`);
    }
});

test("a group takes dotfiles, and others' sources stay out", () => {
    const ts = trackedFiles('ts');
    assert.ok(
        ts.includes('.lintstagedrc.ts') && ts.includes('app/src/ui/hooks.ts') && ts.includes('app/src/ui/App.tsx')
    );
    assert.ok(trackedFiles('c').includes('firmware/third_party/bmi270/bmi270_config.h'));
    assert.ok(!trackedFiles('c').includes('firmware/third_party/bmi270/bmi270_config.c')); // others' sources stay
});

test('a group is the same from a subfolder', () => {
    const fromApp = spawnSync(process.execPath, ['../scripts/files.ts', 'ts'], { cwd: 'app', encoding: 'utf8' });
    const paths = fromApp.stdout.split('\0').filter(Boolean);
    assert.equal(paths.length, trackedFiles('ts').length);
    assert.ok(paths.includes('../.lintstagedrc.ts'), fromApp.stdout); // relative to where it runs, for xargs
});

test('an unknown group stops with the list of groups', () => {
    const { status, stderr } = spawnSync(process.execPath, ['scripts/files.ts', 'tss'], { encoding: 'utf8' });
    assert.equal(status, 2);
    assert.ok(stderr.includes(Object.keys(GROUPS).join('|')), stderr);
});

// The code is TypeScript that Node runs as it is and tsc checks (tsconfig.json, app/tsconfig*.json): a JavaScript file
// would escape the type check. The WebAssembly build (engine/build/wasm/lilpet.mjs) is built, never tracked.
test('no JavaScript file is tracked', () => {
    assert.deepEqual(tracked('*.js', '*.mjs', '*.cjs', '*.jsx'), []);
});

// A path of the list that is renamed would no longer match, and git diff --exit-code on a missing path passes: the
// data check of the commit hook would silently skip the file
test('every data file of the commit hook is tracked', () => {
    assert.deepEqual(
        DATA.filter((path) => tracked(path).length === 0),
        []
    );
});
