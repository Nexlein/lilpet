// Types of engine.mjs, for the TypeScript PC app (app/).

export const PUPILS: readonly ['dot', 'cat', 'acorn', 'circle'];

export type PupilType = (typeof PUPILS)[number];

// Names of lp_event (reactions in eyes/emotions.json)
export type EyeEvent =
    | 'tap'
    | 'shake'
    | 'upside_down'
    | 'face_seen'
    | 'gesture_like'
    | 'gesture_dislike'
    | 'gesture_ok'
    | 'gesture_five'
    | 'gesture_call'
    | 'expression_happy'
    | 'expression_sad'
    | 'expression_angry'
    | 'expression_surprised';

// Same look object as the prototype (makeLook in prototype/eyes.js)
export interface Look {
    id?: string;
    sclera: string;
    pupil: PupilType;
    pupilColor: readonly [string, string];
}

export interface Shape {
    rx: number;
    ry: number;
    gap: number;
    tilt: number;
}

// One reference scene (prototype/scenes.json)
export interface Scene {
    emotion?: string;
    blink?: { phase: 'anticip' | 'close' | 'closed' | 'open'; t: number };
    head?: { yaw?: number; pitch?: number; roll?: number };
    gaze?: { x?: number; y?: number };
    particles?: { seed: number; time: number };
}

export interface Engine {
    version: string;
    size: number;
    emotions: string[];
    renderScene(scene: Scene, look: Look): Uint8ClampedArray<ArrayBuffer>;
    init(seed: number): void;
    // Unknown pupil, emotion and event names are ignored
    setLook(look: Look): void;
    setEmotion(name: string, duration?: number): void;
    blink(): void;
    setPointer(point: { x: number; y: number } | null): void;
    setGravity(gravity: { x: number; y: number } | null): void;
    setShaking(shaking: boolean): void;
    setFollow(follow: boolean): void;
    setShape(shape: Shape): void;
    setHeadAmount(amount: number): void;
    react(event: EyeEvent): void;
    emotion(): string;
    update(dt: number): void;
    // RGBA pixels of the whole screen (size × size × 4)
    render(): Uint8ClampedArray<ArrayBuffer>;
    // The same without a copy, in the engine's memory: valid until the next render
    renderView(): Uint8ClampedArray<ArrayBuffer>;
}

export function loadEngine(moduleUrl: string): Promise<Engine>;
