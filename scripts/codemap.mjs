// Code map for AI assistants and new contributors: every source file with its purpose, exports, local imports and the
// files that use it, the data and who reads it, the commands and the npm packages, in one Markdown file
// (.claude/codemap.md). It reads the files as git has them staged, so the map always matches what gets committed.
// Usage: npm run codemap (the pre-commit hook runs it). With --check it writes nothing and fails if the map is out of
// date, if a local import does not resolve or if a source file says nothing about its purpose (npm run check, CI).
import { execFileSync } from 'node:child_process';
import { mkdirSync, writeFileSync } from 'node:fs';
import { posix } from 'node:path';

export const OUT = '.claude/codemap.md';

const { basename, dirname, join, normalize } = posix;

// The same order on every machine (localeCompare would follow the locale's collation)
const byCodeUnit = (a, b) => (a < b ? -1 : a > b ? 1 : 0);
const escapeRe = (text) => text.replace(/[.*+?^${}()|[\]\\/]/g, '\\$&');

// --- What a file is ----------------------------------------------------------------------------------------------

const KINDS = [
    [/\.[ch]$/, 'c'],
    [/\.(tsx?|mts|mjs|js)$/, 'js'],
    [/\.py$/, 'py'],
    [/(^|\/)CMakeLists\.txt$|\.cmake$/, 'cmake'],
    [/\.html$/, 'html'],
    [/\.css$/, 'css'],
    [/(^|\/)Dockerfile$|\.sh$|^\.husky\/[^/.]+$/, 'sh']
];
// JSON files that hold settings, not data the code reads
const CONFIG_JSON = /(^|\/)(package(-lock)?|tsconfig[^/]*|CMakePresets)\.json$|^\.(vscode|claude)\/|^\.[^/]+rc\.json$/;

export function kindOf(path) {
    if (path.endsWith('.json')) return CONFIG_JSON.test(path) ? null : 'data';
    return KINDS.find(([re]) => re.test(path))?.[1] ?? null;
}

export const isTest = (path) =>
    /(^|\/)tests?\//.test(path) || /\.(test|spec)\.[cm]?[jt]sx?$/.test(path) || /(^|\/)test_[^/]+$/.test(path);

// --- Comments -----------------------------------------------------------------------------------------------------

const SLASH_COMMENT = /^\s*(?:\/\*([\s\S]*?)\*\/|((?:[ \t]*\/\/.*\n?)+))/;
const HASH_COMMENT = /^\s*((?:[ \t]*#.*\n?)+)/;
const LEADING_COMMENT = {
    c: SLASH_COMMENT,
    js: SLASH_COMMENT,
    css: /^\s*\/\*([\s\S]*?)\*\//,
    py: /^\s*(?:"""([\s\S]*?)"""|'''([\s\S]*?)'''|((?:[ \t]*#.*\n?)+))/,
    cmake: HASH_COMMENT,
    sh: HASH_COMMENT
};

/** The comments at the top of a file, in order (a licence first, then the file's own description). */
function leadingComments(src, kind) {
    let s = src.replace(/^\uFEFF/, '').replace(/^#!.*\n/, '');
    if (kind === 'html') return [...s.slice(0, 4000).matchAll(/<!--([\s\S]*?)-->/g)].map((m) => m[1]);
    if (kind === 'py') s = s.replace(/^# \/\/\/ script[\s\S]*?^# \/\/\/\n/m, '');
    const comments = [];
    for (;;) {
        const m = s.match(LEADING_COMMENT[kind]);
        if (!m) return comments;
        comments.push(m.slice(1).find((g) => g !== undefined) ?? '');
        s = s.slice(m[0].length);
    }
}

/** The text of a file without its comments, strings kept: what the code itself names. */
export function withoutComments(src, kind) {
    if (kind === 'c' || kind === 'js' || kind === 'css')
        return src.replace(/\/\*[\s\S]*?\*\//g, '').replace(/(^|[^:'"`\\])\/\/.*$/gm, '$1');
    if (kind === 'py' || kind === 'sh' || kind === 'cmake') return src.replace(/^[ \t]*#.*$/gm, '');
    if (kind === 'html') return src.replace(/<!--[\s\S]*?-->/g, '');
    return src;
}

// --- Purpose: the first sentence of the file's first comment ------------------------------------------------------

const LICENSE = /^\s*(copyright|spdx|licen[cs]e|\(c\))/i;
const ABBREVIATION = /\b(e\.g|i\.e|etc|vs|cf)\.$/i;
const PURPOSE_MAX = 150;

export function firstSentence(text) {
    const flat = text
        .replace(/^[ \t]*(\*|\/\/|#)[ \t]?/gm, '')
        .replace(/\s+/g, ' ')
        .trim();
    const end = [...flat.matchAll(/[.!?](?=\s|$)/g)].find((m) => !ABBREVIATION.test(flat.slice(0, m.index + 1)));
    const first = end ? flat.slice(0, end.index + 1) : flat;
    return first.length > PURPOSE_MAX ? `${first.slice(0, PURPOSE_MAX - 3)}...` : first;
}

export function purposeOf(src, kind) {
    if (kind === 'data') return '';
    const own = leadingComments(src, kind).find((c) => c.trim() && !LICENSE.test(firstSentence(c)));
    return own ? firstSentence(own) : '';
}

// --- Symbols ------------------------------------------------------------------------------------------------------

function stripC(src) {
    // Comments and string contents out, line breaks kept
    return src
        .replace(/\/\*[\s\S]*?\*\//g, (c) => c.replace(/[^\n]/g, ' '))
        .replace(/\/\/.*$/gm, '')
        .replace(/"(?:\\.|[^"\\\n])*"/g, '""')
        .replace(/'(?:\\.|[^'\\\n])*'/g, "''");
}

/** Functions, types and macros a C header declares, or the functions a C source defines (static ones excluded). */
export function cSymbols(src, header) {
    // The extern "C" wrapper for C++ would put every declaration inside braces
    const code = stripC(src).replace(/^[ \t]*#[ \t]*ifdef[ \t]+__cplusplus\b[\s\S]*?^[ \t]*#[ \t]*endif.*$/gm, '');
    const names = [];
    if (header) {
        for (const m of code.matchAll(/^[ \t]*#[ \t]*define[ \t]+([A-Z][A-Z0-9_]*)\b/gm))
            if (!/_H_?$/.test(m[1])) names.push(m[1]);
    }
    const flat = code.replace(/^[ \t]*#.*(?:\\\n.*)*$/gm, ' ');
    let depth = 0;
    let statement = ''; // what is written at the top level since the last `;` or `{`
    let beforeBraces = ''; // the statement before the braces just closed, e.g. "typedef struct"
    for (const ch of flat) {
        if (ch === '{') {
            if (depth === 0) {
                const fn = statement.match(/\b([A-Za-z_]\w*)\s*\([^;{}]*\)\s*$/);
                if (!header && fn && !/\bstatic\b/.test(statement) && !/[=]/.test(statement)) names.push(fn[1]);
                beforeBraces = statement;
                statement = '';
            }
            depth++;
        } else if (ch === '}') {
            depth = Math.max(0, depth - 1);
            // A function body ends there; only a type definition goes on to its name and `;`
            if (depth === 0 && !/^\s*(typedef|struct|enum|union)\b/.test(beforeBraces)) beforeBraces = '';
        } else if (depth === 0) {
            if (ch === ';') {
                const whole = `${beforeBraces} ${statement}`.replace(/\s+/g, ' ').trim();
                if (header && /^typedef\b/.test(whole)) {
                    const name =
                        whole.match(/\(\s*\*\s*([A-Za-z_]\w*)\s*\)/)?.[1] ?? whole.match(/([A-Za-z_]\w*)\s*$/)?.[1];
                    if (name) names.push(name);
                } else if (header && !beforeBraces) {
                    const fn = statement.match(/\b([A-Za-z_]\w*)\s*\([^;{}]*\)\s*$/s);
                    if (fn && !/\b(static|typedef)\b/.test(statement) && !/\(\s*\*/.test(statement)) names.push(fn[1]);
                }
                statement = '';
                beforeBraces = '';
            } else statement += ch;
        }
    }
    return [...new Set(names)];
}

export function jsExports(src) {
    const code = withoutComments(src, 'js');
    const names = [];
    for (const m of code.matchAll(
        /^[ \t]*export\s+(?:declare\s+)?(?:default\s+)?(?:async\s+)?(?:function\*?|class|const|let|var|interface|type|enum|abstract\s+class)\s+([A-Za-z_$][\w$]*)/gm
    ))
        names.push(m[1]);
    for (const m of code.matchAll(/^[ \t]*export\s+(?:type\s+)?\{([^}]*)\}/gm))
        for (const part of m[1].split(',')) {
            const name = part
                .trim()
                .split(/\s+as\s+/)
                .pop();
            if (name) names.push(name.replace(/^type\s+/, ''));
        }
    if (/^[ \t]*export\s+default\s+(?!(?:async\s+)?(?:function|class)\s+[A-Za-z_$])/m.test(code)) names.push('default');
    return [...new Set(names)];
}

/** Every module specifier a JS or TS file loads, static or dynamic. */
export function jsSpecifiers(src) {
    const code = withoutComments(src, 'js');
    const res = [
        // Static imports and re-exports start a line (Prettier keeps them so); a string that only looks like one does not
        /^[ \t]*(?:import|export)\b[^'"`;]*?\bfrom\s*['"]([^'"]+)['"]/gm,
        /^[ \t]*import\s*['"]([^'"]+)['"]/gm,
        /\bimport\(\s*['"]([^'"]+)['"]\s*\)/g,
        /\bnew URL\(\s*['"]([^'"]+)['"]\s*,\s*import\.meta\.url/g
    ];
    return [...new Set(res.flatMap((re) => [...code.matchAll(re)].map((m) => m[1])))];
}

export function pySymbols(src) {
    return [...src.matchAll(/^(?:async\s+)?(?:def|class)\s+([A-Za-z]\w*)/gm)].map((m) => m[1]);
}

export function pyModules(src) {
    return [
        ...[...src.matchAll(/^\s*import\s+([\w.]+(?:\s*,\s*[\w.]+)*)/gm)].flatMap((m) => m[1].split(/\s*,\s*/)),
        ...[...src.matchAll(/^\s*from\s+([\w.]+)\s+import\b/gm)].map((m) => m[1])
    ];
}

function symbolsOf(path, src, kind) {
    if (kind === 'c') return cSymbols(src, path.endsWith('.h'));
    if (kind === 'js') return jsExports(src);
    if (kind === 'py') return pySymbols(src);
    return [];
}

// --- Resolving what a file uses -----------------------------------------------------------------------------------

const JS_EXTENSIONS = ['', '.ts', '.tsx', '.mts', '.mjs', '.js', '.json', '.d.mts'];

/** The files of the repository, and the git rule for a path that is not one of them. */
export function repository(files, isIgnored = () => false) {
    return { files, known: new Set(files), isIgnored };
}

function resolveRelative(from, spec, repo) {
    const base = spec.startsWith('/') ? join(dirname(from), spec.slice(1)) : normalize(join(dirname(from), spec));
    const candidates = [
        ...JS_EXTENSIONS.map((e) => base + e),
        ...['ts', 'tsx', 'mjs', 'js'].map((e) => `${base}/index.${e}`)
    ];
    return candidates.find((c) => repo.known.has(c));
}

/** A C include or a CMake file by the end of its path ("lilpet/lilpet.h"): the one file, or each board's. */
function resolveBySuffix(spec, from, repo) {
    const near = normalize(join(dirname(from), spec));
    if (repo.known.has(near)) return [near];
    const hits = repo.files.filter((f) => f === spec || f.endsWith(`/${spec}`));
    if (hits.length <= 1) return hits;
    const sameTop = hits.filter((h) => h.split('/')[0] === from.split('/')[0]);
    return sameTop.length ? sameTop : hits;
}

const packageOf = (spec) =>
    spec.startsWith('node:') ? null : spec.startsWith('@') ? spec.split('/').slice(0, 2).join('/') : spec.split('/')[0];

/** The local files a file uses, the npm packages it imports, the build outputs it loads and what does not resolve. */
export function usesOf(file, src, kind, repo) {
    const local = [];
    const packages = [];
    const unresolved = [];
    const built = [];
    const relative = (spec) => {
        const hit = resolveRelative(file, spec, repo);
        const path = normalize(join(dirname(file), spec.startsWith('/') ? spec.slice(1) : spec));
        if (hit) local.push(hit);
        else if (repo.isIgnored(path))
            built.push(path); // a build output, absent until built
        else unresolved.push(spec);
    };
    if (kind === 'js') {
        for (const spec of jsSpecifiers(src)) {
            if (/^\.{1,2}\//.test(spec)) relative(spec);
            else if (!/^[a-z]+:/.test(spec) || spec.startsWith('node:')) {
                const pkg = packageOf(spec);
                if (pkg) packages.push(pkg);
            }
        }
    } else if (kind === 'html') {
        const specs = [
            ...[...src.matchAll(/<(?:script|img)\b[^>]*\ssrc=["']([^"']+)["']/g)].map((m) => m[1]),
            ...[...src.matchAll(/<link\b[^>]*\shref=["']([^"']+)["']/g)].map((m) => m[1]),
            ...[...src.matchAll(/<script\b[^>]*type=["']module["'][^>]*>([\s\S]*?)<\/script>/g)].flatMap((m) =>
                jsSpecifiers(m[1])
            )
        ];
        for (const spec of specs) if (!/^[a-z]+:|^\/\//i.test(spec) && !spec.startsWith('#')) relative(spec);
    } else if (kind === 'css') {
        for (const m of src.matchAll(/@import\s+(?:url\()?\s*['"]?([^'")\s;]+)/g))
            if (/^\.{1,2}\//.test(m[1])) relative(m[1]);
    } else if (kind === 'c') {
        for (const m of src.matchAll(/^[ \t]*#[ \t]*include[ \t]+"([^"]+)"/gm))
            local.push(...resolveBySuffix(m[1], file, repo));
    } else if (kind === 'py') {
        for (const mod of pyModules(src)) {
            const path = join(dirname(file), `${mod.replace(/\./g, '/')}.py`);
            if (repo.known.has(path)) local.push(path);
        }
    } else if (kind === 'cmake') {
        for (const m of src.matchAll(/\b(include|add_subdirectory)\s*\(\s*([^)\s]+)/g)) {
            const arg = m[2].replace(/^"|"$/g, '').replace(/^\$\{[^}]+\}\//, '');
            if (/\$/.test(arg)) continue; // outside the repository (ESP-IDF, CMake modules)
            const target = m[1] === 'add_subdirectory' ? `${arg}/CMakeLists.txt` : arg;
            if (target.includes('.') || m[1] === 'add_subdirectory') local.push(...resolveBySuffix(target, file, repo));
        }
    }
    return {
        local: [...new Set(local)].filter((f) => f !== file),
        packages: [...new Set(packages)],
        built: [...new Set(built)],
        unresolved
    };
}

// Files that run or read other files by naming them: scripts in tasks and hooks, data read by the code
const MENTION_SOURCES =
    /^(package\.json|app\/package\.json|mise\.toml|\.lintstagedrc[^/]*|playwright\.config\.mjs)$|^\.github\/workflows\/|^\.husky\/[^/.]+$/;

function mentionTargets(files, kinds) {
    return files.filter(
        (f) => kinds.get(f) === 'data' || /^scripts\/[^/]+\.(py|mjs|sh)$/.test(f) || /(^|\/)Dockerfile$/.test(f)
    );
}

function mentionPattern(target, files) {
    const name = basename(target);
    const unique = files.filter((f) => basename(f) === name).length === 1 && name !== 'Dockerfile';
    return new RegExp(`(^|[^\\w.-])${unique ? '(?:[\\w./-]*/)?' : ''}${escapeRe(unique ? name : target)}(?![\\w-])`);
}

// The same file name in any folder of the same parent: firmware/boards/*/board_config.h
function folderKey(path) {
    const parts = path.split('/');
    return parts.length > 2 ? `${parts.slice(0, -2).join('/')}/*/${parts.at(-1)}` : path;
}

/** Paths of the same file in every folder of one parent (each board of firmware/boards), written once with `*`. */
export function collapse(paths, files = paths) {
    const groups = new Map();
    for (const p of paths) groups.set(folderKey(p), [...(groups.get(folderKey(p)) ?? []), p]);
    // `*` only when the list holds that file of every such folder, not just some of them
    const everywhere = (key, list) =>
        list.length > 1 && files.filter((f) => folderKey(f) === key).length === list.length;
    return [...groups].flatMap(([key, list]) => (everywhere(key, list) ? [key] : list));
}

// --- Commands -----------------------------------------------------------------------------------------------------

export function miseTasks(toml) {
    const tasks = [];
    for (const section of toml.split(/^(?=\[)/m)) {
        const name = section
            .match(/^\[tasks\.(?:"([^"]+)"|([\w:.-]+))\]/)
            ?.slice(1)
            .find(Boolean);
        if (!name || /^\s*hide\s*=\s*true/m.test(section)) continue;
        const description = section
            .match(/^\s*description\s*=\s*(?:"((?:\\.|[^"\\])*)"|'([^']*)')/m)
            ?.slice(1)
            .find((x) => x !== undefined);
        tasks.push({ name, description: description ?? '' });
    }
    return tasks.toSorted((a, b) => byCodeUnit(a.name, b.name));
}

function commandLines(contents, manifests) {
    const lines = ['## Commands', ''];
    const toml = contents.get('mise.toml');
    if (toml) {
        lines.push('mise (`mise run <task>`):');
        for (const t of miseTasks(toml)) lines.push(`- \`${t.name}\`${t.description ? ` — ${t.description}` : ''}`);
        lines.push('');
    }
    for (const m of manifests) {
        const scripts = Object.entries(JSON.parse(contents.get(m)).scripts ?? {});
        if (!scripts.length) continue;
        lines.push(`npm (\`npm run <script>\`${m === 'package.json' ? '' : `, workspace ${dirname(m)}`}):`);
        for (const [name, cmd] of scripts) lines.push(`- \`${name}\`: \`${cmd}\``);
        lines.push('');
    }
    return lines;
}

function packageLines(contents, manifests, all, info) {
    const importers = new Map();
    for (const [f, i] of info) for (const p of i.packages) importers.set(p, [...(importers.get(p) ?? []), f]);
    // Where a package used only as a command or a plugin shows up: npm scripts, tasks, hooks and config files
    const configText = [
        ...manifests.map((m) => Object.values(JSON.parse(contents.get(m)).scripts ?? {}).join('\n')),
        ...all
            .filter((f) => !manifests.includes(f))
            .filter(
                (f) =>
                    MENTION_SOURCES.test(f) || /\.config\.m?[jt]s$|^\.[^/]*rc[^/]*$|(^|\/)tsconfig[^/]*\.json$/.test(f)
            )
            .map((f) => contents.get(f))
    ].join('\n');
    const lines = ['## npm packages', ''];
    for (const m of manifests) {
        const pkg = JSON.parse(contents.get(m));
        for (const name of Object.keys({ ...pkg.dependencies, ...pkg.devDependencies }).toSorted(byCodeUnit)) {
            const users = importers.get(name) ?? [];
            const notes = [
                users.length ? `imported by ${users.length} file${users.length > 1 ? 's' : ''}` : '',
                new RegExp(`(^|[^\\w@/-])${escapeRe(name)}(?![\\w-])`).test(configText)
                    ? 'named in scripts or config'
                    : '',
                name.startsWith('@types/') ? 'types for TypeScript' : ''
            ].filter(Boolean);
            lines.push(`- \`${name}\` (${m}) — ${notes.join(', ') || 'not referenced'}`);
        }
    }
    lines.push('');
    return lines;
}

// --- The map ------------------------------------------------------------------------------------------------------

export function buildMap(contents, isIgnored = () => false) {
    const all = [...contents.keys()].toSorted(byCodeUnit);
    const kinds = new Map(all.map((f) => [f, kindOf(f)]));
    const files = all.filter((f) => kinds.get(f));
    const repo = repository(files, isIgnored);
    const info = new Map();
    for (const f of files) {
        const src = contents.get(f);
        const kind = kinds.get(f);
        const purpose = purposeOf(src, kind);
        const generated = /^generated\b/i.test(purpose);
        info.set(f, {
            kind,
            lines: src.split('\n').length - (src.endsWith('\n') ? 1 : 0),
            purpose,
            exports: generated ? [] : symbolsOf(f, src, kind),
            ...usesOf(f, src, kind, repo)
        });
    }
    // A C source with no comment of its own implements the header of the same name
    for (const [f, i] of info) {
        if (i.purpose || i.kind !== 'c' || !f.endsWith('.c')) continue;
        const header = i.local.find((h) => basename(h) === `${basename(f, '.c')}.h`);
        const said = info.get(header)?.purpose;
        if (said)
            i.purpose = `Implements ${basename(header)}: ${/^[A-Z][a-z]/.test(said) ? said[0].toLowerCase() + said.slice(1) : said}`;
    }

    const usedBy = new Map(files.map((f) => [f, new Set()]));
    for (const [f, i] of info) for (const dep of i.local) usedBy.get(dep)?.add(f);
    const mentioners = all.filter((f) => MENTION_SOURCES.test(f) || (kinds.get(f) && kinds.get(f) !== 'data'));
    const code = new Map(mentioners.map((m) => [m, withoutComments(contents.get(m), kinds.get(m) ?? 'sh')]));
    for (const target of mentionTargets(files, kinds)) {
        const re = mentionPattern(target, all);
        for (const m of mentioners) if (m !== target && re.test(code.get(m))) usedBy.get(target).add(m);
    }

    const problems = [];
    for (const [f, i] of info) {
        // A test's fixtures often hold made-up imports, and running the test catches a real one that is missing
        if (!isTest(f)) for (const spec of i.unresolved) problems.push(`${f}: local import "${spec}" not found`);
        if (!i.purpose && !isTest(f) && i.kind !== 'data')
            problems.push(`${f}: no purpose comment at the top (what the file is for, in one sentence)`);
    }

    const lines = [
        '# Code map',
        '',
        'Generated by `npm run codemap` from the files git tracks (the pre-commit hook keeps it fresh, CI checks it). Do not edit by hand.',
        'Per file: lines, purpose, exports (C headers: functions, types and macros; C sources: the functions they define),',
        '→ local files it uses, ← files that use it (including the tasks, hooks and code that run a script or read a data',
        'file). Tests are listed, not detailed. Commands and npm packages at the end.',
        ''
    ];
    const groups = new Map();
    for (const f of files) {
        const dir = f.includes('/') ? dirname(f) : '(root)';
        groups.set(dir, [...(groups.get(dir) ?? []), f]);
    }
    for (const [dir, list] of [...groups].toSorted(([a], [b]) => byCodeUnit(a, b))) {
        lines.push(`## ${dir}`);
        const tests = list.filter(isTest);
        for (const f of list.filter((x) => !isTest(x))) {
            const i = info.get(f);
            const label = i.kind === 'data' ? 'data' : i.purpose;
            lines.push(`- **${basename(f)}** (${i.lines})${label ? ` — ${label}` : ''}`);
            if (i.exports.length) lines.push(`  - exports: ${i.exports.join(', ')}`);
            if (i.local.length) lines.push(`  - → ${collapse(i.local, files).join(', ')}`);
            if (i.built.length) lines.push(`  - → built: ${i.built.join(', ')}`);
            if (i.packages.length) lines.push(`  - packages: ${i.packages.join(', ')}`);
            const users = [...usedBy.get(f)].filter((u) => !isTest(u)).toSorted(byCodeUnit);
            if (users.length) lines.push(`  - ← ${collapse(users, files).join(', ')}`);
        }
        if (tests.length) lines.push(`- tests: ${tests.map((t) => basename(t)).join(', ')}`);
        lines.push('');
    }
    const manifests = all.filter((f) => /(^|\/)package\.json$/.test(f));
    lines.push(...commandLines(contents, manifests), ...packageLines(contents, manifests, all, info));
    return { text: lines.join('\n'), problems, count: files.length };
}

// --- Reading the staged files -------------------------------------------------------------------------------------

function stagedFiles() {
    const entries = execFileSync('git', ['ls-files', '-s', '-z'], { encoding: 'utf8', maxBuffer: 1 << 26 })
        .split('\0')
        .filter(Boolean)
        .map((line) => {
            const tab = line.indexOf('\t'); // a path may hold a tab, the fields before it never do
            const [mode, sha, stage] = line.slice(0, tab).split(' ');
            return { mode, sha, stage, path: line.slice(tab + 1) };
        })
        // Files, not symbolic links or submodules; during a merge, only what is resolved (stage 0)
        .filter((e) => e.mode.startsWith('100') && e.stage === '0');
    const out = execFileSync('git', ['cat-file', '--batch'], {
        input: entries.map((e) => e.sha).join('\n') + '\n',
        maxBuffer: 1 << 28
    });
    const contents = new Map();
    let at = 0;
    for (const e of entries) {
        const eol = out.indexOf(10, at);
        const size = Number(out.toString('latin1', at, eol).split(' ')[2]);
        const blob = out.subarray(eol + 1, eol + 1 + size);
        at = eol + 1 + size + 1;
        if (!blob.includes(0)) contents.set(e.path, blob.toString('utf8')); // binary files have no place in the map
    }
    return contents;
}

function ignoredByGit(path) {
    try {
        execFileSync('git', ['check-ignore', '-q', '--no-index', path], { stdio: 'ignore' });
        return true;
    } catch {
        return false;
    }
}

function main() {
    const staged = stagedFiles();
    const map = staged.get(OUT);
    staged.delete(OUT);
    const { text, problems, count } = buildMap(staged, ignoredByGit);
    if (process.argv.includes('--check')) {
        if (map !== text)
            problems.unshift(
                `${OUT}: out of date, run npm run codemap and stage it (the pre-commit hook does both at each commit)`
            );
        for (const p of problems) console.error(p);
        if (problems.length) process.exit(1);
        console.log(`${OUT}: up to date (${count} files)`);
        return;
    }
    mkdirSync(dirname(OUT), { recursive: true });
    writeFileSync(OUT, text);
    for (const p of problems) console.error(`warning: ${p}`);
    console.log(`${OUT}: ${count} files`);
}

if (import.meta.main) main();
