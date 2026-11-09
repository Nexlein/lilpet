// JavaScript side of the WebAssembly engine: loads the module built by `mise run engine:wasm` and drives it with
// scene objects as in engine/tests/scenes.json and looks (a swatch of eyes/looks.json worn with a pupil type).
// Used by the PC app, the reference renders (scripts/goldens.mjs), the data validator and smoke.mjs.

export const PUPILS = ['dot', 'cat', 'acorn', 'circle']; // lp_pupil order
export const BLINK_PHASES = [null, 'anticip', 'close', 'closed', 'open']; // lp_blink_phase order

const color = (hex) => Number.parseInt(hex.slice(1), 16);

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

    return {
        version: m.UTF8ToString(m._lp_version()),
        size,
        emotions: [...emotions.keys()],

        // A scene object as in engine/tests/scenes.json, frozen by lp_eyes_set_scene
        renderScene(scene, look) {
            setLook(look);
            m._lpw_set_scene(
                emotions.get(scene.emotion ?? 'neutral') ?? -1, // the engine ignores a scene it does not know
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
        setEmotion: (name) => {
            if (emotions.has(name)) m._lpw_set_emotion(emotions.get(name));
        },
        blink: () => m._lpw_blink(),
        setPointer: (point) => m._lpw_set_pointer(point ? 1 : 0, point?.x ?? 0, point?.y ?? 0),
        setGravity: (g) => m._lpw_set_gravity(g ? 1 : 0, g?.x ?? 0, g?.y ?? 0),
        setShaking: (shaking) => m._lpw_set_shaking(shaking ? 1 : 0),
        setFollow: (follow) => m._lpw_set_follow(follow ? 1 : 0),
        setShape: ({ rx, ry, gap, tilt }) => m._lpw_set_shape(rx, ry, gap, tilt),
        setHeadAmount: (amount) => m._lpw_set_head_amount(amount),
        react: (event) => {
            if (events.has(event)) m._lpw_react(events.get(event));
        },
        emotion: () => names[m._lpw_emotion()],
        update: (dt) => m._lpw_update(dt),
        render,
        renderView
    };
}
