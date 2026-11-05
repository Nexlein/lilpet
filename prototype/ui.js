// Controls of the interactive page: looks, emotions, shape sliders, pointer and keyboard. They drive the prototype
// (JS), the C engine (WebAssembly, built by `mise run engine:wasm`) or both side by side, as the "Rendu" switch picks.
import { PUPIL_TYPES, drawEyes, setupCanvas } from './draw.js';
import { EMOTIONS, Eyes, SIZE, makeLook } from './eyes.js';
import { loadEngine } from '../engine/wasm/engine.mjs';
import { ENGINE_URL, compareFrame } from './parity.js';

// Side by side, both renderers advance in fixed steps of 1/64 s, exact in float and double, with the same seed and
// inputs: their logic stays in step (as in the parity tests) until a rounding difference flips an event.
const STEP = 1 / 64;

const screens = document.getElementById('screens');
const jsView = document.getElementById('jsView');
const cView = document.getElementById('cView');
const jsCanvas = document.getElementById('screen');
const cCanvas = document.getElementById('engine');
const jsCtx = setupCanvas(jsCanvas);
const cCtx = cCanvas.getContext('2d');
const info = document.getElementById('engineInfo');
const resyncButton = document.getElementById('resync');

let eyes = null; // prototype renderer
let engine = null; // C engine, loaded the first time it is shown
let engineMissing = false;
let mode = 'js'; // 'js' | 'c' | 'side'
let time = 0; // seconds of animation, shared by both renderers
let look = null;
let emotion = 'neutral'; // last emotion picked: the mood that reactions come back to
let shownEmotion = null;
let paused = false;
let renderMs = 0;

const randomSeed = () => (Math.random() * 2 ** 32) >>> 0;

// --- Renderers ---------------------------------------------------------------------------------------------------
// A new prototype renderer keeps the settings of the previous one
function createEyes(seed) {
    const prev = eyes;
    eyes = new Eyes(seed);
    window.eyes = eyes; // drive the renderer from the browser console when debugging
    eyes.onEmotion = highlightEmotion;
    if (prev) {
        eyes.shape = { ...prev.shape };
        eyes.headAmount = prev.headAmount;
        eyes.follow = prev.follow;
        eyes.mouse = prev.mouse;
        eyes.gravity = prev.gravity;
        eyes.shaking = prev.shaking;
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
    engine.setGravity(eyes.gravity);
    engine.setShaking(eyes.shaking);
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
        `${l.id} · ${l.pupil} | œil ${l.sclera} | pupille ${a}${a !== b ? ' / ' + b : ''}`;
}

function setEmotion(name) {
    emotion = name;
    eyes.setEmotion(name);
    engine?.setEmotion(name);
}

function blink() {
    eyes.startBlink();
    engine?.blink();
}

// Reactions (REACTIONS in eyes.js): what the device's button and motion sensor will trigger
function react(event) {
    eyes.react(event);
    engine?.react(event);
}

const pet = () => react('tap');

function setPointer(point) {
    eyes.setPointer(point);
    engine?.setPointer(point);
}

// Being shaken (the device tells it from its accelerometer): held down on the page
function setShaking(shaking) {
    eyes.shaking = shaking;
    engine?.setShaking(shaking);
}

// Accelerometer stand-in: gravity in the screen plane, or null
function setGravity(g) {
    eyes.setGravity(g);
    engine?.setGravity(g);
}

// --- Looks -------------------------------------------------------------------------------------------------------
// A look is a swatch worn with a pupil type, each picked from its own list
let swatches = [];
const swatchSelect = document.getElementById('swatchSelect');
const pupilSelect = document.getElementById('pupilSelect');

async function loadLooks() {
    try {
        const res = await fetch('../eyes/looks.json');
        swatches = (await res.json()).swatches;
    } catch (err) {
        console.error('Cannot load looks.json, serve the repo root over HTTP', err);
        swatches = [{ id: 'terrarium', sclera: '#6FF5D0', pupilColor: ['#106E54', '#106E54'] }];
    }
    swatchSelect.innerHTML = swatches.map((s, i) => `<option value="${i}">${s.id}</option>`).join('');
    pupilSelect.innerHTML = PUPIL_TYPES.map((p) => `<option>${p}</option>`).join('');
    swatchSelect.addEventListener('change', applyLook);
    pupilSelect.addEventListener('change', applyLook);
    const cherry = swatches.findIndex((s) => s.id === 'cherry');
    swatchSelect.value = String(Math.max(cherry, 0));
    applyLook();
}

function applyLook() {
    setLook(makeLook(swatches[swatchSelect.value], pupilSelect.value));
}

function randomLook() {
    swatchSelect.value = String(Math.floor(Math.random() * swatches.length));
    pupilSelect.value = PUPIL_TYPES[Math.floor(Math.random() * PUPIL_TYPES.length)];
    applyLook();
}

// --- Emotions ----------------------------------------------------------------------------------------------------
const emotionsEl = document.getElementById('emotions');
Object.entries(EMOTIONS).forEach(([name, e], i) => {
    const b = document.createElement('button');
    b.dataset.e = name;
    b.innerHTML = i < 9 ? `${e.label} <kbd>${i + 1}</kbd>` : e.label; // digit keys 1 to 9
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

    // Object turned by an angle, standing up: gravity in the screen plane (0, the default, as without accelerometer)
    const lean = document.getElementById('lean');
    lean.addEventListener('input', () => {
        const a = (+lean.value * Math.PI) / 180;
        lean.nextElementSibling.textContent = `${lean.value}°`;
        setGravity(+lean.value ? { x: Math.sin(a), y: Math.cos(a) } : null);
    });
    lean.nextElementSibling.textContent = '0°';

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
    const shakeBtn = document.getElementById('shakeBtn');
    shakeBtn.addEventListener('pointerdown', () => setShaking(true));
    for (const end of ['pointerup', 'pointerleave', 'pointercancel'])
        shakeBtn.addEventListener(end, () => setShaking(false));
    document.addEventListener('keyup', (e) => e.key.toLowerCase() === 's' && setShaking(false));
    window.addEventListener('blur', () => setShaking(false)); // the key or button may be let go elsewhere
    document.addEventListener('keydown', (e) => {
        if (e.target.tagName === 'SELECT') return;
        const names = Object.keys(EMOTIONS);
        const digit = '123456789'.indexOf(e.key);
        if (digit >= 0 && digit < names.length) setEmotion(names[digit]);
        else if (e.key === ' ') {
            e.preventDefault();
            blink();
        } else if (e.key.toLowerCase() === 'r') randomLook();
        else if (e.key.toLowerCase() === 's') setShaking(true);
    });
}

// --- Loop --------------------------------------------------------------------------------------------------------
function step(dt) {
    time += dt;
    if (mode !== 'c') eyes.update(dt, time);
    if (mode !== 'js') engine.update(dt);
}

// Prototype against C engine, for the side-by-side view
const diff = () => compareFrame(eyes, time, engine.render());

let frames = 0;
let lastDiff = null;

function draw() {
    if (mode !== 'c') drawEyes(jsCtx, eyes, time);
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
            pending += dt; // dt is capped above, so a slow frame never snowballs
            while (pending >= STEP) {
                pending -= STEP;
                step(STEP);
            }
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
    setGravity,
    setShaking,
    react,
    pause: () => (paused = true),
    step: (n) => {
        for (let i = 0; i < n; i++) step(STEP);
    },
    diff
};
