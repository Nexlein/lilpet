// Tests that the numbers the docs give a reader are the code's: each case reads a number in a doc and the constant it
// stands for, so that a constant changed in the code fails here until the doc says the same. Built-in test runner, no
// dependency.
// Usage: npm run check:scripts
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';

const WORDS = { deux: 2, trois: 3, quatre: 4, cinq: 5 };
const read = (path) => readFileSync(path, 'utf8');
// A number in a text, found by a pattern whose spaces may be any line break and indent
function numberIn(path, words) {
    const pattern = new RegExp(
        words
            .replace(/[.()`]/g, '\\$&')
            .replace(/ /g, '\\s+')
            .replace('N', '(\\d+|\\w+)')
    );
    const found = pattern.exec(read(path))?.[1];
    assert.ok(found, `${path}: "${words}" not found`);
    return WORDS[found] ?? Number(found);
}
const codeNumber = (path, pattern) => {
    const found = pattern.exec(read(path))?.[1];
    assert.ok(found, `${path}: ${pattern} not found`);
    return Number(found);
};

const CLANG = codeNumber('engine/CMakeLists.txt', /set\(LILPET_MIN_CLANG (\d+)\)/);
const CASES = [
    [
        'docs/protocole.md',
        'au plus N caractères',
        codeNumber('engine/include/lilpet/lilpet.h', /#define LP_COMMAND_LINE_MAX (\d+)/)
    ],
    [
        'docs/protocole.md',
        '(N commandes entre deux images)',
        codeNumber('firmware/main/commands.c', /#define QUEUE_SLOTS\s+(\d+)/)
    ],
    [
        'docs/protocole.md',
        'au plus toutes les N s',
        codeNumber('firmware/main/look_saving.h', /#define LOOK_SAVE_GAP_US (\d+)/) / 1e6
    ],
    [
        'docs/protocole.md',
        'Elle attend N s la réponse à un look',
        codeNumber('app/src/device/connection.ts', /timeoutMs = (\d+)/) / 1000
    ],
    ['docs/protocole.md', 'N ms pour le reste', codeNumber('app/src/device/device.ts', /COMMAND_TIMEOUT_MS = (\d+)/)],
    [
        'docs/protocole.md',
        'N s pour `hello`',
        codeNumber('app/src/device/device.ts', /HELLO_TIMEOUT_MS = (\d+)/) / 1000
    ],
    ['docs/protocole.md', "qu'elle pose N fois", codeNumber('app/src/device/device.ts', /HELLO_ATTEMPTS = (\d+)/)],
    ['README.md', 'clang N ou plus récent', CLANG],
    ['CLAUDE.md', "older than CI's (N)", CLANG],
    ['docs/adr/0008-outils-figes.md', 'de la CI (N)', CLANG]
];

for (const [path, words, value] of CASES)
    test(`${path}: "${words.replace('N', value)}"`, () => assert.equal(numberIn(path, words), value));
