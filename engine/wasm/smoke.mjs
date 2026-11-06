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

const look = { sclera: '#FD0C0D', pupil: 'dot', pupilColor: ['#2905E6', '#2905E6'] }; // siren
const pixels = engine.renderScene({}, look);
let lit = 0;
for (let i = 0; i < pixels.length; i += 4) if (pixels[i] > 200) lit++;
const share = lit / (engine.size * engine.size);
if (share < 0.05 || share > 0.5) fail(`rendered frame looks wrong: ${(share * 100).toFixed(1)}% red pixels`);

// RGB565 widens to the nearest 8-bit value: a color that fits 565 comes back as it went in
const exact = engine.renderScene({}, { ...look, sclera: '#192D19' });
let kept = 0;
for (let i = 0; i < exact.length; i += 4)
    if (exact[i] === 0x19 && exact[i + 1] === 0x2d && exact[i + 2] === 0x19) kept++;
if (kept / (engine.size * engine.size) < 0.05) fail('a color that fits RGB565 changed on its way out');

// Unknown names never reach the module: nothing changes
engine.init(1);
engine.setLook(look);
engine.setEmotion('love');
for (const name of ['furious', undefined]) {
    engine.setEmotion(name);
    engine.react(name);
}
if (engine.emotion() !== 'love') fail(`an unknown name changed the emotion to ${engine.emotion()}`);
const before = engine.render();
engine.setLook({ ...look, pupil: 'slit' });
engine.setLook({ ...look, pupil: undefined });
if (!engine.render().every((v, i) => v === before[i])) fail('an unknown pupil changed the look');

// Eyes past the edge of the screen, and a shape with holes in it, render without a memory error
engine.setShape({ rx: 93, ry: 109, gap: 400, tilt: 0.37 });
engine.render();
engine.setShape({ gap: 400 });
engine.render();

console.log(`WebAssembly engine ${engine.version} OK (${(share * 100).toFixed(1)}% of the frame is eye)`);
