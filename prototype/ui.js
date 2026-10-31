// Controls of the interactive page: looks, emotions, shape sliders, pointer and keyboard. They drive the prototype
// (JS), the C engine (WebAssembly, built by `mise run engine:wasm`) or both side by side, as the "Rendu" switch picks.
import { EMOTIONS, PUPIL_TYPES, Eyes, SIZE, setupCanvas } from './eyes.js';
import { loadEngine } from '../engine/wasm/engine.mjs';
import { compare } from './parity.js';

const ENGINE_URL = new URL('../engine/build/wasm/lilpet.mjs', import.meta.url).href;
// Side by side, both renderers advance in fixed steps of 1/64 s, exact in float and double, with the same seed and
// inputs: their logic stays in step (as in the parity tests) until a rounding difference flips an event.
const STEP = 1 / 64;
const MAX_STEPS = 8; // per frame, so a slow frame does not snowball

const screens = document.getElementById('screens');
const jsView = document.getElementById('jsView');
const cView = document.getElementById('cView');
const jsCanvas = document.getElementById('screen');
const cCanvas = document.getElementById('engine');
const jsCtx = setupCanvas(jsCanvas);
const cCtx = cCanvas.getContext('2d');
const flatCtx = setupCanvas(document.createElement('canvas'), 1); // the prototype at screen resolution, to compare
const info = document.getElementById('engineInfo');
const resyncButton = document.getElementById('resync');

let eyes = null; // prototype renderer
let engine = null; // C engine, loaded the first time it is shown
let engineMissing = false;
let mode = 'js'; // 'js' | 'c' | 'side'
let time = 0; // seconds of animation, shared by both renderers
let look = null;
let emotion = 'neutral'; // last emotion picked (timed ones go back to neutral on their own)
let shownEmotion = null;
let paused = false;
let renderMs = 0;

const randomSeed = () => (Math.random() * 2 ** 32) >>> 0;

// --- Renderers ---------------------------------------------------------------------------------------------------
// A new prototype renderer keeps the settings of the previous one
function createEyes(seed) {
    const prev = eyes;
    eyes = new Eyes(jsCtx, { seed });
    window.eyes = eyes; // drive the renderer from the browser console when debugging
    eyes.onEmotion = highlightEmotion;
    if (prev) {
        eyes.shape = { ...prev.shape };
        eyes.headAmount = prev.headAmount;
        eyes.follow = prev.follow;
        eyes.mouse = prev.mouse;
    }
    if (look) eyes.setLook(look);
    eyes.setEmotion(emotion);
}

// Restarts both renderers from the same seed and the current settings
function resync() {
    const seed = randomSeed();
    time = 0;
    createEyes(seed);
    if (!engine) return;
    engine.init(seed);
    if (look) engine.setLook(look);
    engine.setShape(eyes.shape);
    engine.setHeadAmount(eyes.headAmount);
    engine.setFollow(eyes.follow);
    engine.setPointer(eyes.mouse);
    engine.setEmotion(emotion);
}

async function loadEngineOnce() {
    if (!engine && !engineMissing) {
        try {
            engine = await loadEngine(ENGINE_URL);
        } catch (err) {
            engineMissing = true;
            console.warn('C engine not built, run: mise run engine:wasm', err);
        }
    }
    return engine;
}

async function setMode(next) {
    if (next !== 'js' && !(await loadEngineOnce())) {
        info.textContent = 'Moteur C absent : lancer « mise run engine:wasm ».';
        next = 'js';
    }
    mode = next;
    jsView.hidden = mode === 'c';
    cView.hidden = mode === 'js';
    screens.classList.toggle('side', mode === 'side');
    resyncButton.hidden = mode !== 'side';
    for (const b of document.querySelectorAll('#modes button')) b.classList.toggle('on', b.dataset.mode === mode);
    if (mode === 'js') info.textContent = '';
    else resync();
}

// --- Actions: applied to both renderers, so they keep the same inputs ---------------------------------------------
function setLook(l) {
    look = l;
    eyes.setLook(l);
    engine?.setLook(l);
    const [a, b] = l.pupilColor;
    document.getElementById('lookInfo').textContent =
        `${l.id} | œil ${l.sclera} | pupille ${l.pupil} ${a}${a !== b ? ' / ' + b : ''}${l.highlight ? ' | reflet' : ''}`;
}

function setEmotion(name, duration = 0) {
    if (!duration) emotion = name;
    eyes.setEmotion(name, duration);
    engine?.setEmotion(name, duration);
}

function blink() {
    eyes.startBlink();
    engine?.blink();
}

function pet() {
    setEmotion('happy', 1.8);
    for (let i = 0; i < 4; i++) {
        eyes.spawn('sparkle');
        engine?.spawn('sparkle');
    }
}

function setPointer(point) {
    eyes.mouse = point;
    engine?.setPointer(point);
}

// --- Looks -------------------------------------------------------------------------------------------------------
let looks = [];

async function loadLooks() {
    try {
        const res = await fetch('../eyes/looks.json');
        looks = (await res.json()).looks;
    } catch (err) {
        console.error('Cannot load looks.json, serve the repo root over HTTP', err);
        looks = [
            { id: 'fallback', sclera: '#6EF2CF', pupil: 'acorn', pupilColor: ['#167358', '#167358'], highlight: false }
        ];
    }
    const select = document.getElementById('lookSelect');
    select.innerHTML = looks.map((l, i) => `<option value="${i}">${l.id} · ${l.pupil}</option>`).join('');
    select.addEventListener('change', () => setLook(looks[select.value]));
    setLook(looks[0]);
}

const hsl = (h, s, l) => {
    s /= 100;
    l /= 100;
    const f = (n) => {
        const k = (n + h / 30) % 12;
        const c = l - s * Math.min(l, 1 - l) * Math.max(-1, Math.min(k - 3, 9 - k, 1));
        return Math.round(c * 255)
            .toString(16)
            .padStart(2, '0');
    };
    return `#${f(0)}${f(8)}${f(4)}`;
};

function randomLook() {
    const h = Math.random() * 360;
    const pupil = PUPIL_TYPES[Math.floor(Math.random() * PUPIL_TYPES.length)];
    const sclera = Math.random() < 0.12 ? '#F5F1E8' : hsl(h, 70 + Math.random() * 30, 45 + Math.random() * 30);
    // Pupil: darker same hue, or a contrasting hue
    const pick = () =>
        Math.random() < 0.5
            ? hsl(h, 70, 18 + Math.random() * 15)
            : hsl((h + 120 + Math.random() * 120) % 360, 85, 50 + Math.random() * 15);
    const left = pick();
    const right = Math.random() < 0.15 ? pick() : left; // heterochromia, sometimes
    setLook({
        id: 'random',
        sclera,
        pupil,
        pupilColor: [left, right],
        highlight: pupil === 'round' || Math.random() < 0.2
    });
    document.getElementById('lookSelect').selectedIndex = -1;
}

// --- Emotions ----------------------------------------------------------------------------------------------------
const emotionsEl = document.getElementById('emotions');
Object.entries(EMOTIONS).forEach(([name, e], i) => {
    const b = document.createElement('button');
    b.dataset.e = name;
    b.innerHTML = `${e.label} <kbd>${i + 1}</kbd>`;
    b.addEventListener('click', () => setEmotion(name));
    emotionsEl.append(b);
});

function highlightEmotion(name) {
    shownEmotion = name;
    emotionsEl.querySelectorAll('button').forEach((b) => b.classList.toggle('on', b.dataset.e === name));
}

// --- Shape sliders: the prototype's values are the source, sliders mirror them ------------------------------------
const SLIDERS = {
    rx: { get: () => eyes.shape.rx, set: (v) => (eyes.shape.rx = v) },
    ry: { get: () => eyes.shape.ry, set: (v) => (eyes.shape.ry = v) },
    gap: { get: () => eyes.shape.gap, set: (v) => (eyes.shape.gap = v) },
    tilt: { get: () => eyes.shape.tilt * 100, set: (v) => (eyes.shape.tilt = v / 100) },
    head: { get: () => eyes.headAmount * 100, set: (v) => (eyes.headAmount = v / 100) }
};

function bindControls() {
    for (const [id, { get, set }] of Object.entries(SLIDERS)) {
        const input = document.getElementById(id);
        const out = input.nextElementSibling;
        input.value = Math.round(get());
        out.textContent = input.value;
        input.addEventListener('input', () => {
            set(+input.value);
            out.textContent = input.value;
            engine?.setShape(eyes.shape);
            engine?.setHeadAmount(eyes.headAmount);
        });
    }

    const follow = document.getElementById('follow');
    eyes.follow = follow.checked;
    follow.addEventListener('change', () => {
        eyes.follow = follow.checked;
        engine?.setFollow(follow.checked);
    });

    // Pointer: normalized position over whichever screen is hovered
    for (const canvas of [jsCanvas, cCanvas]) {
        canvas.addEventListener('mousemove', (e) => {
            const r = canvas.getBoundingClientRect();
            let x = (e.clientX - r.left - r.width / 2) / (r.width / 2);
            let y = (e.clientY - r.top - r.height / 2) / (r.height / 2);
            const len = Math.hypot(x, y);
            if (len > 1) {
                x /= len;
                y /= len;
            }
            setPointer({ x, y });
        });
        canvas.addEventListener('mouseleave', () => setPointer(null));
        canvas.addEventListener('click', pet); // petting
    }

    for (const b of document.querySelectorAll('#modes button'))
        b.addEventListener('click', () => setMode(b.dataset.mode));
    resyncButton.addEventListener('click', resync);
    document.getElementById('randomLook').addEventListener('click', randomLook);
    document.getElementById('blinkBtn').addEventListener('click', blink);
    document.addEventListener('keydown', (e) => {
        if (e.target.tagName === 'SELECT') return;
        const names = Object.keys(EMOTIONS);
        if (e.key >= '1' && e.key <= String(names.length)) setEmotion(names[+e.key - 1]);
        else if (e.key === ' ') {
            e.preventDefault();
            blink();
        } else if (e.key.toLowerCase() === 'r') randomLook();
    });
}

// --- Loop --------------------------------------------------------------------------------------------------------
function step(dt) {
    time += dt;
    if (mode !== 'c') eyes.update(dt, time);
    if (mode !== 'js') engine.update(dt);
}

// Prototype against C engine at screen resolution, for the side-by-side view
function diff() {
    eyes.ctx = flatCtx;
    eyes.render(time);
    eyes.ctx = jsCtx;
    return compare(flatCtx.getImageData(0, 0, SIZE, SIZE).data, engine.render());
}

let frames = 0;
let lastDiff = null;

function draw() {
    if (mode !== 'c') eyes.render(time);
    if (mode === 'js') return;
    const t0 = performance.now();
    const pixels = engine.render();
    renderMs += (performance.now() - t0 - renderMs) * 0.05;
    cCtx.putImageData(new ImageData(pixels, SIZE, SIZE), 0, 0);
    if (mode === 'c' && engine.emotion() !== shownEmotion) highlightEmotion(engine.emotion());
    if (mode === 'side' && frames++ % 30 === 0) lastDiff = diff();
    const gap = mode === 'side' && lastDiff ? ` · écart moyen ${lastDiff.mean.toFixed(2)}` : '';
    const drift = mode === 'side' && lastDiff && !lastDiff.pass ? ' : décalés, resynchroniser' : '';
    info.textContent = `Moteur C ${engine.version} · rendu ${renderMs.toFixed(2)} ms${gap}${drift}`;
}

let last = performance.now();
let pending = 0;
function frame(now) {
    const dt = Math.min((now - last) / 1000, 0.05);
    last = now;
    if (!paused) {
        if (mode === 'side') {
            pending += dt;
            for (let n = 0; pending >= STEP && n < MAX_STEPS; n++) {
                pending -= STEP;
                step(STEP);
            }
            pending = Math.min(pending, STEP);
        } else step(dt);
    }
    draw();
    requestAnimationFrame(frame);
}

createEyes(randomSeed());
bindControls();
await loadLooks();
requestAnimationFrame(frame);

// Hooks for the live parity test (tests/live.spec.mjs): pause the loop, then step both renderers by hand
window.live = {
    setMode,
    resync,
    setEmotion,
    pet,
    setPointer,
    pause: () => (paused = true),
    step: (n) => {
        for (let i = 0; i < n; i++) step(STEP);
    },
    diff
};
