// Tests the boards' flash layout, firmware/partitions.csv (docs/adr/0011-disposition-de-la-flash.md): nvs stays where
// ESP-IDF's default table put it, so that the look saved there survives a change of the table, and every partition
// comes after the one before, without overlapping it. Built-in test runner, no dependency.
// Usage: npm run check:scripts
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';

const partitions = readFileSync('firmware/partitions.csv', 'utf8')
    .split('\n')
    .filter((line) => line.trim() && !line.startsWith('#'))
    .map((line) => {
        const [name, type, subtype, offset, size] = line.split(',').map((field) => field.trim());
        return { name, type, subtype, offset: Number(offset), size: Number(size) };
    });

test('nvs stays at 0x9000 with 0x6000, where the default table put it', () => {
    const nvs = partitions.find(({ subtype }) => subtype === 'nvs');
    assert.deepEqual([nvs?.name, nvs?.offset, nvs?.size], ['nvs', 0x9000, 0x6000]);
});

test('every partition has a written offset and size, after the one before', () => {
    let end = 0;
    for (const { name, offset, size } of partitions) {
        assert.ok(Number.isInteger(offset) && Number.isInteger(size) && size > 0, `${name}: offset and size in hex`);
        assert.ok(offset >= end, `${name} starts before the end of the partition above it`);
        end = offset + size;
    }
    assert.ok(end <= 8 * 1024 * 1024, 'the layout fits in 8 MB of flash');
});

test('the core dump has its partition, which firmware:coredump reads', () => {
    assert.ok(partitions.some(({ name, subtype }) => name === 'coredump' && subtype === 'coredump'));
});
