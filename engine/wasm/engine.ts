// TypeScript side of the WebAssembly engine: loads the module built by `mise run engine:wasm` and drives it with
// scene objects as in engine/tests/scenes.json and looks (a swatch of eyes/looks.json worn with a pupil type).
// Used by the PC app, the reference renders (scripts/goldens.ts), the data validator and smoke.ts; its scene fields
// also by the host scene table (scripts/gen-scenes-table.ts).
import type emotions from '../../eyes/emotions.json';

export const PUPILS = ['dot', 'cat', 'acorn', 'circle'] as const; // lp_pupil order
export const BLINK_PHASES = [null, 'anticip', 'close', 'closed', 'open'] as const; // lp_blink_phase order

export type PupilType = (typeof PUPILS)[number];
export type BlinkPhase = Exclude<(typeof BLINK_PHASES)[number], null>;

// Names of lp_event: the reactions of eyes/emotions.json, so the two cannot drift apart
export type EyeEvent = keyof (typeof emotions)['reactions'];

// A swatch of eyes/looks.json worn with a pupil type (makeLook in app/src/data/looks.ts)
export interface Look {
    id?: string;
    sclera: string;
    pupil: PupilType;
    pupilColor: readonly [string, string];
}

// A frozen frame: the state part of a reference scene (engine/tests/scenes.json), every field optional
export interface Scene {
    emotion?: string;
    blink?: { phase?: BlinkPhase; t?: number };
    head?: { yaw?: number; pitch?: number; roll?: number };
    gaze?: { x?: number; y?: number };
    particles?: { seed?: number; time?: number };
}

// The eye shape (lp_shape): radii, gap between the eyes and tilt, in pixels and radians
export interface Shape {
    rx: number;
    ry: number;
    gap: number;
    tilt: number;
}

// A region of the screen, in pixels
export interface Region {
    x: number;
    y: number;
    width: number;
    height: number;
}

export interface Engine {
    version: string; // MAJOR.MINOR.PATCH, as lp_version
    size: number;
    emotions: string[];
    // A scene object as in engine/tests/scenes.json, frozen by lp_eyes_set_scene
    renderScene(scene: Scene, look: Look): Uint8ClampedArray<ArrayBuffer>;
    // Live animation, for the PC app
    init(seed: number): void;
    // Unknown pupil, emotion and event names are ignored: they never reach the module, where they would become a number
    setLook(look: Look): void;
    setEmotion(name: string): void;
    blink(): void;
    setPointer(point: { x: number; y: number } | null): void;
    // Gravity in g on the screen's axes, as a motion sensor gives it, or null for none
    setGravity(gravity: { x: number; y: number } | null): void;
    setShaking(shaking: boolean): void;
    // Kept for the eye editor (#59): follow the pointer with the head, the eye shape, how much the head moves
    // (smoke.ts checks they reach the module)
    setFollow(follow: boolean): void;
    setShape(shape: Shape): void;
    setHeadAmount(amount: number): void;
    react(event: EyeEvent): void;
    // The emotion the eyes show now: the mood, or a reaction's for as long as it lasts
    emotion(): string;
    update(dt: number): void;
    // RGBA pixels of the whole screen (size × size × 4), as a copy to keep
    render(): Uint8ClampedArray<ArrayBuffer>;
    // The same, without a copy, in the engine's memory: valid until the next render
    renderView(): Uint8ClampedArray<ArrayBuffer>;
    // The region the last render changed: the rest of the frame is the one before
    dirty(): Region;
}

// What the Emscripten module exports (engine/wasm/bindings.h): pointers and C enums are numbers
interface LilpetModule {
    HEAPU8: Uint8Array<ArrayBuffer>;
    UTF8ToString(pointer: number): string;
    _lp_version(): number;
    _lpw_screen_size(): number;
    _lpw_emotion_count(): number;
    _lpw_emotion_name(emotion: number): number;
    _lpw_event_count(): number;
    _lpw_event_name(event: number): number;
    _lpw_init(seed: number): void;
    _lpw_set_look(sclera: number, pupil: number, left: number, right: number): void;
    _lpw_set_emotion(emotion: number): void;
    _lpw_blink(): void;
    _lpw_set_pointer(active: number, x: number, y: number): void;
    _lpw_set_gravity(active: number, x: number, y: number): void;
    _lpw_set_shaking(shaking: number): void;
    _lpw_set_follow(follow: number): void;
    _lpw_set_shape(rx: number, ry: number, gap: number, tilt: number): void;
    _lpw_set_head_amount(amount: number): void;
    _lpw_react(event: number): void;
    _lpw_emotion(): number;
    _lpw_update(dt: number): void;
    _lpw_set_scene(...fields: number[]): void;
    _lpw_render(): number;
    _lpw_dirty(): number;
}

const color = (hex: string) => Number.parseInt(hex.slice(1), 16);

// A scene object as in engine/tests/scenes.json, as the fields of lp_scene, by name. Read here for the reference renders
// and by scripts/gen-scenes-table.ts for the host tests: both draw the same state.
export const sceneFields = (scene: Scene) => ({
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
export async function loadEngine(moduleUrl: string): Promise<Engine> {
    const loaded: { default: () => Promise<LilpetModule> } = await import(/* @vite-ignore */ moduleUrl);
    const m = await loaded.default();
    const size = m._lpw_screen_size();
    const names = Array.from({ length: m._lpw_emotion_count() }, (_, i) => m.UTF8ToString(m._lpw_emotion_name(i)));
    const emotions = new Map(names.map((name, i) => [name, i]));
    const events = new Map(
        Array.from({ length: m._lpw_event_count() }, (_, i) => [m.UTF8ToString(m._lpw_event_name(i)), i])
    );

    const setLook = (look: Look) => {
        const pupil = PUPILS.indexOf(look.pupil);
        if (pupil < 0) return;
        m._lpw_set_look(color(look.sclera), pupil, color(look.pupilColor[0]), color(look.pupilColor[1]));
    };

    const renderView = () => new Uint8ClampedArray(m.HEAPU8.buffer, m._lpw_render(), size * size * 4);
    const render = () => renderView().slice();

    const dirty = () => {
        const [x0 = 0, y0 = 0, x1 = 0, y1 = 0] = new Int32Array(m.HEAPU8.buffer, m._lpw_dirty(), 4); // lp_rect
        return x0 < x1 && y0 < y1
            ? { x: x0, y: y0, width: x1 - x0, height: y1 - y0 }
            : { x: 0, y: 0, width: 0, height: 0 };
    };

    return {
        version: m.UTF8ToString(m._lp_version()),
        size,
        emotions: [...emotions.keys()],
        renderScene(scene, look) {
            setLook(look);
            const s = sceneFields(scene);
            // In the order of sceneFields, which is that of lpw_set_scene's parameters (engine.test.ts)
            const args = {
                ...s,
                emotion: emotions.get(s.emotion) ?? -1, // the engine ignores a scene it does not know
                blink_phase: BLINK_PHASES.indexOf(s.blink_phase),
                has_particles: s.has_particles ? 1 : 0
            };
            m._lpw_set_scene(...Object.values(args));
            return render();
        },
        init: (seed) => m._lpw_init(seed),
        setLook,
        setEmotion: (name) => {
            const emotion = emotions.get(name);
            if (emotion !== undefined) m._lpw_set_emotion(emotion);
        },
        blink: () => m._lpw_blink(),
        setPointer: (point) => m._lpw_set_pointer(point ? 1 : 0, point?.x ?? 0, point?.y ?? 0),
        setGravity: (g) => m._lpw_set_gravity(g ? 1 : 0, g?.x ?? 0, g?.y ?? 0),
        setShaking: (shaking) => m._lpw_set_shaking(shaking ? 1 : 0),
        setFollow: (follow) => m._lpw_set_follow(follow ? 1 : 0),
        setShape: ({ rx, ry, gap, tilt }) => m._lpw_set_shape(rx, ry, gap, tilt),
        setHeadAmount: (amount) => m._lpw_set_head_amount(amount),
        react: (event) => {
            const index = events.get(event);
            if (index !== undefined) m._lpw_react(index);
        },
        // lpw_emotion is always below lpw_emotion_count, the length of names (both come from the module)
        emotion: () => names[m._lpw_emotion()]!,
        update: (dt) => m._lpw_update(dt),
        render,
        renderView,
        dirty
    };
}
