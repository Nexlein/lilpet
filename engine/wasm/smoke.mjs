// Loads the WebAssembly build in Node and checks the C API answers and renders something.
// Usage: node engine/wasm/smoke.mjs engine/build/wasm/lilpet.mjs
import { pathToFileURL } from 'node:url';
import { resolve } from 'node:path';
import { loadEngine } from './engine.mjs';

const modulePath = process.argv[2] ?? 'engine/build/wasm/lilpet.mjs';
const engine = await loadEngine(pathToFileURL(resolve(modulePath)).href);

const fail = (msg) => {
    console.error(msg);
    process.exit(1);
};
if (!/^\d+\.\d+\.\d+$/.test(engine.version)) fail(`lp_version() returned "${engine.version}"`);
if (!engine.emotions.includes('love')) fail(`unexpected emotion list: ${engine.emotions.join(', ')}`);

const look = { sclera: '#FF1A1A', pupil: 'dot', pupilColor: ['#2B4CFF', '#2B4CFF'], highlight: false };
const pixels = engine.renderScene({}, look);
let lit = 0;
for (let i = 0; i < pixels.length; i += 4) if (pixels[i] > 200) lit++;
const share = lit / (engine.size * engine.size);
if (share < 0.05 || share > 0.5) fail(`rendered frame looks wrong: ${(share * 100).toFixed(1)}% red pixels`);

console.log(`WebAssembly engine ${engine.version} OK (${(share * 100).toFixed(1)}% of the frame is eye)`);
