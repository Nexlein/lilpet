// Tests of the code map generator (scripts/codemap.mjs): on small made-up files, what it reads in each language, how it
// resolves what a file uses and the problems --check reports; then the script itself in a throwaway git repository.
// Built-in test runner, no dependency.
// Usage: npm run check:codemap
import assert from 'node:assert/strict';
import { execFileSync, spawnSync } from 'node:child_process';
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { after, test } from 'node:test';
import {
    buildMap,
    collapse,
    cSymbols,
    firstSentence,
    isTest,
    jsExports,
    jsSpecifiers,
    kindOf,
    miseTasks,
    purposeOf,
    repository,
    scanJs,
    usesOf
} from './codemap.mjs';

test('tells source, data and settings files apart', () => {
    assert.equal(kindOf('engine/src/eyes.c'), 'c');
    assert.equal(kindOf('app/src/ui/App.tsx'), 'js');
    assert.equal(kindOf('engine/wasm/engine.d.mts'), 'js');
    assert.equal(kindOf('scripts/flash-guard.py'), 'py');
    assert.equal(kindOf('engine/CMakeLists.txt'), 'cmake');
    assert.equal(kindOf('firmware/Dockerfile'), 'sh');
    assert.equal(kindOf('.husky/pre-commit'), 'sh');
    assert.equal(kindOf('eyes/looks.json'), 'data');
    assert.equal(kindOf('package.json'), null);
    assert.equal(kindOf('app/tsconfig.json'), null);
    assert.equal(kindOf('.vscode/settings.json'), null);
    assert.equal(kindOf('README.md'), null);
    assert.ok(isTest('engine/tests/test_render.c'));
    assert.ok(isTest('app/src/device/session.test.ts'));
    assert.ok(isTest('scripts/test_board_scripts.py'));
    assert.ok(!isTest('scripts/validate-data.mjs'));
});

test('takes the first sentence of the first comment, after a licence', () => {
    assert.equal(purposeOf('/* Seeded numbers. More text. */\nint x;', 'c'), 'Seeded numbers.');
    assert.equal(
        purposeOf('/**\n * Copyright (c) 2023 Someone.\n */\n/* The real purpose */\n', 'c'),
        'The real purpose'
    );
    assert.equal(purposeOf('#!/usr/bin/env node\n// One line\n// and more.\nimport x;', 'js'), 'One line and more.');
    assert.equal(purposeOf('#!/usr/bin/env python3\n"""Docstring here."""\n', 'py'), 'Docstring here.');
    assert.equal(purposeOf("'''Single-quoted docstring.'''\nimport os", 'py'), 'Single-quoted docstring.');
    assert.equal(
        purposeOf('# /// script\n# dependencies = []\n# ///\n# After the block\nimport os', 'py'),
        'After the block'
    );
    assert.equal(purposeOf('<!doctype html><head><!-- The page --></head>', 'html'), 'The page');
    assert.equal(purposeOf('# A CMake file\nproject(x)', 'cmake'), 'A CMake file');
    assert.equal(purposeOf('int main(void) {}', 'c'), '');
    assert.equal(
        firstSentence('Board parts, e.g. the button, i.e. GPIO41. Next one.'),
        'Board parts, e.g. the button, i.e. GPIO41.'
    );
    assert.equal(firstSentence('Calls a.b() and v1.2 then stops. Next.'), 'Calls a.b() and v1.2 then stops.');
    assert.equal(firstSentence('x'.repeat(200)).length, 150);
});

test('lists what a C header declares and what a C source defines', () => {
    const header = `#ifndef X_H
#define X_H
#define LP_SIZE 466
#ifdef __cplusplus
extern "C" {
#endif
typedef struct lp_thing {
    int a;
} lp_thing;
typedef void (*lp_callback)(int value);
typedef enum { LP_A, LP_B } lp_kind;
/* A comment with a fake(call); inside */
void lp_do(lp_thing *t,
           int n);
static inline int lp_inline(int x) { return x; }
const char *lp_name(void);
#ifdef __cplusplus
}
#endif
#endif`;
    assert.deepEqual(cSymbols(header, true), ['LP_SIZE', 'lp_thing', 'lp_callback', 'lp_kind', 'lp_do', 'lp_name']);
    const source = `#include "x.h"
static int helper(void) { return 1; }
void lp_do(lp_thing *t, int n)
{
    if (n) { helper(); }
}
const char *lp_name(void) { return "a{b // not a comment /* nor this"; }
/* it's a comment: void fake(void) { } */
char lp_quote(void) { return '}'; }
static const int TABLE[] = {1, 2};`;
    assert.deepEqual(cSymbols(source, false), ['lp_do', 'lp_name', 'lp_quote']);
});

test('lists the exports and the imports of a JS or TS module', () => {
    const src = `// header
export function one() {}
export async function two() {}
export const THREE = 3;
export type Four = number;
export interface Five {}
export declare function six(): void;
const a = 1, b = 2;
export { a, b as seven };
export default class Eight {}
// import { nope } from './commented';
import x from './x';
import './side-effect.css';
const lazy = await import('./lazy.mjs');
const wasm = new URL('../engine/build/wasm/lilpet.mjs', import.meta.url);
export * from './again';`;
    assert.deepEqual(jsExports(src), ['one', 'two', 'THREE', 'Four', 'Five', 'six', 'Eight', 'a', 'seven']);
    assert.deepEqual(jsSpecifiers(src), [
        './x',
        './again',
        './side-effect.css',
        './lazy.mjs',
        '../engine/build/wasm/lilpet.mjs'
    ]);
    assert.deepEqual(jsExports('const x = 1;\nexport default x;'), ['default']);
    assert.deepEqual(jsSpecifiers('const s = "not an import \'./fake\'";\nconsole.log(\'import "./no"\');'), []);
});

const ignored = (path) => path.startsWith('engine/build/');

test('resolves local files, notes packages and build outputs, reports what is missing', () => {
    const files = [
        'app/src/a.ts',
        'app/src/b.tsx',
        'app/src/data/index.ts',
        'eyes/looks.json',
        'engine/include/lilpet/lilpet.h',
        'engine/src/eyes.c',
        'firmware/main/main.c',
        'firmware/boards/one/board_config.h',
        'firmware/boards/two/board_config.h'
    ];
    const repo = repository(files, ignored);
    const js = usesOf(
        'app/src/a.ts',
        `import { B } from './b';
import data from './data';
import looks from '../../eyes/looks.json';
import { useState } from 'react';
import { x } from '@scope/pkg/sub';
import { readFileSync } from 'node:fs';
import gone from './gone';
import wasm from '../../engine/build/wasm/lilpet.mjs';`,
        'js',
        repo
    );
    assert.deepEqual(js.local, ['app/src/b.tsx', 'app/src/data/index.ts', 'eyes/looks.json']);
    assert.deepEqual(js.packages, ['react', '@scope/pkg']);
    assert.deepEqual(js.unresolved, ['./gone']);
    assert.deepEqual(js.built, ['engine/build/wasm/lilpet.mjs']);
    const c = usesOf(
        'firmware/main/main.c',
        '#include "lilpet/lilpet.h"\n#include "board_config.h"\n#include "esp_timer.h"\n#include <stdio.h>\n/*\n#include "engine/src/eyes.c"\n*/',
        'c',
        repo
    );
    assert.deepEqual(c.local, [
        'engine/include/lilpet/lilpet.h',
        'firmware/boards/one/board_config.h',
        'firmware/boards/two/board_config.h'
    ]);
    assert.deepEqual(collapse(c.local, files), ['engine/include/lilpet/lilpet.h', 'firmware/boards/*/board_config.h']);
    // Two boards out of three: no `*`, which would claim every board
    const three = [...files, 'firmware/boards/three/board_config.h'];
    assert.deepEqual(collapse(c.local, three), c.local);
});

test('reads JS past templates, regular expressions and divisions', () => {
    const src = [
        'const fixture = `',
        "import { fake } from './fixture-only';",
        "${`nested ${'}'} template`}`;",
        'const re = /[`\'"]\\/x/g; // a backtick and quotes in a regular expression',
        'const half = total / 2 / 1;',
        "import { real } from './real';",
        'export const after = 1;'
    ].join('\n');
    assert.deepEqual(jsSpecifiers(src), ['./real']);
    assert.deepEqual(jsExports(src), ['after']);
    assert.equal(scanJs(src).split('\n').length, src.split('\n').length, 'every line keeps its place');
});

test('reads the visible mise tasks with their description', () => {
    const toml = `[tools]
node = "26"

[tasks."engine:test"]
description = "Run the tests"
run = "x"

[tasks.setup]
description = 'Prepare a clone'

[tasks."firmware:board"]
description = "Hidden helper"
hide = true
`;
    assert.deepEqual(miseTasks(toml), [
        { name: 'engine:test', description: 'Run the tests' },
        { name: 'setup', description: 'Prepare a clone' }
    ]);
});

test('builds the map: purposes, users, data readers, task users and problems', () => {
    const contents = new Map(
        Object.entries({
            'mise.toml': '[tasks.check]\ndescription = "Check"\nrun = "uv run scripts/tool.py"\n',
            'package.json': JSON.stringify({
                scripts: { lint: 'oxlint' },
                devDependencies: { oxlint: '1', unused: '1' }
            }),
            'engine/src/raster.h': '/* Spans of shapes */\nvoid lp_spans(void);\n',
            'engine/src/raster.c': '#include "raster.h"\nvoid lp_spans(void) {}\n',
            'engine/src/lonely.c': 'int lonely(void) { return 0; }\n',
            'scripts/tool.py': '"""A tool."""\nimport helper\n',
            'scripts/helper.py': '# Helps\ndef run():\n    pass\n',
            'eyes/looks.json': '{}',
            'app/src/a.ts': "// Reads the looks\nimport looks from '../../eyes/looks.json';\nimport './missing';\n",
            'app/src/a.test.ts': "import './a';\n"
        })
    );
    const { text, problems, count } = buildMap(contents);
    assert.equal(count, 8);
    assert.match(text, /\*\*raster\.c\*\* \(2\) — Implements raster\.h: spans of shapes/);
    assert.match(
        text,
        /\*\*raster\.h\*\* \(2\) — Spans of shapes\n {2}- exports: lp_spans\n {2}- ← engine\/src\/raster\.c/
    );
    assert.match(text, /\*\*looks\.json\*\* \(1\) — data\n {2}- ← app\/src\/a\.ts/);
    assert.match(text, /\*\*tool\.py\*\* \(2\) — A tool\.\n {2}- → scripts\/helper\.py\n {2}- ← mise\.toml/);
    assert.match(text, /- tests: a\.test\.ts/);
    assert.match(text, /- `check` — Check/);
    assert.match(text, /- `lint`: `oxlint`/);
    assert.match(text, /- `oxlint` \(package\.json\) — named in scripts or config/);
    assert.match(text, /- `unused` \(package\.json\) — not referenced/);
    assert.deepEqual(problems, [
        'app/src/a.ts: local import "./missing" not found',
        'engine/src/lonely.c: no purpose comment at the top (what the file is for, in one sentence)'
    ]);
    assert.equal(buildMap(contents).text, text, 'the same files give the same map');
});

// --- The script itself, on the index of a throwaway repository -------------------------------------------------------

const SCRIPT = new URL('./codemap.mjs', import.meta.url).pathname;
const repoDir = mkdtempSync(join(tmpdir(), 'lilpet-codemap-'));
after(() => rmSync(repoDir, { recursive: true, force: true }));

// A throwaway identity for the commits of the test repository, as CI has none
const IDENTITY = {
    GIT_AUTHOR_NAME: 'test',
    GIT_AUTHOR_EMAIL: 'test@example.com',
    GIT_COMMITTER_NAME: 'test',
    GIT_COMMITTER_EMAIL: 'test@example.com'
};
const git = (...args) =>
    execFileSync('git', args, { cwd: repoDir, encoding: 'utf8', env: { ...process.env, ...IDENTITY }, stdio: 'pipe' });
const codemap = (...args) => spawnSync('node', [SCRIPT, ...args], { cwd: repoDir, encoding: 'utf8' });
const put = (path, text) => {
    mkdirSync(join(repoDir, path, '..'), { recursive: true });
    writeFileSync(join(repoDir, path), text);
};

test('writes the map from what is staged and checks it against the staged map', () => {
    git('init', '-q');
    put('src/a.mjs', "// Says hello\nimport { b } from './b.mjs';\nexport const a = b;\n");
    put('src/b.mjs', '// Gives b\nexport const b = 1;\n');
    git('add', 'src');

    assert.equal(codemap().status, 0); // creates .claude/ for the map
    const map = readFileSync(join(repoDir, '.claude/codemap.md'), 'utf8');
    assert.match(map, /\*\*a\.mjs\*\* \(3\) — Says hello\n {2}- exports: a\n {2}- → src\/b\.mjs/);
    assert.equal(codemap('--check').status, 1, 'the map is written but not staged yet');
    git('add', '.claude/codemap.md');
    assert.equal(codemap('--check').status, 0);

    // A change in the working tree only does not count: the map follows what gets committed
    put('src/b.mjs', '// Gives b, now with more\nexport const b = 2;\nexport const c = 3;\n');
    assert.equal(codemap('--check').status, 0);
    git('add', 'src/b.mjs');
    const stale = codemap('--check');
    assert.equal(stale.status, 1);
    assert.match(stale.stderr, /out of date, run npm run codemap and stage it/);

    // A local import that does not resolve fails the check, even with a fresh map
    put('src/c.mjs', "// Uses a file that is not there\nimport { d } from './d.mjs';\nexport const c = d;\n");
    git('add', 'src');
    codemap();
    git('add', '.claude/codemap.md');
    const broken = codemap('--check');
    assert.equal(broken.status, 1);
    assert.match(broken.stderr, /src\/c\.mjs: local import "\.\/d\.mjs" not found/);
});

test('reads a path with a tab, and only resolved files during a merge', () => {
    put('src/odd\tname.mjs', '// A name with a tab in it\nexport const odd = 1;\n');
    git('add', 'src');
    git('commit', '-q', '-m', 'base');
    git('checkout', '-q', '-b', 'other');
    put('src/b.mjs', '// Gives b, the other way\nexport const b = 3;\n');
    git('commit', '-q', '-am', 'other');
    git('checkout', '-q', '-');
    put('src/b.mjs', '// Gives b, this way\nexport const b = 4;\n');
    git('commit', '-q', '-am', 'this');
    assert.throws(() => git('merge', '-q', 'other'), 'the two changes of src/b.mjs conflict');

    assert.equal(codemap().status, 0);
    const map = readFileSync(join(repoDir, '.claude/codemap.md'), 'utf8');
    assert.match(map, /\*\*odd\tname\.mjs\*\* \(2\) — A name with a tab in it/);
    assert.doesNotMatch(map, /\*\*b\.mjs\*\*/, 'a file in conflict waits for its resolution');
    git('merge', '--abort');
});
