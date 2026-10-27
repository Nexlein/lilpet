// Loads the WebAssembly build in Node and checks the C API answers.
// Usage: node engine/wasm/smoke.mjs engine/build/wasm/lilpet.mjs
import { pathToFileURL } from 'node:url';
import { resolve } from 'node:path';

const modulePath = process.argv[2] ?? 'engine/build/wasm/lilpet.mjs';
const { default: createLilpet } = await import(pathToFileURL(resolve(modulePath)).href);
const lilpet = await createLilpet();

const version = lilpet.UTF8ToString(lilpet._lp_version());
if (!/^\d+\.\d+\.\d+$/.test(version)) {
    console.error(`lp_version() returned "${version}", expected MAJOR.MINOR.PATCH`);
    process.exit(1);
}
console.log(`WebAssembly engine ${version} OK`);
