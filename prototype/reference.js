// Renders every scene of scenes.json, frozen, as the visual spec for the C engine.
import { Eyes, setupCanvas } from './eyes.js';

const grid = document.getElementById('grid');

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

try {
    const [looksData, scenesData] = await Promise.all(
        ['../eyes/looks.json', 'scenes.json'].map((url) =>
            fetch(url).then((res) => {
                if (!res.ok) throw new Error(`${url}: HTTP ${res.status}`);
                return res.json();
            })
        )
    );
    const looks = new Map(looksData.looks.map((l) => [l.id, l]));
    for (const scene of scenesData.scenes) {
        const figure = document.createElement('figure');
        const canvas = document.createElement('canvas');
        const caption = document.createElement('figcaption');
        caption.innerHTML = `<b>${scene.id}</b><br>${describe(scene)}`;
        figure.append(canvas, caption);
        grid.append(figure);
        new Eyes(setupCanvas(canvas), { seed: 1 }).renderScene(scene, looks.get(scene.look));
    }
} catch (err) {
    console.error('Cannot render the reference scenes, serve the repo root over HTTP', err);
    const box = document.getElementById('error');
    box.textContent = `Impossible d'afficher les scènes : ${err.message}`;
    box.hidden = false;
}
