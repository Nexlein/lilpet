// Tests .github/workflows/ci.yml against what depends on it. Its board list must be the folders of firmware/boards
// that hold a board.cmake, so that a board added there cannot go unbuilt, and the board table of docs/firmware.md
// lists them. It must run every part of `mise run check`.
// The ruleset of main must require its jobs by their names, and release tags must stay where they were pushed. Built-in
// test runner, no dependency.
// Usage: npm run check:scripts
import assert from 'node:assert/strict';
import { existsSync, readdirSync, readFileSync } from 'node:fs';
import { test } from 'node:test';

const workflow = readFileSync('.github/workflows/ci.yml', 'utf8');
// What follows "jobs:": each job is a key indented by two spaces
const jobsSection = workflow.split(/^jobs:$/m)[1] ?? '';

// The board list of the firmware jobs' matrix
function builtBoards() {
    const list = /^\s+board: \[([^\]]*)\]$/m.exec(workflow)?.[1];
    assert.ok(list !== undefined, 'no "board: [...]" matrix in .github/workflows/ci.yml');
    return list.split(',').map((board) => board.trim());
}

test('the CI builds the firmware of every board, and of nothing else', () => {
    const built = builtBoards();
    const boards = readdirSync('firmware/boards').filter((name) => existsSync(`firmware/boards/${name}/board.cmake`));
    assert.deepEqual(built.toSorted(), boards.toSorted());
});

// docs/adr/0008-outils-figes.md: the CI runs the parts of `mise run check`, each in its own job, instead of copying
// their steps; a part added to the check must not be missing in CI
// Adding a board also means a row in the table of docs/firmware.md, which nothing else would notice missing
test('the board table of docs/firmware.md has every board', () => {
    const table =
        readFileSync('docs/firmware.md', 'utf8')
            .split(/^## Les cartes$/m)[1]
            ?.split(/^## /m)[0] ?? '';
    for (const board of builtBoards()) assert.match(table, new RegExp(`^\\| \`${board}\``, 'm'), `${board} is missing`);
});

test('the CI runs every part of mise run check', () => {
    const task = /^\[tasks\.check\]$[^[]*?^run = \[([^\n]*)\]$/m.exec(readFileSync('mise.toml', 'utf8'))?.[1];
    assert.ok(task !== undefined, 'no "run = [...]" in the [tasks.check] of mise.toml');
    const parts = [...task.matchAll(/\{\s*task\s*=\s*"([^"]+)"\s*\}|"([^"]+)"/g)].map(([, name, command]) =>
        name ? `mise run ${name}` : (command ?? '')
    );
    assert.ok(parts.length > 0);
    // The steps of each job
    const runs = jobsSection
        .split(/^ {2}[\w-]+:/m)
        .map((job) => [...job.matchAll(/^\s+(?:- )?run: (.+)$/gm)].map(([, command = '']) => command.trim()));
    const jobOf = (part: string) => runs.findIndex((commands) => commands.includes(part));
    for (const part of parts) assert.ok(jobOf(part) >= 0, `the CI does not run "${part}"`);
    assert.equal(new Set(parts.map(jobOf)).size, parts.length, 'two parts of mise run check share a CI job');
});

// The part of a ruleset of .github/rulesets this test reads
interface Ruleset {
    name: string;
    target: string;
    conditions: { ref_name: { include: string[] } };
    rules: { type: string; parameters?: { required_status_checks?: { context: string }[] } }[];
}

const readRuleset = (file: string): Ruleset => JSON.parse(readFileSync(`.github/rulesets/${file}`, 'utf8'));

// main is the only branch (docs/adr/0014-une-seule-branche.md), and release tags have their own ruleset
test('the rulesets are those of main and of the release tags', () => {
    assert.deepEqual(readdirSync('.github/rulesets').toSorted(), ['main.json', 'tags.json']);
});

// GitHub names a required check after its job, and a matrix job once per board. A required check that no job reports
// blocks every pull request; a job missing from the ruleset can fail and still be merged. The files reach GitHub
// through `mise run github:rulesets`, so this checks what GitHub enforces as long as they are kept applied.
test('the ruleset of main requires every CI job, and nothing else', () => {
    const names = [...jobsSection.matchAll(/^ {4}name: (.+)$/gm)].map(([, name = '']) => name);
    assert.equal(names.length, [...jobsSection.matchAll(/^ {2}[\w-]+:$/gm)].length, 'a job of ci.yml has no name');
    const checks = names.flatMap((name) =>
        name.includes('${{ matrix.board }}')
            ? builtBoards().map((board) => name.replace('${{ matrix.board }}', board))
            : [name]
    );
    const ruleset = readRuleset('main.json');
    assert.equal(ruleset.target, 'branch');
    assert.deepEqual(ruleset.conditions.ref_name.include, ['refs/heads/main']);
    const required = ruleset.rules
        .find(({ type }) => type === 'required_status_checks')
        ?.parameters?.required_status_checks?.map(({ context }) => context);
    assert.deepEqual(required?.toSorted(), checks.toSorted());
    for (const type of ['deletion', 'non_fast_forward', 'pull_request', 'code_scanning'])
        assert.ok(
            ruleset.rules.some((rule) => rule.type === type),
            `main.json lacks the ${type} rule`
        );
});

// A release tag names the commit its Release and its firmware zips were built from: moved, it would name another one
test('a release tag can be neither moved nor deleted', () => {
    const ruleset = readRuleset('tags.json');
    assert.equal(ruleset.target, 'tag');
    assert.deepEqual(ruleset.conditions.ref_name.include, ['refs/tags/v*']);
    assert.deepEqual(ruleset.rules.map(({ type }) => type).toSorted(), ['deletion', 'non_fast_forward', 'update']);
});
