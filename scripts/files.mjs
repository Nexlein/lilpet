// The groups of tracked files the formatters and linters work on, in one place for package.json, mise.toml and the
// commit hook (.lintstagedrc.mjs). The globs mean the same to git and to lint-staged: one without a slash takes a file
// name in any folder, dotfiles too; one with a slash is a path, where * stays within a folder; [ch] is one of the letters.
// Usage: node scripts/files.mjs <group>   prints the tracked files of the group, NUL-separated, for xargs -0
import { execFileSync } from 'node:child_process';

export const GROUPS = {
    js: ['*.js', '*.mjs', '*.ts', '*.tsx'],
    python: ['scripts/*.py'],
    // Ours only: in firmware/third_party, the others' sources stay as they are
    c: [
        'engine/include/lilpet/*.h',
        'engine/src/*.[ch]',
        'engine/tests/*.[ch]',
        'engine/tools/*.[ch]',
        'engine/wasm/*.[ch]',
        'firmware/main/*.[ch]',
        'firmware/boards/*/*.h',
        'firmware/tests/*.[ch]',
        'firmware/third_party/*/*.h'
    ],
    // What clang-tidy can analyse with the host build's compile commands: engine/wasm and the firmware's chip code need
    // other compilers
    tidy: ['engine/src/*.c', 'engine/tests/*.c', 'engine/tools/*.c', 'firmware/tests/*.c'],
    workflows: ['.github/workflows/*.yml', '.github/workflows/*.yaml'],
    hooks: ['.husky/*'] // every file there is a shell script
};

// As a git pathspec from the top of the repository, wherever it runs: a name alone is looked for in every folder, as
// lint-staged does
const pathspec = (glob) => `:(top,glob)${glob.includes('/') ? '' : '**/'}${glob}`;

/** The tracked files of a group */
export function trackedFiles(group) {
    const out = execFileSync('git', ['ls-files', '-z', '--', ...GROUPS[group].map(pathspec)], { encoding: 'utf8' });
    return out.split('\0').filter(Boolean);
}

if (import.meta.main) {
    const group = process.argv[2];
    if (!Object.hasOwn(GROUPS, group)) {
        console.error(`Usage: node scripts/files.mjs <${Object.keys(GROUPS).join('|')}>`);
        process.exit(2);
    }
    process.stdout.write(
        trackedFiles(group)
            .map((path) => `${path}\0`)
            .join('')
    );
}
