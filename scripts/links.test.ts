// Tests the links between the repo's own Markdown files: each relative link leads to a tracked file, and each anchor to
// a heading of it, as GitHub names them. A section renamed or dropped (the vision board's, in the 1.75" rewrite) would
// otherwise leave links that lead nowhere. Built-in test runner, no dependency.
// Usage: npm run check:scripts
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
import { dirname, join, normalize } from 'node:path';
import { test } from 'node:test';

const TRACKED = new Set(execFileSync('git', ['ls-files', '-z'], { encoding: 'utf8' }).split('\0').filter(Boolean));
const DOCS = [...TRACKED].filter((path) => path.endsWith('.md'));

// GitHub's anchor for a heading: lower case, without punctuation, spaces made hyphens; a repeat gets -1, -2...
function anchors(markdown: string) {
    const seen = new Map<string, number>();
    const found = new Set<string>();
    for (const [, heading = ''] of markdown.replace(/^\s*```[\s\S]*?^\s*```/gm, '').matchAll(/^#{1,6} (.+)$/gm)) {
        const base = heading
            .trim()
            .toLowerCase()
            .replace(/[^\p{L}\p{N}\s_-]/gu, '')
            .replace(/ /g, '-');
        const count = seen.get(base) ?? 0;
        seen.set(base, count + 1);
        found.add(count ? `${base}-${count}` : base);
    }
    return found;
}

test('every link between the docs leads somewhere', () => {
    const broken: string[] = [];
    for (const doc of DOCS) {
        const text = readFileSync(doc, 'utf8').replace(/^\s*```[\s\S]*?^\s*```/gm, ''); // code, indented in lists too
        for (const [, target = ''] of text.matchAll(/\]\(([^)\s]+)\)/g)) {
            if (/^[a-z]+:/.test(target)) continue; // a web address
            const [file, anchor] = target.split('#');
            const path = file ? normalize(join(dirname(doc), decodeURI(file))).replace(/\/$/, '') : doc;
            const isFolder = [...TRACKED].some((tracked) => tracked.startsWith(`${path}/`));
            if (!TRACKED.has(path) && !isFolder) broken.push(`${doc}: ${target} (no such file)`);
            else if (anchor && path.endsWith('.md') && !anchors(readFileSync(path, 'utf8')).has(decodeURI(anchor)))
                broken.push(`${doc}: ${target} (no such heading)`);
        }
    }
    assert.deepEqual(broken, []);
});
