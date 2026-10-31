// JavaScript side of the WebAssembly engine: loads the module built by `mise run engine:wasm` and drives it with the
// same scene and look objects as the prototype (prototype/scenes.json, eyes/looks.json).
// Used by prototype/reference.html, the live view of prototype/index.html and the parity tests; the PC app will build
// on it.

const PUPILS = ['dot', 'slit', 'acorn', 'round'];
const BLINK_PHASES = [null, 'anticip', 'close', 'closed', 'open']; // lp_blink_phase order
const PARTICLES = [null, 'heart', 'sparkle']; // lp_particle_type order

const color = (hex) => Number.parseInt(hex.slice(1), 16);

// moduleUrl: URL of lilpet.mjs (the Emscripten ES module next to lilpet.wasm)
export async function loadEngine(moduleUrl) {
    const { default: createLilpet } = await import(moduleUrl);
    const m = await createLilpet();
    const size = m._lpw_screen_size();
    const names = Array.from({ length: m._lpw_emotion_count() }, (_, i) => m.UTF8ToString(m._lpw_emotion_name(i)));
    const emotions = new Map(names.map((name, i) => [name, i]));

    const setLook = (look) =>
        m._lpw_set_look(
            color(look.sclera),
            PUPILS.indexOf(look.pupil),
            color(look.pupilColor[0]),
            color(look.pupilColor[1]),
            look.highlight ? 1 : 0
        );

    // Renders the current state; returns a copy of the RGBA pixels (size × size × 4)
    const render = () => new Uint8ClampedArray(m.HEAPU8.buffer, m._lpw_render(), size * size * 4).slice();

    return {
        version: m.UTF8ToString(m._lp_version()),
        size,
        emotions: [...emotions.keys()],

        // Same as Eyes.renderScene in prototype/eyes.js
        renderScene(scene, look) {
            m._lpw_init(1);
            setLook(look);
            m._lpw_set_scene(
                emotions.get(scene.emotion ?? 'neutral'),
                BLINK_PHASES.indexOf(scene.blink?.phase ?? null),
                scene.blink?.t ?? 0,
                scene.head?.yaw ?? 0,
                scene.head?.pitch ?? 0,
                scene.head?.roll ?? 0,
                scene.gaze?.x ?? 0,
                scene.gaze?.y ?? 0,
                scene.particles ? 1 : 0,
                scene.particles?.seed ?? 0,
                scene.particles?.time ?? 0
            );
            return render();
        },

        // Live animation, for the PC app
        init: (seed) => m._lpw_init(seed),
        setLook,
        setEmotion: (name, duration = 0) => m._lpw_set_emotion(emotions.get(name), duration),
        blink: () => m._lpw_blink(),
        setPointer: (point) => m._lpw_set_pointer(point ? 1 : 0, point?.x ?? 0, point?.y ?? 0),
        setFollow: (follow) => m._lpw_set_follow(follow ? 1 : 0),
        setShape: ({ rx, ry, gap, tilt }) => m._lpw_set_shape(rx, ry, gap, tilt),
        setHeadAmount: (amount) => m._lpw_set_head_amount(amount),
        spawn: (type) => m._lpw_spawn(PARTICLES.indexOf(type)),
        emotion: () => names[m._lpw_emotion()],
        update: (dt) => m._lpw_update(dt),
        render
    };
}
