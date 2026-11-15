// JavaScript side of the WebAssembly engine: loads the module built by `mise run engine:wasm` and drives it with
// scene objects as in engine/tests/scenes.json and looks (a swatch of eyes/looks.json worn with a pupil type).
// Used by the PC app, the reference renders (scripts/goldens.mjs), the data validator and smoke.mjs; its scene fields
// also by the host scene table (scripts/gen-scenes-table.mjs).

export const PUPILS = ['dot', 'cat', 'acorn', 'circle']; // lp_pupil order
export const BLINK_PHASES = [null, 'anticip', 'close', 'closed', 'open']; // lp_blink_phase order

const color = (hex) => Number.parseInt(hex.slice(1), 16);

// A scene object as in engine/tests/scenes.json, as the fields of lp_scene, by name. Read here for the reference renders
// and by scripts/gen-scenes-table.mjs for the host tests: both draw the same state.
export const sceneFields = (scene) => ({
    emotion: scene.emotion ?? 'neutral',
    blink_phase: scene.blink?.phase ?? null,
    blink_t: scene.blink?.t ?? 0,
    yaw: scene.head?.yaw ?? 0,
    pitch: scene.head?.pitch ?? 0,
    roll: scene.head?.roll ?? 0,
    gaze_x: scene.gaze?.x ?? 0,
    gaze_y: scene.gaze?.y ?? 0,
    has_particles: Boolean(scene.particles),
    particle_seed: scene.particles?.seed ?? 0,
    particle_time: scene.particles?.time ?? 0
});

// moduleUrl: URL of lilpet.mjs (the Emscripten ES module next to lilpet.wasm)
export async function loadEngine(moduleUrl) {
    const { default: createLilpet } = await import(/* @vite-ignore */ moduleUrl);
    const m = await createLilpet();
    const size = m._lpw_screen_size();
    const names = Array.from({ length: m._lpw_emotion_count() }, (_, i) => m.UTF8ToString(m._lpw_emotion_name(i)));
    const emotions = new Map(names.map((name, i) => [name, i]));
    const events = new Map(
        Array.from({ length: m._lpw_event_count() }, (_, i) => [m.UTF8ToString(m._lpw_event_name(i)), i])
    );

    // Unknown emotion, event and pupil names are ignored: they never reach the module, where they would become a number
    const setLook = (look) => {
        const pupil = PUPILS.indexOf(look.pupil);
        if (pupil < 0) return;
        m._lpw_set_look(color(look.sclera), pupil, color(look.pupilColor[0]), color(look.pupilColor[1]));
    };

    // Renders the current state; returns the RGBA pixels (size × size × 4) in the engine's memory, until the next render
    const renderView = () => new Uint8ClampedArray(m.HEAPU8.buffer, m._lpw_render(), size * size * 4);

    // The same, as a copy to keep
    const render = () => renderView().slice();

    // The region the last render changed, in pixels; the rest of the frame is the one before
    const dirty = () => {
        const [x0, y0, x1, y1] = new Int32Array(m.HEAPU8.buffer, m._lpw_dirty(), 4); // lp_rect: x0, y0, x1, y1
        return x0 < x1 && y0 < y1
            ? { x: x0, y: y0, width: x1 - x0, height: y1 - y0 }
            : { x: 0, y: 0, width: 0, height: 0 };
    };

    return {
        version: m.UTF8ToString(m._lp_version()),
        size,
        emotions: [...emotions.keys()],

        // A scene object as in engine/tests/scenes.json, frozen by lp_eyes_set_scene
        renderScene(scene, look) {
            setLook(look);
            const s = sceneFields(scene);
            // In the order of sceneFields, which is that of lpw_set_scene's parameters (engine.test.mjs)
            const args = {
                ...s,
                emotion: emotions.get(s.emotion) ?? -1, // the engine ignores a scene it does not know
                blink_phase: BLINK_PHASES.indexOf(s.blink_phase),
                has_particles: s.has_particles ? 1 : 0
            };
            m._lpw_set_scene(...Object.values(args));
            return render();
        },

        // Live animation, for the PC app
        init: (seed) => m._lpw_init(seed),
        setLook,
        setEmotion: (name) => {
            if (emotions.has(name)) m._lpw_set_emotion(emotions.get(name));
        },
        blink: () => m._lpw_blink(),
        setPointer: (point) => m._lpw_set_pointer(point ? 1 : 0, point?.x ?? 0, point?.y ?? 0),
        setGravity: (g) => m._lpw_set_gravity(g ? 1 : 0, g?.x ?? 0, g?.y ?? 0),
        setShaking: (shaking) => m._lpw_set_shaking(shaking ? 1 : 0),
        // Follow, shape and head amount: kept for the app's future eye editor (smoke.mjs checks they reach the module)
        setFollow: (follow) => m._lpw_set_follow(follow ? 1 : 0),
        setShape: ({ rx, ry, gap, tilt }) => m._lpw_set_shape(rx, ry, gap, tilt),
        setHeadAmount: (amount) => m._lpw_set_head_amount(amount),
        react: (event) => {
            if (events.has(event)) m._lpw_react(events.get(event));
        },
        emotion: () => names[m._lpw_emotion()],
        update: (dt) => m._lpw_update(dt),
        render,
        renderView,
        dirty
    };
}
