// What the pre-commit hook runs on the staged files (lint-staged, one glob after the other): check the data and the
// files generated from it when one of them is staged (with no unstaged change left in them), then lint and format each
// file.
import { EMOTIONS, GENERATORS, LOOKS, SCENES } from './scripts/data.ts';
import { GROUPS } from './scripts/files.ts';

// One lint-staged glob for several (scripts/files.ts)
const anyOf = (globs: readonly string[]) => (globs.length > 1 ? `{${globs.join(',')}}` : globs.join(','));

// The data files, the tables generated from them and their generators (scripts/data.ts), and the scripts they use. The
// validator also reads the engine's names of pupils and blink phases (engine/wasm/engine.ts) and its screen limits
// (screen.h); the scenes table, the fields of a scene (engine.ts too). scripts/files.test.ts checks each one is tracked.
export const DATA = [
    LOOKS,
    EMOTIONS,
    SCENES,
    ...Object.keys(GENERATORS),
    ...Object.values(GENERATORS),
    'scripts/gen-common.ts',
    'scripts/tables.ts',
    'scripts/data.ts',
    'scripts/validate-data.ts',
    'engine/wasm/engine.ts',
    'engine/include/lilpet/screen.h'
];

export default {
    // First, before any task rewrites a staged file: the data, the tables generated from it (npm run tables) and the
    // scripts that read it, the whole check, not per file. These checks read the files on disk, and lint-staged hides
    // only the unstaged changes of the files it is given: a table regenerated but left unstaged would pass while the
    // commit carries the old one. So none of these files may hold unstaged changes (git diff lists them): stage them
    // too, or stash them.
    [`{${DATA.join(',')}}`]: () => [
        'npm run check:data',
        'npm run check:tables',
        `git diff --exit-code --stat -- ${DATA.join(' ')}`
    ],
    [anyOf(GROUPS.ts)]: ['oxlint --fix --deny-warnings', 'prettier --write'],
    [anyOf(GROUPS.c)]: 'clang-format -i',
    [anyOf(GROUPS.python)]: ['ruff check --fix', 'ruff format'],
    [anyOf(GROUPS.hooks)]: 'shellcheck -x', // -x: follows in-mise.sh, which the hooks source
    [anyOf(GROUPS.workflows)]: 'actionlint',
    '!(*.{ts,tsx,c,h,py})': 'prettier --ignore-unknown --write'
};
