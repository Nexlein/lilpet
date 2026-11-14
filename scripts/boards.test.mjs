// Tests that the boards' facts and the firmware agree on their names. An #ifdef only asks whether a name exists, so a
// misspelt one builds without a word: each BOARD_* a board's board_config.h defines must be read by the firmware or
// the board scripts, and each BOARD_* the firmware tests with #ifdef must be defined by a board. Without
// BOARD_POWER_HOLD, for one, the Waveshare would turn off on battery. Built-in test runner, no dependency.
// Usage: npm run check:scripts
import assert from 'node:assert/strict';
import { existsSync, readdirSync, readFileSync } from 'node:fs';
import { test } from 'node:test';

const BOARDS = readdirSync('firmware/boards').filter((board) => existsSync(`firmware/boards/${board}/board.cmake`));
const DEFINE = /^#define (BOARD_\w+)/gm;

// The text of the files of a folder that end with one of `extensions`
const sources = (folder, extensions) =>
    readdirSync(folder)
        .filter((name) => extensions.some((extension) => name.endsWith(extension)))
        .map((name) => readFileSync(`${folder}/${name}`, 'utf8'));
// C without its comments: a name in a comment reads nothing
const withoutComments = (c) => c.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/.*$/gm, '');

// What reads the boards' facts: the firmware, its tests, and the board scripts (check-device.py reads BOARD_BENCH)
const FIRMWARE = [...sources('firmware/main', ['.c', '.h']), ...sources('firmware/tests', ['.c'])].map(withoutComments);
const READERS = [...FIRMWARE, ...sources('scripts', ['.py'])].join('\n');

test('every fact a board defines is read', () => {
    for (const board of BOARDS) {
        const header = readFileSync(`firmware/boards/${board}/board_config.h`, 'utf8');
        for (const [, name] of header.matchAll(DEFINE))
            assert.match(READERS, new RegExp(`\\b${name}\\b`), `${board}: ${name} is read nowhere: misspelt?`);
    }
});

test('every fact the firmware asks about is defined by a board', () => {
    const defined = new Set();
    for (const board of BOARDS)
        for (const [, name] of readFileSync(`firmware/boards/${board}/board_config.h`, 'utf8').matchAll(DEFINE))
            defined.add(name);
    // #ifdef, #ifndef, and #if or #elif with a value or defined(): an unknown name is not defined, or 0, without a word
    for (const [line] of FIRMWARE.join('\n').matchAll(/^\s*#\s*(?:if|elif|ifdef|ifndef)\b.*$/gm))
        for (const [name] of line.matchAll(/\bBOARD_\w+/g))
            assert.ok(defined.has(name), `the firmware asks about ${name}, which no board defines: misspelt?`);
});
