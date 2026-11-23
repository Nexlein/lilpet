// Runs every table generator of scripts/data.ts (npm run tables), each in its own process, so that a fault in one data
// file never stops the tables of the others; with --check (npm run check:tables), fails if a committed table is out of
// date.
// Usage: node scripts/tables.ts [--check]
import { spawnSync } from 'node:child_process';
import { GENERATORS } from './data.ts';

const failed = Object.values(GENERATORS).filter(
    (script) => spawnSync(process.execPath, [script, ...process.argv.slice(2)], { stdio: 'inherit' }).status !== 0
);
process.exit(failed.length > 0 ? 1 : 0);
