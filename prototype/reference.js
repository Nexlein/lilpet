// Renders every scene of scenes.json with the prototype (JS) and with the C engine (WebAssembly), side by side, and
// scores the difference. The parity test reads window.parity once window.parityDone is true.
import { Eyes, SIZE, setupCanvas } from './eyes.js';
import { loadEngine } from '../engine/wasm/engine.mjs';

const ENGINE_URL = new URL('../engine/build/wasm/lilpet.mjs', import.meta.url).href;

// Tolerances of the parity rule (notes/architecture.md): mean difference per channel (0-255), and share of pixels
// that differ by more than BIG. Anti-aliasing and RGB565 rounding stay well below them.
export const PARITY = { maxMean: 2, big: 32, maxBigShare: 0.005 };

const grid = document.getElementById('grid');
const summary = document.getElementById('summary');

// Short human-readable summary of what a scene sets, shown under its render
function describe(scene) {
    const parts = [scene.look];
    if (scene.emotion) parts.push(scene.emotion);
    if (scene.blink) parts.push(`blink ${scene.blink.phase} ${scene.blink.t}s`);
    if (scene.head) parts.push(`head ${JSON.stringify(scene.head)}`);
    if (scene.gaze) parts.push(`gaze ${JSON.stringify(scene.gaze)}`);
    if (scene.particles) parts.push(`seed ${scene.particles.seed} · ${scene.particles.time}s`);
    return parts.join(' · ');
}

// Mean absolute difference per channel, and share of pixels differing by more than PARITY.big
function compare(a, b) {
    const n = SIZE * SIZE;
    let sum = 0;
    let big = 0;
    for (let p = 0; p < n; p++) {
        const d =
            (Math.abs(a[4 * p] - b[4 * p]) +
                Math.abs(a[4 * p + 1] - b[4 * p + 1]) +
                Math.abs(a[4 * p + 2] - b[4 * p + 2])) /
            3;
        sum += d;
        if (d > PARITY.big) big++;
    }
    return { mean: sum / n, bigShare: big / n };
}

function labelled(label, canvas) {
    const box = document.createElement('div');
    const span = document.createElement('span');
    span.textContent = label;
    box.append(span, canvas);
    return box;
}

async function fetchJson(url) {
    const res = await fetch(url);
    if (!res.ok) throw new Error(`${url}: HTTP ${res.status}`);
    return res.json();
}

try {
    const [looksData, scenesData] = await Promise.all([fetchJson('../eyes/looks.json'), fetchJson('scenes.json')]);
    const looks = new Map(looksData.looks.map((l) => [l.id, l]));

    let engine = null;
    try {
        engine = await loadEngine(ENGINE_URL);
    } catch (err) {
        console.warn('C engine not built, showing the prototype only', err);
        summary.textContent = 'Moteur C absent : lancer « mise run engine:wasm » pour le comparer au prototype.';
    }

    const results = [];
    for (const scene of scenesData.scenes) {
        const look = looks.get(scene.look);
        const figure = document.createElement('figure');
        const caption = document.createElement('figcaption');
        caption.innerHTML = `<b>${scene.id}</b><br>${describe(scene)}`;
        const pair = document.createElement('div');
        pair.className = 'pair';
        figure.append(pair, caption);
        grid.append(figure);

        const jsCanvas = document.createElement('canvas');
        new Eyes(setupCanvas(jsCanvas), { seed: 1 }).renderScene(scene, look);
        pair.append(labelled('Prototype (JS)', jsCanvas));
        if (!engine) continue;

        const cCanvas = document.createElement('canvas');
        cCanvas.width = cCanvas.height = SIZE;
        const pixels = engine.renderScene(scene, look);
        cCanvas.getContext('2d').putImageData(new ImageData(pixels, SIZE, SIZE), 0, 0);
        pair.append(labelled(`Moteur C ${engine.version}`, cCanvas));

        // Compare at screen resolution (the visible JS canvas follows the device pixel ratio)
        const flat = document.createElement('canvas');
        const flatCtx = setupCanvas(flat, 1);
        new Eyes(flatCtx, { seed: 1 }).renderScene(scene, look);
        const { mean, bigShare } = compare(flatCtx.getImageData(0, 0, SIZE, SIZE).data, pixels);
        const pass = mean <= PARITY.maxMean && bigShare <= PARITY.maxBigShare;
        results.push({ id: scene.id, mean, bigShare, pass });
        const score = document.createElement('p');
        score.className = `score ${pass ? 'pass' : 'fail'}`;
        score.textContent = `écart moyen ${mean.toFixed(2)} · ${(bigShare * 100).toFixed(3)} % de pixels > ${PARITY.big}`;
        figure.insertBefore(score, caption);
    }

    if (engine) {
        const passed = results.filter((r) => r.pass).length;
        const worst = Math.max(...results.map((r) => r.mean));
        summary.className = passed === results.length ? 'pass' : 'fail';
        summary.textContent = `Moteur C ${engine.version} : ${passed}/${results.length} scènes dans la tolérance (écart moyen le plus haut : ${worst.toFixed(2)}).`;
    }
    window.parity = results;
} catch (err) {
    console.error('Cannot render the reference scenes, serve the repo root over HTTP', err);
    const box = document.getElementById('error');
    box.textContent = `Impossible d'afficher les scènes : ${err.message}`;
    box.hidden = false;
} finally {
    window.parityDone = true;
}
