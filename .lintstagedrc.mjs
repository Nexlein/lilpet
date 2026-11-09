// What the pre-commit hook runs on the staged files (lint-staged, one glob after the other): lint and format each
// file, then check the files generated from the data when one of their sources is staged.
// The validator also reads the engine's names of pupils and blink phases (engine/wasm/engine.mjs)
const DATA = [
    'eyes/*.json',
    'engine/tests/scenes.json',
    'engine/src/emotions_table.h',
    'engine/tests/scenes_table.h',
    'scripts/gen-*.mjs',
    'scripts/expression.mjs',
    'scripts/data.mjs',
    'scripts/validate-data.mjs',
    'engine/wasm/engine.mjs'
];

export default {
    '*.{js,mjs,ts,tsx}': ['oxlint --fix --deny-warnings', 'prettier --write'],
    // firmware/third_party keeps others' files as they are: only our headers there are formatted
    '{engine,firmware/main,firmware/boards}/**/*.{c,h}': 'clang-format -i',
    'firmware/third_party/**/*.h': 'clang-format -i',
    '*.py': ['ruff check --fix', 'ruff format'],
    // the git hooks: every file of .husky is a shell script
    '.husky/*': 'shellcheck',
    '.github/workflows/*.yml': 'actionlint',
    '!(*.{js,mjs,ts,tsx,c,h,py})': 'prettier --ignore-unknown --write',
    // The data, the tables generated from it (npm run tables) and the scripts that read it: the whole check, not per file
    [`{${DATA.join(',')}}`]: () => ['npm run check:data', 'npm run check:tables']
};
