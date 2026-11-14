// Tests that the CI builds every firmware board: the board list of .github/workflows/ci.yml must be the folders of
// firmware/boards that hold a board.cmake, so that a board added there cannot go unbuilt. And that it runs every part
// of `mise run check`. Built-in test runner, no dependency.
// Usage: npm run check:scripts
import assert from 'node:assert/strict';
import { existsSync, readdirSync, readFileSync } from 'node:fs';
import { test } from 'node:test';

test('the CI builds the firmware of every board, and of nothing else', () => {
    const workflow = readFileSync('.github/workflows/ci.yml', 'utf8');
    const list = /^\s+board: \[([^\]]*)\]$/m.exec(workflow)?.[1];
    assert.ok(list !== undefined, 'no "board: [...]" matrix in .github/workflows/ci.yml');
    const built = list.split(',').map((board) => board.trim());
    const boards = readdirSync('firmware/boards').filter((name) => existsSync(`firmware/boards/${name}/board.cmake`));
    assert.deepEqual(built.toSorted(), boards.toSorted());
});

// docs/adr/0008-outils-figes.md: the CI runs the parts of `mise run check`, each in its own job, instead of copying
// their steps; a part added to the check must not be missing in CI
test('the CI runs every part of mise run check', () => {
    const task = /^\[tasks\.check\]$[^[]*?^run = \[([^\n]*)\]$/m.exec(readFileSync('mise.toml', 'utf8'))?.[1];
    assert.ok(task !== undefined, 'no "run = [...]" in the [tasks.check] of mise.toml');
    const parts = [...task.matchAll(/\{\s*task\s*=\s*"([^"]+)"\s*\}|"([^"]+)"/g)].map(([, name, command]) =>
        name ? `mise run ${name}` : command
    );
    assert.ok(parts.length > 0);
    // The steps of each job: the jobs are the keys indented by two spaces under "jobs:"
    const jobs =
        readFileSync('.github/workflows/ci.yml', 'utf8')
            .split(/^jobs:$/m)[1]
            ?.split(/^ {2}[\w-]+:/m) ?? [];
    const runs = jobs.map((job) => [...job.matchAll(/^\s+(?:- )?run: (.+)$/gm)].map(([, command]) => command.trim()));
    const jobOf = (part) => runs.findIndex((commands) => commands.includes(part));
    for (const part of parts) assert.ok(jobOf(part) >= 0, `the CI does not run "${part}"`);
    assert.equal(new Set(parts.map(jobOf)).size, parts.length, 'two parts of mise run check share a CI job');
});
