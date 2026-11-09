// Tests of engine/wasm/engine.mjs that need no WebAssembly build: its own lists of the engine's names (pupil types,
// blink phases) must follow the order of their enums in lilpet.h, since the module gets their index. A name inserted
// in C and not here would draw one pupil for another, and the data validator, which reads these lists, would agree.
// Built-in test runner, no dependency.
// Usage: npm run check:scripts
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';
import { BLINK_PHASES, PUPILS } from './engine.mjs';

const HEADER = readFileSync(new URL('../include/lilpet/lilpet.h', import.meta.url), 'utf8');

// The enumerators of `typedef enum { ... } name;`, in order, without the prefix and the COUNT one
function enumerators(name, prefix) {
    const body = HEADER.match(new RegExp(`typedef enum \\{([^}]*)\\} ${name};`))?.[1];
    assert.ok(body, `enum ${name} not found in lilpet.h`);
    return body
        .replace(/\/\*[\s\S]*?\*\//g, '')
        .split(',')
        .map((e) => e.trim().split(/\s*=/)[0])
        .filter((e) => e && e !== `${prefix}COUNT`)
        .map((e) => e.slice(prefix.length).toLowerCase());
}

test('pupil types in the order of lp_pupil', () => {
    assert.deepEqual(PUPILS, enumerators('lp_pupil', 'LP_PUPIL_'));
});

test('blink phases in the order of lp_blink_phase, with no phase for none', () => {
    assert.deepEqual(
        BLINK_PHASES,
        enumerators('lp_blink_phase', 'LP_BLINK_').map((p) => (p === 'none' ? null : p))
    );
});
