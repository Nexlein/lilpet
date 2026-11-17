// Tests mise.toml: a tool that several tasks pin for themselves has the same version in each (ADR 0008), so that flash
// and coredump, say, never run two esptools. Renovate or a hand edit could bump one pin and not the other. Built-in
// test runner, no dependency.
// Usage: npm run check:scripts
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';

test('a tool pinned by several tasks has one version', () => {
    const versions = new Map();
    for (const [, tool, version] of readFileSync('mise.toml', 'utf8').matchAll(/"([\w:@/.-]+)"\s*=\s*"([^"]+)"/g))
        versions.set(tool, [...(versions.get(tool) ?? []), version]);
    const esptool = versions.get('pypi:esptool') ?? [];
    assert.ok(esptool.length >= 2, 'esptool is pinned by firmware:flash and firmware:coredump');
    for (const [tool, pins] of versions)
        assert.equal(new Set(pins).size, 1, `${tool} is pinned at ${[...new Set(pins)].join(' and ')} in mise.toml`);
});
