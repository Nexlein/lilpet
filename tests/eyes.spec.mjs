// The prototype's state, in Node (no DOM): the limits it shares with the C engine, which the parity vectors cannot reach
// (a vector replays the same steps on both sides, with valid inputs).
import { expect, test } from '@playwright/test';
import { EMOTIONS, Eyes, REACTIONS, createRng } from '../prototype/eyes.js';

const MAX_PARTICLES = 32; // LP_MAX_PARTICLES
const SPARKLE_DRAWS = 5; // random numbers spawn() takes for a sparkle

// Everything but the random generator, a closure, and the listener
const fields = (eyes) => ({ ...eyes, rand: null, onEmotion: null });

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
    const sparkles = MAX_PARTICLES + 16;
    for (let i = 0; i < sparkles; i++) eyes.spawn('sparkle');
    expect(eyes.particles).toHaveLength(MAX_PARTICLES);

    const sequence = createRng(5);
    for (let i = 0; i < sparkles * SPARKLE_DRAWS; i++) sequence();
    expect(eyes.rand()).toBe(sequence());
});

test('the same emotion again within half a second goes on without a new burst, as in the engine', () => {
    const { gesture_like: like, tap } = REACTIONS;
    const eyes = new Eyes(1);
    eyes.react('gesture_like');
    expect(eyes.particles).toHaveLength(like.sparkles);

    for (let i = 0; i < 31; i++) eyes.update(1 / 64);
    eyes.rand = createRng(1);
    eyes.react('gesture_like');
    eyes.react('tap'); // another event of the same emotion, and a shorter one
    expect(eyes.rand()).toBe(createRng(1)()); // no random draw
    expect(eyes.particles).toHaveLength(like.sparkles);
    expect(eyes.emotionTimer).toBe(like.duration);

    eyes.update(1 / 64); // half a second after the burst
    eyes.react('tap');
    expect(eyes.particles).toHaveLength(like.sparkles + tap.sparkles);
    expect(eyes.emotionTimer).toBe(tap.duration);
});

test('another emotion reacts at once, as in the engine', () => {
    const eyes = new Eyes(1);
    eyes.react('gesture_like');
    eyes.react('gesture_dislike');
    expect(eyes.emotion).toBe(REACTIONS.gesture_dislike.emotion);
    eyes.react('gesture_like');
    expect(eyes.emotion).toBe(REACTIONS.gesture_like.emotion);
    expect(eyes.particles).toHaveLength(2 * REACTIONS.gesture_like.sparkles);
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

test('a scene starts over, as in the engine', () => {
    const scene = { emotion: 'love', head: { yaw: 0.25 }, gaze: { x: 0.5 }, particles: { seed: 3, time: 1 } };
    const fresh = new Eyes(1);
    fresh.setScene(scene, null);

    const eyes = new Eyes(1);
    eyes.onEmotion = console.log;
    eyes.setPointer({ x: 0.5, y: 0.5 });
    eyes.setGravity({ x: 0, y: -1 });
    eyes.shaking = true;
    eyes.follow = false;
    eyes.headAmount = 0;
    eyes.shape.rx = 50;
    for (let i = 0; i < 64; i++) eyes.update(1 / 64);
    eyes.setScene(scene, null);

    expect(eyes.onEmotion).toBe(console.log); // the listener stays
    expect(fields(eyes)).toEqual(fields(fresh));
    expect(eyes.rand()).toBe(fresh.rand());
});

test('a scene without particles starts from seed 0, as in the engine', () => {
    const eyes = new Eyes(1);
    eyes.setScene({}, null);
    expect(eyes.rand()).toBe(createRng(0)());
});

test('a scene that is not valid is ignored, as in the engine', () => {
    const eyes = new Eyes(1);
    eyes.setEmotion('love');
    eyes.update(1 / 64);
    const before = state(eyes);
    for (const scene of [
        { emotion: 'furious' },
        { emotion: 'toString' },
        { blink: { phase: 'wink', t: 0 } },
        { blink: { phase: 'open', t: Number.NaN } },
        { head: { yaw: Number.NaN } },
        { head: { pitch: Infinity } },
        { head: { roll: -Infinity } },
        { gaze: { x: Number.NaN } },
        { gaze: { y: Infinity } },
        { particles: { seed: 1, time: Number.NaN } },
        { particles: { seed: 1, time: 60.5 } },
        { particles: { seed: 1, time: -1 } }
    ]) {
        eyes.setScene(scene, null);
        expect(state(eyes)).toEqual(before);
    }
    eyes.setScene({ particles: { seed: 1, time: 60 } }, null); // the limit itself is a scene
    expect(eyes.time).toBe(0);
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
