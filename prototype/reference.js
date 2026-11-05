// Renders every scene of scenes.json with the prototype (JS) and with the C engine (WebAssembly), side by side, and
// scores the difference. The parity test reads window.parity once window.parityDone is true.
import { drawEyes, setupCanvas } from './draw.js';
import { Eyes, SIZE, makeLook } from './eyes.js';
import { loadEngine } from '../engine/wasm/engine.mjs';
import { ENGINE_URL, PARITY, compareFrame } from './parity.js';

document.getElementById('serveHint').hidden = true; // the page runs: it is served
const grid = document.getElementById('grid');
const summary = document.getElementById('summary');

// Short human-readable summary of what a scene sets, shown under its render
function describe(scene) {
    const parts = [`${scene.swatch} ${scene.pupil}`];
    if (scene.emotion) parts.push(scene.emotion);
    if (scene.blink) parts.push(`blink ${scene.blink.phase} ${scene.blink.t}s`);
    if (scene.head) parts.push(`head ${JSON.stringify(scene.head)}`);
    if (scene.gaze) parts.push(`gaze ${JSON.stringify(scene.gaze)}`);
    if (scene.particles) parts.push(`seed ${scene.particles.seed} · ${scene.particles.time}s`);
    return parts.join(' · ');
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
    const swatches = new Map(looksData.swatches.map((s) => [s.id, s]));

    let engine = null;
    try {
        engine = await loadEngine(ENGINE_URL);
    } catch (err) {
        console.warn('C engine not built, showing the prototype only', err);
        summary.textContent = 'Moteur C absent : lancer « mise run engine:wasm » pour le comparer au prototype.';
    }

    const results = [];
    for (const scene of scenesData.scenes) {
        const look = makeLook(swatches.get(scene.swatch), scene.pupil);
        const figure = document.createElement('figure');
        const caption = document.createElement('figcaption');
        caption.innerHTML = `<b>${scene.id}</b><br>${describe(scene)}`;
        const pair = document.createElement('div');
        pair.className = 'pair';
        figure.append(pair, caption);
        grid.append(figure);

        const eyes = new Eyes(1);
        eyes.setScene(scene, look);
        const jsCanvas = document.createElement('canvas');
        drawEyes(setupCanvas(jsCanvas), eyes, 0);
        pair.append(labelled('Prototype (JS)', jsCanvas));
        if (!engine) continue;

        const cCanvas = document.createElement('canvas');
        cCanvas.width = cCanvas.height = SIZE;
        const pixels = engine.renderScene(scene, look);
        cCanvas.getContext('2d').putImageData(new ImageData(pixels, SIZE, SIZE), 0, 0);
        pair.append(labelled(`Moteur C ${engine.version}`, cCanvas));

        const { mean, core, pass } = compareFrame(eyes, 0, pixels);
        results.push({ id: scene.id, mean, core, pass });
        const score = document.createElement('p');
        score.className = `score ${pass ? 'pass' : 'fail'}`;
        score.textContent = `écart moyen ${mean.toFixed(2)} · ${core} pixels de cœur > ${PARITY.big}`;
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
