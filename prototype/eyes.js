// Eyes of the design prototype: state and animation, no DOM access, so that Node can import them (draw.js draws them).
// It is the visual reference for the C engine (see notes/architecture.md).
// Units are logical pixels of the 466×466 round screen.
import emotionData from '../eyes/emotions.json' with { type: 'json' };

export const SIZE = 466;
const C = SIZE / 2;

// Eye shape measured on the official gallery (see notes/yeux.md)
const DEFAULT_SHAPE = { rx: 93, ry: 109, gap: 101, tilt: 0.37 };

export const BLINK_PHASES = ['anticip', 'close', 'closed', 'open'];

// A look is a swatch from eyes/looks.json worn with one of the pupil types: the object setLook and setScene take
export function makeLook(swatch, pupil) {
    return { id: swatch.id, sclera: swatch.sclera, pupil, pupilColor: swatch.pupilColor };
}

// --- Emotions: eyelid shapes and pupil size ---------------------------------
// tl: top lid closure (0-1), ts: top lid slope (+ = inner corner lower, - = outer corner lower)
// bl: bottom lid closure (curved, "happy" shape), sq: vertical squash (squint into a rounded bar)
// cr: crescent cut (0-1, only the top arc of the eye remains, like a frown)
// rot: extra eye rotation, inner corner down (rad, tuned for the default 0.37 tilt), hl: pupil glint (0/1)
// ps: pupil scale, es: eye scale, gy: gaze bias down, spin: dizzy gaze circling (rad/s, DIZZY)
// emit: particles spawned while the emotion is on ({ type, rate per second })
export const BASE = { tl: 0, ts: 0, bl: 0, sq: 0, cr: 0, rot: 0, hl: 0, ps: 1, es: 1, gy: 0, spin: 0 };
export const EMOTIONS = Object.fromEntries(
    Object.entries(emotionData.emotions).map(([name, emotion]) => [name, { ...BASE, ...emotion }])
);
// Dizzy: the gaze circles (radius, in gaze units) and the head wobbles (rad), in proportion to spin / max; the spin
// wears off slower than it comes, so the eyes calm down after the emotion is over
const DIZZY = { max: 9, radius: 0.55, wobble: 0.12, wearOff: 1.5 };

// How the eyes react to what happens to the object: an emotion for a while, and a burst of sparkles. Data, not code,
// so that the PC app can change them later. The table of the C engine comes from the same file (emotions_table.h).
export const REACTIONS = emotionData.reactions;
const BURST_GAP = 0.5; // seconds: a burst of sparkles fills the screen if repeated any faster (same as the engine)
// Upside down: gravity toward the top of the screen (in g, -1 fully upside down), held this many seconds
const UPSIDE_DOWN = { gravity: -0.6, delay: 0.6 };

// Blink, from the official footage (~37 ms per frame): slight squash, fast close with a straight
// top lid, thin arc while closed, slower reopening. See notes/yeux.md.
const BLINK = { anticip: 0.06, close: 0.1 };

// Virtual 3D face: eye depth, camera distance, eyes facing slightly outward
const FACE = { depth: 130, camera: 600, outward: 0.35 };

// Particles alive at once, as many as the engine holds (LP_MAX_PARTICLES)
const MAX_PARTICLES = 32;

// The emitter debt often reaches exactly 1 (rate × dt sums), where float (C) and double (JS) rounding disagree:
// the margin makes both spawn on the same frame.
const EMIT_EPSILON = 1e-4;

// Seeded PRNG (mulberry32): the C engine uses the same algorithm, so a seed gives the same sequence in both.
export function createRng(seed) {
    let a = seed >>> 0;
    return () => {
        a = (a + 0x6d2b79f5) >>> 0;
        let t = a;
        t = Math.imul(t ^ (t >>> 15), t | 1);
        t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
        return ((t ^ (t >>> 14)) >>> 8) / 16777216; // top 24 bits: exact in a C float, so JS and C agree
    };
}

// Spring-like smoothing, frame-rate independent.
const approach = (v, target, k, dt) => v + (target - v) * (1 - Math.exp(-k * dt));
const easeIn = (p) => p * p;
const easeOut = (p) => 1 - (1 - p) ** 3;
export const clamp = (v, lim) => Math.max(-lim, Math.min(lim, v));

const TAU = Math.PI * 2;
const TILT_SLIDE = 0.5; // gaze shift toward the low side, at full tilt
const MAX_STEP = 1; // seconds, the longest update: after that the state has settled anyway (same as the engine)

// The angle equal to `target` modulo a turn that is nearest to `from`, so that easing toward it never spins around
const nearestTurn = (target, from) => target + TAU * Math.floor((from - target) / TAU + 0.5);
const wrapAngle = (a) => (Math.abs(a) > Math.PI ? a - TAU * Math.sign(a) : a);
// A NaN would spread through the easing to the whole state, for good: points that are not finite are dropped
const finitePoint = (p) => (p && Number.isFinite(p.x) && Number.isFinite(p.y) ? p : null);
const settled = (emotion) => Object.fromEntries(Object.keys(BASE).map((k) => [k, EMOTIONS[emotion][k]]));

const SCENE_MAX_TIME = 60; // seconds of particles to replay: a larger time would block for as long (same as the engine)

// A scene with an unknown emotion or blink phase, a value that is not finite or a time out of range (same as the engine)
function sceneIsValid({ emotion, blink, head, gaze, particles }) {
    const phase = blink?.phase ?? null;
    const time = particles?.time ?? 0;
    const values = [blink?.t, head?.yaw, head?.pitch, head?.roll, gaze?.x, gaze?.y];
    return (
        Object.hasOwn(EMOTIONS, emotion ?? 'neutral') &&
        (phase === null || BLINK_PHASES.includes(phase)) &&
        values.every((v) => Number.isFinite(v ?? 0)) &&
        time >= 0 &&
        time <= SCENE_MAX_TIME
    );
}

export class Eyes {
    constructor(seed) {
        this.onEmotion = null; // called with the new emotion, also when a temporary one ends
        this.#start(seed);
    }

    // A fresh start, for the constructor and setScene: lp_eyes_init in the engine
    #start(seed) {
        this.rand = createRng(seed);
        this.time = 0; // seconds of animation: the steps of update(), after their limit

        this.look = null;
        this.emotion = 'neutral';
        this.mood = 'neutral'; // the emotion picked for good: temporary ones come back to it
        this.emotionTimer = 0; // > 0: seconds before going back to the mood, which may be this very emotion
        this.burstWait = 0; // > 0: sparkles just burst, no reaction brings more yet
        this.upsideDown = 0; // seconds held upside down
        this.shaking = false; // being shaken (the device tells it: lp_shake_update)
        this.cur = settled('neutral'); // animated emotion values

        this.shape = { ...DEFAULT_SHAPE };
        this.gaze = { x: 0, y: 0, tx: 0, ty: 0, vx: 0, vy: 0 };
        this.follow = true; // follow `mouse` (face tracking stand-in) instead of idle saccades
        this.mouse = null; // normalized pointer position, null when outside
        this.nextSaccade = 0;
        this.pendingGaze = null; // gaze change hidden behind the next blink
        this.gravity = null; // accelerometer, in the screen plane (x right, y down, in g), null without one

        // Head pose (radians). Eyes sit on a virtual 3D face: turning makes the near eye bigger and
        // the far one smaller and flatter, tilting rotates the pair. The head follows the gaze, slower.
        this.head = { yaw: 0, pitch: 0, roll: 0, rollT: 0, level: 0 }; // level: tilt roll, eased
        this.headAmount = 0.6;
        this.nextRoll = 0;
        this.spinPhase = 0; // where the dizzy gaze is on its circle (rad)

        this.blinkState = { phase: null, t: 0, hold: 0, open: 0, double: false };
        this.nextBlink = 1.5;

        // Particles: hearts rising from the bottom, twinkling 4-point sparkles
        this.particles = [];
        this.emitDebt = 0; // fractional particles owed by the current emitter
    }

    setLook(look) {
        this.look = look;
    }

    // duration > 0: back to the mood after that many seconds; otherwise the new mood. Unknown emotions and durations
    // that are not finite are ignored.
    setEmotion(name, duration = 0) {
        if (!Object.hasOwn(EMOTIONS, name) || !Number.isFinite(duration)) return;
        this.emotion = name;
        this.emotionTimer = duration;
        if (!(duration > 0)) this.mood = name;
        this.onEmotion?.(name);
    }

    setPointer(point) {
        const p = finitePoint(point);
        this.mouse = p && { x: clamp(p.x, 1), y: clamp(p.y, 1) };
    }

    setGravity(gravity) {
        this.gravity = finitePoint(gravity);
    }

    // Unknown events are ignored. The emotion changes at once; one already on lasts at least as long as the event asks.
    // Sparkles burst half a second apart at most, whatever the emotion, and a reaction with none leaves that wait alone:
    // a camera can repeat itself as often as it likes.
    react(event) {
        if (!Object.hasOwn(REACTIONS, event)) return;
        const r = REACTIONS[event];
        if (this.emotion === r.emotion) this.emotionTimer = Math.max(this.emotionTimer, r.duration);
        else this.setEmotion(r.emotion, r.duration);
        if (r.sparkles === 0 || this.burstWait > 0) return;
        this.burstWait = BURST_GAP;
        for (let i = 0; i < r.sparkles; i++) this.spawn('sparkle');
    }

    startBlink() {
        if (this.blinkState.phase) return;
        const sleepy = this.emotion === 'sleepy';
        Object.assign(this.blinkState, {
            phase: 'anticip',
            t: 0,
            hold: (0.08 + this.rand() * 0.2) * (sleepy ? 2.5 : 1),
            open: (0.17 + this.rand() * 0.06) * (sleepy ? 2 : 1)
        });
    }

    spawn(type) {
        const r = this.rand;
        let particle;
        if (type === 'heart') {
            particle = {
                type,
                x: C + (r() - 0.5) * 300,
                y: SIZE - 40 + r() * 30,
                vy: -(45 + r() * 35),
                sway: r() * Math.PI * 2,
                size: 12 + r() * 7,
                age: 0,
                life: 2.6 + r() * 1.4
            };
        } else {
            // Sparkles pop around the eyes, never on top of them
            const a = r() * Math.PI * 2,
                dist = 150 + r() * 60;
            particle = {
                type,
                x: C + Math.cos(a) * dist,
                y: C + Math.sin(a) * dist * 0.8,
                size: 6 + r() * 7,
                spin: (r() - 0.5) * 2,
                age: 0,
                life: 0.6 + r() * 0.5
            };
        }
        // After the random draws, so that a full screen leaves the random sequence as it is (same as the engine)
        if (this.particles.length < MAX_PARTICLES) this.particles.push(particle);
    }

    #updateParticles(dt) {
        const emit = EMOTIONS[this.emotion].emit;
        if (emit) {
            this.emitDebt += emit.rate * dt;
            for (; this.emitDebt >= 1 - EMIT_EPSILON; this.emitDebt--) this.spawn(emit.type);
        } else this.emitDebt = 0;
        const ps = this.particles;
        for (let i = ps.length - 1; i >= 0; i--) {
            const p = ps[i];
            p.age += dt;
            if (p.age > p.life) {
                ps.splice(i, 1);
                continue;
            }
            if (p.type === 'heart') {
                p.y += p.vy * dt;
                p.x += Math.sin(p.age * 3 + p.sway) * 18 * dt;
            }
        }
    }

    #updateBlink(dt, t) {
        const b = this.blinkState;
        if (!b.phase) return;
        b.t += dt;
        const next = (phase) => {
            b.phase = phase;
            b.t = 0;
        };
        if (b.phase === 'anticip' && b.t > BLINK.anticip) next('close');
        else if (b.phase === 'close' && b.t > BLINK.close) {
            next('closed');
            // Eyes are hidden: jump to the new gaze without showing the move
            if (this.pendingGaze) {
                const { gaze } = this;
                gaze.tx = gaze.x = this.pendingGaze.x;
                gaze.ty = this.pendingGaze.y;
                gaze.y = this.pendingGaze.y + this.cur.gy;
                this.pendingGaze = null;
            }
        } else if (b.phase === 'closed' && b.t > b.hold) next('open');
        else if (b.phase === 'open' && b.t > b.open) {
            b.phase = null;
            if (b.double) {
                b.double = false;
                this.nextBlink = t + 0.1;
            }
        }
    }

    #easeExpression(dt) {
        const { cur } = this;
        const target = EMOTIONS[this.emotion];
        for (const key in BASE) {
            const wearOff = key === 'spin' && target.spin < cur.spin;
            cur[key] = approach(cur[key], target[key], wearOff ? DIZZY.wearOff : 9, dt);
        }
    }

    #dizziness() {
        return Math.min(this.cur.spin / DIZZY.max, 1);
    }

    // A reaction that goes on as long as its cause, and its duration after. A mood that is already that emotion stays
    // as it is: the timer only brings it back to itself. This runs every frame, so react() only starts it: sparkles in
    // the table burst once, not every BURST_GAP for as long as the cause lasts.
    #holdReaction(event) {
        const r = REACTIONS[event];
        if (this.emotion !== r.emotion) this.react(event);
        else this.emotionTimer = Math.max(this.emotionTimer, r.duration);
    }

    // Held upside down: angry as long as it lasts; shaken: dizzy, even upside down (one reaction at a time)
    #updateHeldReactions(dt) {
        this.upsideDown = this.gravity?.y < UPSIDE_DOWN.gravity ? this.upsideDown + dt : 0;
        if (this.shaking) this.#holdReaction('shake');
        else if (this.upsideDown >= UPSIDE_DOWN.delay) this.#holdReaction('upside_down');
    }

    // Tilting the object turns the face to stay level and slides the eyes toward the low side; returns the slide. The
    // level roll eases in as fast as it can be trusted: it holds lying flat, where the angle is mostly noise, so the
    // face never flips back and forth; without accelerometer it goes back to upright.
    #updateTilt(dt) {
        const { gravity: g, head } = this;
        if (!g) {
            head.level = wrapAngle(approach(head.level, 0, 3, dt));
            return 0;
        }
        // Trust: 0 lying flat, where little gravity is left in the screen plane, 1 standing up
        const w = Math.min(Math.max((Math.hypot(g.x, g.y) - 0.3) / 0.4, 0), 1);
        const a = Math.atan2(g.x, g.y); // 0 upright, > 0 turned clockwise
        head.level = wrapAngle(approach(head.level, nearestTurn(-a, head.level), 10 * w, dt));
        return Math.sin(a) * w;
    }

    // Gaze target: pointer (face tracking stand-in) or idle saccades. Dizzy, the gaze circles instead. Returns where the
    // gaze aims, shifted sideways by the tilt slide.
    #updateGaze(dt, t, slide) {
        const { cur, gaze, blinkState } = this;
        const tracking = this.follow && this.mouse;
        if (tracking) {
            gaze.tx = this.mouse.x;
            gaze.ty = this.mouse.y;
        } else if (t > this.nextSaccade) {
            const r = this.rand() < 0.3 ? 0 : 0.3 + this.rand() * 0.6; // often return to center
            const a = this.rand() * Math.PI * 2;
            const tx = Math.cos(a) * r,
                ty = Math.sin(a) * r * 0.7;
            // Big gaze jumps are often hidden behind a blink, like in the original
            if (Math.hypot(tx - gaze.tx, ty - gaze.ty) > 0.5 && !blinkState.phase && this.rand() < 0.5) {
                this.pendingGaze = { x: tx, y: ty };
                this.startBlink();
            } else {
                gaze.tx = tx;
                gaze.ty = ty;
            }
            this.nextSaccade = t + 0.6 + this.rand() * 2.4;
        }
        // Dizzy: the gaze circles instead, as fast as the spin
        let tx = gaze.tx,
            ty = gaze.ty;
        const dizzy = this.#dizziness();
        if (dizzy > 0) {
            this.spinPhase += cur.spin * dt;
            if (this.spinPhase > TAU) this.spinPhase -= TAU;
            tx += (Math.cos(this.spinPhase) * DIZZY.radius - tx) * dizzy;
            ty += (Math.sin(this.spinPhase) * DIZZY.radius * 0.7 - ty) * dizzy;
        }
        const aimX = clamp(tx + slide * TILT_SLIDE, 1);
        const px = gaze.x,
            py = gaze.y;
        const k = tracking ? 14 : 22;
        gaze.x = approach(gaze.x, aimX, k, dt);
        gaze.y = approach(gaze.y, ty + cur.gy, k, dt);
        gaze.vx = (gaze.x - px) / dt;
        gaze.vy = (gaze.y - py) / dt;
        return { x: aimX, y: ty };
    }

    update(dt) {
        if (!(dt > 0) || !Number.isFinite(dt)) return; // nothing to advance, and speeds would divide by zero
        dt = Math.min(dt, MAX_STEP);
        this.time += dt;
        const t = this.time;
        const { cur, head, blinkState } = this;

        this.#easeExpression(dt);
        if (this.emotionTimer > 0 && (this.emotionTimer -= dt) <= 0) this.setEmotion(this.mood);
        this.burstWait = Math.max(this.burstWait - dt, 0);
        this.#updateHeldReactions(dt);

        const aim = this.#updateGaze(dt, t, this.#updateTilt(dt));

        // Blinks at irregular intervals, sometimes double
        if (t > this.nextBlink && !blinkState.phase) {
            this.startBlink();
            blinkState.double = this.rand() < 0.2;
            this.nextBlink = t + (this.emotion === 'sleepy' ? 1.5 : 2) + this.rand() * 4;
        }
        this.#updateBlink(dt, t);
        this.#updateParticles(dt);

        // Head follows the gaze target, faster while the eyes are shut (hidden pose change)
        const hk = blinkState.phase ? 12 : 4;
        head.yaw = approach(head.yaw, aim.x * 0.55 * this.headAmount, hk, dt);
        head.pitch = approach(head.pitch, (aim.y + cur.gy) * 0.4 * this.headAmount, hk, dt);
        if (t > this.nextRoll) {
            head.rollT = (this.rand() - 0.5) * 0.4;
            this.nextRoll = t + 2 + this.rand() * 4;
        }
        const wobble = Math.sin(this.spinPhase) * DIZZY.wobble * this.#dizziness();
        const roll = head.rollT * this.headAmount - head.yaw * 0.4 + head.level + wobble;
        head.roll = wrapAngle(approach(head.roll, nearestTurn(roll, head.roll), 3, dt));
    }

    // Starts over, as new Eyes(seed) does with the seed of its particles, in the state of a scene (format:
    // prototype/scenes.json): emotion settled, gaze still, particles replayed from their seed. A scene that is not valid
    // is ignored. Same as lp_eyes_set_scene in the C engine.
    setScene(scene, look) {
        if (!sceneIsValid(scene)) return;
        const { blink, particles } = scene;
        const emotion = scene.emotion ?? 'neutral';
        this.#start(particles?.seed ?? 0);
        this.look = look;
        this.emotion = emotion;
        this.mood = emotion;
        this.cur = settled(emotion);
        Object.assign(this.head, scene.head);
        const g = { x: 0, y: 0, ...scene.gaze };
        Object.assign(this.gaze, { x: g.x, y: g.y + this.cur.gy, tx: g.x, ty: g.y });
        Object.assign(this.blinkState, { phase: blink?.phase ?? null, t: blink?.t ?? 0, hold: 0.2, open: 0.2 });
        if (particles) {
            const steps = Math.round(particles.time * 60);
            for (let i = 0; i < steps; i++) this.#updateParticles(1 / 60);
        }
    }

    // Lid closure (0 open, 1 shut) and eye scale for the current blink frame
    blinkPose() {
        const { phase, t, open } = this.blinkState;
        switch (phase) {
            case 'anticip': {
                const p = Math.min(t / BLINK.anticip, 1);
                return { lid: 0.1 * p, sx: 1 + 0.05 * p, sy: 1 - 0.03 * p, closed: false };
            }
            case 'close': {
                const p = easeIn(Math.min(t / BLINK.close, 1));
                return { lid: 0.1 + 0.9 * p, sx: 1.05, sy: 0.97, closed: false };
            }
            case 'closed':
                return { lid: 1, sx: 1.04, sy: 1, closed: true };
            case 'open': {
                const p = easeOut(Math.min(t / open, 1));
                return { lid: 1 - p, sx: 1.04 - 0.04 * p, sy: 0.97 + 0.03 * p, closed: false };
            }
            default:
                return { lid: 0, sx: 1, sy: 1, closed: false };
        }
    }

    // Screen position, perspective scale and foreshortening of one eye for the current head pose
    eyePose(side) {
        const { depth, camera, outward } = FACE;
        const { yaw, pitch, roll } = this.head;
        const cy = Math.cos(yaw),
            sy = Math.sin(yaw),
            cp = Math.cos(pitch),
            sp = Math.sin(pitch);
        const rotate = (x, y, z) => {
            const x1 = x * cy + z * sy,
                z1 = -x * sy + z * cy; // yaw
            return [x1, y * cp + z1 * sp, -y * sp + z1 * cp]; // pitch
        };
        const [x, y, z] = rotate(side * this.shape.gap, 0, depth);
        const persp = camera / (camera - z) / (camera / (camera - depth)); // 1 at rest
        // Eye surface normal: flatter on screen as it turns away from the viewer
        const n = Math.hypot(outward, 1);
        const [nx, ny] = rotate((side * outward) / n, 0, 1 / n);
        const fx = Math.sqrt(1 - nx * nx) / Math.sqrt(1 - (outward / n) ** 2);
        const fy = Math.sqrt(1 - ny * ny);
        // Roll turns the whole face around the screen center
        const cr = Math.cos(roll),
            sr = Math.sin(roll);
        const px = x * persp,
            py = y * persp;
        return { x: C + px * cr - py * sr, y: C + px * sr + py * cr, persp, fx, fy };
    }
}
