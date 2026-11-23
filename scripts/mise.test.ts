// Tests mise.toml: a tool that several tasks pin for themselves, or that a task pins over [tools], has the same version
// in each (ADR 0008), so that flash and coredump, say, never run two esptools; and @types/node follows its Node.
// Renovate or a hand edit could bump one pin and not the other. Built-in test runner, no dependency.
// Usage: npm run check:scripts
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';

// The pins of a TOML text: `tool = "version"`, the tool's name quoted or bare
const pinsOf = (text: string) =>
    [...text.matchAll(/^\s*(?:"([\w:@/.-]+)"|([\w-]+))\s*=\s*"([^"]+)"/gm)].map(([, quoted, bare, version]) => ({
        tool: quoted ?? bare ?? '',
        version: version ?? ''
    }));

test('a tool pinned by several tasks, or by a task and [tools], has one version', () => {
    const mise = readFileSync('mise.toml', 'utf8');
    // The [tools] section, up to the next section, and the tools = { ... } of the tasks, one pin a line
    const toolsSection = /^\[tools\]$(.*?)^\[/ms.exec(mise)?.[1] ?? '';
    const taskPins = [...mise.matchAll(/^tools\s*=\s*\{([^}]*)\}/gm)].map(([, pins = '']) =>
        pins.replaceAll(',', '\n')
    );
    const versions = new Map<string, string[]>();
    for (const { tool, version } of pinsOf([toolsSection, ...taskPins].join('\n')))
        versions.set(tool, [...(versions.get(tool) ?? []), version]);
    assert.ok(
        versions.has('node') && versions.has('pypi:clang-format'),
        'the tools of [tools] are read, bare or quoted'
    );
    const esptool = versions.get('pypi:esptool') ?? [];
    assert.ok(esptool.length >= 2, 'esptool is pinned by firmware:flash and firmware:coredump');
    for (const [tool, pins] of versions)
        assert.equal(new Set(pins).size, 1, `${tool} is pinned at ${[...new Set(pins)].join(' and ')} in mise.toml`);
});

// @types/node describes the API of one Node major: that of mise.toml, which CI and every machine run. Renovate bumps
// one and Dependabot the other (major updates of @types/node are left out, .github/dependabot.yml): they must move
// together.
test('the Node types are those of the Node of mise.toml', () => {
    const node = /^node = "(\d+)\./m.exec(readFileSync('mise.toml', 'utf8'))?.[1];
    const types: string = JSON.parse(readFileSync('package.json', 'utf8')).devDependencies['@types/node'];
    assert.ok(node, 'no node = "<version>" in mise.toml');
    assert.equal(/^\^?(\d+)\./.exec(types)?.[1], node, `@types/node ${types} for Node ${node}`);
});
