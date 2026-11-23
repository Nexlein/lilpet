// Tests that the numbers the docs give a reader are the code's: each case reads a number in a doc and the constant it
// stands for, so that a constant changed in the code fails here until the doc says the same. Also that the protocol's
// doc has every command. Built-in test runner, no
// dependency.
// Usage: npm run check:scripts
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';

const WORDS: Record<string, number> = { deux: 2, trois: 3, quatre: 4, cinq: 5 };
const read = (path: string) => readFileSync(path, 'utf8');
// A text as it reads, inside a regular expression
const literal = (text: string) => text.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
// A number in a text, found by a pattern whose spaces may be any line break and indent
function numberIn(path: string, words: string) {
    const pattern = new RegExp(literal(words).replace(/ /g, '\\s+').replace('N', '(\\d+(?:,\\d+)?|\\w+)'));
    const found = pattern.exec(read(path))?.[1];
    assert.ok(found, `${path}: "${words}" not found`);
    return WORDS[found] ?? Number(found.replace(',', '.')); // a French decimal: 0,5
}
const codeNumber = (path: string, pattern: RegExp) => {
    const found = pattern.exec(read(path))?.[1];
    assert.ok(found, `${path}: ${pattern} not found`);
    return Number(found);
};

const CLANG = codeNumber('engine/CMakeLists.txt', /set\(LILPET_MIN_CLANG (\d+)\)/);
const WATCHDOG = codeNumber('firmware/sdkconfig.defaults', /^CONFIG_ESP_TASK_WDT_TIMEOUT_S=(\d+)$/m);
// The rest and the battery (docs/firmware.md), and the engine's timings that a camera model must know (protocole.md):
// the bench day (#53) may change the values marked proposed
const REST_US = (name: string) => codeNumber('firmware/main/rest.h', new RegExp(`#define ${name}\\s+(\\d+)`));
const ENGINE_SECONDS = (path: string, name: string) => codeNumber(path, new RegExp(`#define ${name}\\s+([\\d.]+)f`));
const CHARGE_MA = codeNumber('firmware/boards/waveshare_175/board_config.h', /#define BOARD_CHARGE_MA (\d+)/);
const CASES: [path: string, words: string, value: number][] = [
    ['docs/firmware.md', 'après N min sans commande', REST_US('REST_IDLE_US') / 60e6],
    ['docs/firmware.md', "l'air ensommeillé N s", REST_US('REST_DROWSY_US') / 1e6],
    ['docs/firmware.md', 'en fondu sur les N dernières', REST_US('REST_FADE_OUT_US') / 1e6],
    ['docs/firmware.md', "l'écran en fondu (N s)", REST_US('REST_FADE_IN_US') / 1e6],
    ['docs/firmware.md', 'ensommeillés N s avant leur humeur', REST_US('REST_WAKING_US') / 1e6],
    ['docs/firmware.md', 'Sous N V', codeNumber('firmware/main/battery_rule.h', /#define BATTERY_LOW_MV (\d+)/) / 1000],
    [
        'docs/firmware.md',
        "jusqu'à N V",
        codeNumber('firmware/main/battery_rule.h', /#define BATTERY_OK_MV\s+(\d+)/) / 1000
    ],
    [
        'docs/firmware.md',
        '`BOARD_CHARGE_MA` (N mA)',
        codeNumber('firmware/boards/waveshare_175/board_config.h', /#define BOARD_CHARGE_MA (\d+)/)
    ],
    // The same values, copied where a decision or a plan quotes them (ADR 0012, objet-v1.md, materiel.md, CLAUDE.md)
    ['docs/adr/0012-energie.md', 'après N min sans rien', REST_US('REST_IDLE_US') / 60e6],
    ['docs/adr/0012-energie.md', 'pendant N s, l', REST_US('REST_DROWSY_US') / 1e6],
    ['docs/adr/0012-energie.md', 'en fondu sur les N dernières', REST_US('REST_FADE_OUT_US') / 1e6],
    ['docs/adr/0012-energie.md', 'en fondu (N s)', REST_US('REST_FADE_IN_US') / 1e6],
    ['docs/adr/0012-energie.md', 'ensommeillés N s avant', REST_US('REST_WAKING_US') / 1e6],
    [
        'docs/adr/0012-energie.md',
        'Sous N V',
        codeNumber('firmware/main/battery_rule.h', /#define BATTERY_LOW_MV (\d+)/) / 1000
    ],
    [
        'docs/adr/0012-energie.md',
        'au-dessus de N V',
        codeNumber('firmware/main/battery_rule.h', /#define BATTERY_OK_MV\s+(\d+)/) / 1000
    ],
    ['docs/objet-v1.md', 'après N min sans toucher', REST_US('REST_IDLE_US') / 60e6],
    ['docs/objet-v1.md', 'courant de charge sûr : **N mA', CHARGE_MA],
    ['docs/materiel.md', 'règle la charge à N mA', CHARGE_MA],
    ['docs/materiel.md', 'que le vendeur donne : **N mA', CHARGE_MA],
    ['CLAUDE.md', '`BOARD_CHARGE_MA` (N mA', CHARGE_MA],
    ['docs/protocole.md', 'au plus toutes les N s, quelle', ENGINE_SECONDS('engine/src/eyes.c', 'BURST_GAP')],
    ['docs/protocole.md', 'sans nouvelle position pendant N s', ENGINE_SECONDS('engine/src/senses.c', 'FACE_LOST')],
    ['docs/protocole.md', 'absent depuis N s', ENGINE_SECONDS('engine/src/senses.c', 'FACE_NEW')],
    ['docs/protocole.md', '(N s) plus tard', WATCHDOG],
    ['docs/firmware.md', 'qui ne finit pas en N s', WATCHDOG],
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
    [
        'README.md',
        'après N redémarrages en pleine lecture',
        codeNumber('scripts/check-device.py', /^REBOOTS_DURING_READS = (\d+)$/m)
    ],
    ['CLAUDE.md', "older than CI's (N)", CLANG],
    ['docs/adr/0008-outils-figes.md', 'de la CI (N)', CLANG]
];

for (const [path, words, value] of CASES)
    test(`${path}: "${words.replace('N', String(value))}"`, () => assert.equal(numberIn(path, words), value));

// Every command the engine reads, and the firmware's own test commands, is in the protocol a camera model or the app is
// written from: one added to the code and not to docs/protocole.md would not exist for them
test('docs/protocole.md has every command', () => {
    const protocol = read('docs/protocole.md');
    const engine = [...read('engine/src/command.c').matchAll(/strcmp\(name, "([^"]+)"\)/g)].map(
        ([, name = '']) => name
    );
    const firmware = [...read('firmware/main/commands.c').matchAll(/"> (test \w+)"/g)].map(([, name = '']) => name);
    assert.ok(engine.length >= 7 && firmware.length >= 2, 'the commands were not found in the code');
    // The name and what follows it in the doc: `> look ...` and `> look?` are two commands
    for (const name of [...engine, ...firmware])
        assert.match(protocol, new RegExp(`\`> ${literal(name)}[\` ]`), `> ${name} is missing`);
});
