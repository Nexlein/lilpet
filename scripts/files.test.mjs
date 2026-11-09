// Tests of scripts/files.mjs: a group that a command names but files.mjs lacks, or that holds no file, would let the
// command pass on nothing (xargs -r runs nothing on no input). Built-in test runner, no dependency.
// Usage: npm run check:scripts
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';
import { GROUPS, trackedFiles } from './files.mjs';

test('every group that a command names exists and holds tracked files', () => {
    const commands = readFileSync('package.json', 'utf8') + readFileSync('mise.toml', 'utf8');
    const named = [...commands.matchAll(/scripts\/files\.mjs (\w+)/g)].map((m) => m[1]);
    assert.ok(named.length >= 8, named.join(' '));
    for (const group of named) {
        assert.ok(Object.hasOwn(GROUPS, group), `no group "${group}" in scripts/files.mjs`);
        assert.ok(trackedFiles(group).length > 0, `the group "${group}" holds no tracked file`);
    }
});

test("a group takes dotfiles, and others' sources stay out", () => {
    const js = trackedFiles('js');
    assert.ok(js.includes('.lintstagedrc.mjs') && js.includes('app/src/ui/hooks.ts'));
    assert.ok(trackedFiles('c').includes('firmware/third_party/bmi270/bmi270_config.h'));
    assert.ok(!trackedFiles('c').includes('firmware/third_party/bmi270/bmi270_config.c')); // others' sources stay
});

test('an unknown group stops with the list of groups', () => {
    const { status, stderr } = spawnSync(process.execPath, ['scripts/files.mjs', 'jss'], { encoding: 'utf8' });
    assert.equal(status, 2);
    assert.ok(stderr.includes(Object.keys(GROUPS).join('|')), stderr);
});
