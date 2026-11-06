// The prototype's state, in Node (no DOM): the limits it shares with the C engine, which the parity vectors cannot reach
// (a vector replays the same steps on both sides, with valid inputs).
import { expect, test } from '@playwright/test';
import { EMOTIONS, Eyes, REACTIONS, createRng } from '../prototype/eyes.js';

const MAX_PARTICLES = 32; // LP_MAX_PARTICLES
const SPARKLE_DRAWS = 5; // random numbers spawn() takes for a sparkle

// What update() changes
const state = ({ time, cur, gaze, head, blinkState, emotion, emotionTimer, particles }) =>
    structuredClone({ time, cur, gaze, head, blinkState, emotion, emotionTimer, particles });

test('update ignores a step that is not positive and finite', () => {
    const eyes = new Eyes(1);
    eyes.setEmotion('love');
    eyes.update(1 / 64);
    const before = state(eyes);
    for (const dt of [0, -1, Number.NaN, Infinity, -Infinity]) eyes.update(dt);
    expect(state(eyes)).toEqual(before);
});

test('a long update counts for one second', () => {
    const capped = new Eyes(1);
    const oneSecond = new Eyes(1);
    capped.setEmotion('love');
    oneSecond.setEmotion('love');
    capped.update(1e9); // would spawn three billion hearts
    oneSecond.update(1);
    expect(state(capped)).toEqual(state(oneSecond));
    expect(capped.time).toBe(1); // what is left of the step, for the next deadlines too
});

test('particles stop at the limit of the engine, after their random draws', () => {
    const eyes = new Eyes(5);
    const taps = 12;
    for (let i = 0; i < taps; i++) eyes.react('tap');
    expect(eyes.particles).toHaveLength(MAX_PARTICLES);

    const sequence = createRng(5);
    for (let i = 0; i < taps * REACTIONS.tap.sparkles * SPARKLE_DRAWS; i++) sequence();
    expect(eyes.rand()).toBe(sequence());
});

test('an emotion without a positive duration is the new mood, as in the engine', () => {
    for (const duration of [0, -1]) {
        const eyes = new Eyes(1);
        eyes.setEmotion('sad', duration);
        expect(eyes.mood).toBe('sad');
    }
    const eyes = new Eyes(1);
    eyes.setEmotion('sad', 1);
    expect(eyes.mood).toBe('neutral');
});

test('a duration that is not finite is ignored, as in the engine', () => {
    for (const duration of [Number.NaN, Infinity, -Infinity]) {
        const eyes = new Eyes(1);
        eyes.setEmotion('sad', duration);
        expect(eyes.emotion).toBe('neutral');
        expect(eyes.mood).toBe('neutral');
    }
});

test('unknown emotions and events are ignored, as in the engine', () => {
    const eyes = new Eyes(1);
    for (const name of ['furious', 'toString', undefined]) {
        eyes.setEmotion(name);
        eyes.react(name);
    }
    expect(eyes.emotion).toBe('neutral');
    eyes.update(1 / 64); // throws on an emotion without a row
    expect(Object.hasOwn(EMOTIONS, eyes.mood)).toBe(true);
});

test('the pointer stays inside its range', () => {
    const eyes = new Eyes(1);
    eyes.setPointer({ x: 2, y: -2 });
    expect(eyes.mouse).toEqual({ x: 1, y: -1 });
});

test('points that are not finite are dropped, as in the engine', () => {
    const eyes = new Eyes(1);
    for (const bad of [
        { x: Number.NaN, y: 0 },
        { x: 0, y: Infinity }
    ]) {
        eyes.setPointer({ x: 0.5, y: 0.5 });
        eyes.setGravity({ x: 0, y: 1 });
        eyes.setPointer(bad);
        eyes.setGravity(bad);
        expect(eyes.mouse).toBeNull();
        expect(eyes.gravity).toBeNull();
    }
});

test('a held reaction outlives its cause by its duration, then the mood comes back', () => {
    const eyes = new Eyes(1);
    const dt = 1 / 64;
    const run = (seconds) => {
        for (let i = 0; i < seconds / dt; i++) eyes.update(dt);
    };
    eyes.shaking = true;
    run(3); // longer than the reaction: it is renewed
    expect(eyes.emotion).toBe('dizzy');
    eyes.shaking = false;
    run(REACTIONS.shake.duration - 0.25);
    expect(eyes.emotion).toBe('dizzy');
    run(0.5);
    expect(eyes.emotion).toBe('neutral');
});

test('a mood that is already the reaction stays after it', () => {
    const eyes = new Eyes(1);
    const dt = 1 / 64;
    eyes.setEmotion('dizzy');
    eyes.shaking = true;
    eyes.update(dt);
    eyes.shaking = false;
    for (let i = 0; i < 4 / dt; i++) eyes.update(dt);
    expect(eyes.emotion).toBe('dizzy');
    expect(eyes.mood).toBe('dizzy');
});
