// Controls of the prototype page: looks, emotions, shape sliders, pointer and keyboard.
import { EMOTIONS, PUPIL_TYPES, Eyes, setupCanvas } from './eyes.js';

const canvas = document.getElementById('screen');
const eyes = new Eyes(setupCanvas(canvas));
window.eyes = eyes; // drive the renderer from the browser console when debugging

let looks = [];

// --- Looks ----------------------------------------------------------------------
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

function setLook(l) {
    eyes.setLook(l);
    const [a, b] = l.pupilColor;
    document.getElementById('lookInfo').textContent =
        `${l.id} | œil ${l.sclera} | pupille ${l.pupil} ${a}${a !== b ? ' / ' + b : ''}${l.highlight ? ' | reflet' : ''}`;
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

// --- Emotions -------------------------------------------------------------------
const emotionsEl = document.getElementById('emotions');
Object.entries(EMOTIONS).forEach(([name, e], i) => {
    const b = document.createElement('button');
    b.dataset.e = name;
    b.innerHTML = `${e.label} <kbd>${i + 1}</kbd>`;
    b.addEventListener('click', () => eyes.setEmotion(name));
    emotionsEl.append(b);
});
eyes.onEmotion = (name) =>
    emotionsEl.querySelectorAll('button').forEach((b) => b.classList.toggle('on', b.dataset.e === name));

// --- Shape sliders: the renderer's values are the source, sliders mirror them ------------
const SLIDERS = {
    rx: { get: () => eyes.shape.rx, set: (v) => (eyes.shape.rx = v) },
    ry: { get: () => eyes.shape.ry, set: (v) => (eyes.shape.ry = v) },
    gap: { get: () => eyes.shape.gap, set: (v) => (eyes.shape.gap = v) },
    tilt: { get: () => eyes.shape.tilt * 100, set: (v) => (eyes.shape.tilt = v / 100) },
    head: { get: () => eyes.headAmount * 100, set: (v) => (eyes.headAmount = v / 100) }
};
for (const [id, { get, set }] of Object.entries(SLIDERS)) {
    const input = document.getElementById(id);
    const out = input.nextElementSibling;
    input.value = Math.round(get());
    out.textContent = input.value;
    input.addEventListener('input', () => {
        set(+input.value);
        out.textContent = input.value;
    });
}

const follow = document.getElementById('follow');
eyes.follow = follow.checked;
follow.addEventListener('change', () => (eyes.follow = follow.checked));

// --- Pointer and keyboard -----------------------------------------------------------
canvas.addEventListener('mousemove', (e) => {
    const r = canvas.getBoundingClientRect();
    let x = (e.clientX - r.left - r.width / 2) / (r.width / 2);
    let y = (e.clientY - r.top - r.height / 2) / (r.height / 2);
    const len = Math.hypot(x, y);
    if (len > 1) {
        x /= len;
        y /= len;
    }
    eyes.mouse = { x, y };
});
canvas.addEventListener('mouseleave', () => (eyes.mouse = null));
canvas.addEventListener('click', () => {
    // petting
    eyes.setEmotion('happy', 1.8);
    for (let i = 0; i < 4; i++) eyes.spawn('sparkle');
});

document.getElementById('randomLook').addEventListener('click', randomLook);
document.getElementById('blinkBtn').addEventListener('click', () => eyes.startBlink());
document.addEventListener('keydown', (e) => {
    if (e.target.tagName === 'SELECT') return;
    const names = Object.keys(EMOTIONS);
    if (e.key >= '1' && e.key <= String(names.length)) eyes.setEmotion(names[+e.key - 1]);
    else if (e.key === ' ') {
        e.preventDefault();
        eyes.startBlink();
    } else if (e.key.toLowerCase() === 'r') randomLook();
});

// --- Loop -------------------------------------------------------------------------
let last = performance.now();
function frame(now) {
    const dt = Math.min((now - last) / 1000, 0.05);
    last = now;
    eyes.update(dt, now / 1000);
    eyes.render(now / 1000);
    requestAnimationFrame(frame);
}

eyes.setEmotion('neutral');
await loadLooks();
requestAnimationFrame(frame);
