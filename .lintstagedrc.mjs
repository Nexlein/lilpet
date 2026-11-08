// What the pre-commit hook runs on the staged files (lint-staged, one glob after the other): lint and format each
// file, then check the files generated from the data and the prototype when one of their sources is staged.
export default {
    '*.{js,mjs,ts,tsx}': ['oxlint --fix --deny-warnings', 'prettier --write'],
    // .clang-format-ignore keeps the third-party BMI270 file as it is
    '{engine,firmware/main,firmware/boards}/**/*.{c,h}': 'clang-format -i',
    '*.py': ['ruff check --fix', 'ruff format'],
    '.husky/{pre-commit,pre-push,commit-msg,no-secrets.sh}': 'shellcheck',
    '.github/workflows/*.yml': 'actionlint',
    '!(*.{js,mjs,ts,tsx,c,h,py})': 'prettier --ignore-unknown --write',
    // engine/src/emotions_table.h and engine/tests/js_vectors.h (npm run vectors): the whole check, not per file
    '{eyes/*.json,prototype/eyes.js,prototype/scenes.json,scripts/gen-*.mjs}': () => [
        'npm run check:data',
        'npm run check:vectors'
    ]
};
