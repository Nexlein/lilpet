// Tests .github/workflows/ci.yml against what depends on it. Its board list must be the folders of firmware/boards
// that hold a board.cmake, so that a board added there cannot go unbuilt, and the board table of docs/firmware.md
// lists them. It must run every part of `mise run check`.
// The rulesets of .github/rulesets must require its jobs by their names. Built-in test runner, no dependency.
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
        name ? `mise run ${name}` : command
    );
    assert.ok(parts.length > 0);
    // The steps of each job
    const runs = jobsSection
        .split(/^ {2}[\w-]+:/m)
        .map((job) => [...job.matchAll(/^\s+(?:- )?run: (.+)$/gm)].map(([, command]) => command.trim()));
    const jobOf = (part) => runs.findIndex((commands) => commands.includes(part));
    for (const part of parts) assert.ok(jobOf(part) >= 0, `the CI does not run "${part}"`);
    assert.equal(new Set(parts.map(jobOf)).size, parts.length, 'two parts of mise run check share a CI job');
});

// GitHub names a required check after its job, and a matrix job once per board. A required check that no job reports
// blocks every pull request; a job missing from the rulesets can fail and still be merged. The files reach GitHub
// through `mise run github:rulesets`, so this checks what GitHub enforces as long as they are kept applied.
test('the rulesets of dev and main require every CI job, and nothing else', () => {
    const names = [...jobsSection.matchAll(/^ {4}name: (.+)$/gm)].map(([, name]) => name);
    assert.equal(names.length, [...jobsSection.matchAll(/^ {2}[\w-]+:$/gm)].length, 'a job of ci.yml has no name');
    const checks = names.flatMap((name) =>
        name.includes('${{ matrix.board }}')
            ? builtBoards().map((board) => name.replace('${{ matrix.board }}', board))
            : [name]
    );
    assert.deepEqual(readdirSync('.github/rulesets').toSorted(), ['dev.json', 'main.json']);
    const rulesets = ['dev', 'main'].map((branch) => {
        const ruleset = JSON.parse(readFileSync(`.github/rulesets/${branch}.json`, 'utf8'));
        assert.deepEqual(ruleset.conditions.ref_name.include, [`refs/heads/${branch}`]);
        const required = ruleset.rules
            .find(({ type }) => type === 'required_status_checks')
            ?.parameters.required_status_checks.map(({ context }) => context);
        assert.deepEqual(required?.toSorted(), checks.toSorted(), `${branch}.json`);
        // What may differ between the two: the name (which github:rulesets matches) and the branch
        const { name, conditions: _branch, ...rules } = ruleset;
        assert.ok(name.startsWith(`${branch}:`), `${branch}.json: a name of its own, starting with "${branch}:"`);
        return rules;
    });
    assert.deepEqual(rulesets[0], rulesets[1], 'dev.json and main.json differ in more than their name and branch');
});
