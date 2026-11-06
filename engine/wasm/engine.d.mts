// Types of engine.mjs, for the TypeScript PC app (app/): only what the app uses, the module offers more.
import type emotions from '../../eyes/emotions.json';

export const PUPILS: readonly ['dot', 'cat', 'acorn', 'circle'];

export type PupilType = (typeof PUPILS)[number];

// Names of lp_event: the reactions of eyes/emotions.json, so the two cannot drift apart
export type EyeEvent = keyof (typeof emotions)['reactions'];

// Same look object as the prototype (makeLook in prototype/eyes.js)
export interface Look {
    id?: string;
    sclera: string;
    pupil: PupilType;
    pupilColor: readonly [string, string];
}

// A frozen frame: the part of a reference scene (prototype/scenes.json) the app sets
export interface Scene {
    emotion?: string;
}

export interface Engine {
    size: number;
    emotions: string[];
    renderScene(scene: Scene, look: Look): Uint8ClampedArray<ArrayBuffer>;
    init(seed: number): void;
    // Unknown pupil, emotion and event names are ignored
    setLook(look: Look): void;
    setEmotion(name: string, duration?: number): void;
    blink(): void;
    setPointer(point: { x: number; y: number } | null): void;
    react(event: EyeEvent): void;
    update(dt: number): void;
    // RGBA pixels of the whole screen (size × size × 4), without a copy, in the engine's memory: valid until the next
    // render
    renderView(): Uint8ClampedArray<ArrayBuffer>;
}

export function loadEngine(moduleUrl: string): Promise<Engine>;
