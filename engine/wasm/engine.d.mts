// Types of engine.mjs, for the TypeScript PC app (app/): only what the app uses, the module offers more.
import type emotions from '../../eyes/emotions.json';

export const PUPILS: readonly ['dot', 'cat', 'acorn', 'circle'];

export type PupilType = (typeof PUPILS)[number];

// Names of lp_event: the reactions of eyes/emotions.json, so the two cannot drift apart
export type EyeEvent = keyof (typeof emotions)['reactions'];

// A swatch of eyes/looks.json worn with a pupil type (makeLook in app/src/data/looks.ts)
export interface Look {
    id?: string;
    sclera: string;
    pupil: PupilType;
    pupilColor: readonly [string, string];
}

// A frozen frame: the part of a reference scene (engine/tests/scenes.json) the app sets
export interface Scene {
    emotion?: string;
}

// A region of the screen, in pixels
export interface Region {
    x: number;
    y: number;
    width: number;
    height: number;
}

export interface Engine {
    size: number;
    emotions: string[];
    renderScene(scene: Scene, look: Look): Uint8ClampedArray<ArrayBuffer>;
    init(seed: number): void;
    // Unknown pupil, emotion and event names are ignored
    setLook(look: Look): void;
    setEmotion(name: string): void;
    blink(): void;
    setPointer(point: { x: number; y: number } | null): void;
    react(event: EyeEvent): void;
    update(dt: number): void;
    // RGBA pixels of the whole screen (size × size × 4), without a copy, in the engine's memory: valid until the next
    // render
    renderView(): Uint8ClampedArray<ArrayBuffer>;
    // The region the last render changed: the rest of the frame is the one before
    dirty(): Region;
}

export function loadEngine(moduleUrl: string): Promise<Engine>;
