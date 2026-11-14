// Tests that the CI builds every firmware board: the board list of .github/workflows/ci.yml must be the folders of
// firmware/boards that hold a board.cmake, so that a board added there cannot go unbuilt. Built-in test runner, no
// dependency.
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
