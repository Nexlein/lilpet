// Eye renderer of the design prototype: state, animation and drawing, no DOM access.
// It is the visual reference for the C engine (see notes/architecture.md).
// Units are logical pixels of the 466×466 round screen.

export const SIZE = 466;
export const C = SIZE / 2;

// Eye shape measured on the official gallery (see notes/yeux.md)
export const DEFAULT_SHAPE = { rx: 93, ry: 109, gap: 101, tilt: 0.37 };

export const PUPIL_TYPES = ['dot', 'slit', 'acorn', 'round'];
export const BLINK_PHASES = ['anticip', 'close', 'closed', 'open'];

// --- Emotions: eyelid shapes and pupil size ---------------------------------
// tl: top lid closure (0-1), ts: top lid slope (+ = inner corner lower, - = outer corner lower)
// bl: bottom lid closure (curved, "happy" shape), sq: vertical squash (squint into a rounded bar)
// cr: crescent cut (0-1, only the top arc of the eye remains, like a frown)
// rot: extra eye rotation, inner corner down (rad, tuned for the default 0.37 tilt), hl: force the pupil glint (0/1)
// ps: pupil scale, es: eye scale, gy: gaze bias down
// emit: particles spawned while the emotion is on ({ type, rate per second })
export const BASE = { tl: 0, ts: 0, bl: 0, sq: 0, cr: 0, rot: 0, hl: 0, ps: 1, es: 1, gy: 0 };
export const EMOTIONS = {
    neutral: { label: 'Neutre' },
    happy: { label: 'Content', bl: 0.42, ps: 1.08, es: 1.02, emit: { type: 'sparkle', rate: 1.2 } },
    love: { label: 'Amoureux', bl: 0.45, ps: 1.1, es: 1.04, gy: -0.45, hl: 1, emit: { type: 'heart', rate: 3 } },
    angry: { label: 'Fâché', tl: 0.25, ts: 0.7, ps: 0.6, es: 0.96, gy: 0.15 },
    squint: { label: 'Plissé', tl: 0.12, sq: 0.68, rot: 0.37, ps: 0.8, es: 1.04 },
    frown: { label: 'Renfrogné', cr: 0.72, rot: 0.65, ps: 0.8, es: 1.04 },
    sad: { label: 'Triste', tl: 0.2, ts: -0.5, ps: 1.15, es: 0.98, gy: 0.45 },
    surprised: { label: 'Surpris', ps: 0.6, es: 1.12 },
    sleepy: { label: 'Endormi', tl: 0.55, ts: -0.1, bl: 0.05, gy: 0.35, emit: { type: 'sparkle', rate: 0.4 } }
};
for (const e of Object.values(EMOTIONS)) for (const k in BASE) e[k] ??= BASE[k];

// Resting pupil position, as a fraction of the eye radius: x toward the inner corner, y down.
// Measured on the official gallery: pupils converge, a slightly cross-eyed look.
const PUPIL_REST = { dot: [0.5, 0], slit: [0.35, 0], round: [0.3, 0.05], acorn: [0.15, 0.2] };

// Blink, from the official footage (~37 ms per frame): slight squash, fast close with a straight
// top lid, thin arc while closed, slower reopening. See notes/yeux.md.
const BLINK = { anticip: 0.06, close: 0.1 };

// Closed eye, measured on 7 frames of the official footage: the lower half of a flat, level ellipse (flat
// bottom, tips turning up), drawn as a stroke of even thickness with round tips. Fractions of the eye
// radii: half width of RX, lowest point and depth of RY (outer edge); thickness in pixels.
const CLOSED = { halfWidth: 0.76, bottom: 0.7, depth: 0.245, thickness: 4.5 };

// Virtual 3D face: eye depth, camera distance, eyes facing slightly outward
const FACE = { depth: 130, camera: 600, outward: 0.35 };

// Seeded PRNG (mulberry32): the C engine uses the same algorithm, so a seed gives the same sequence in both.
export function createRng(seed) {
    let a = seed >>> 0;
    return () => {
        a = (a + 0x6d2b79f5) >>> 0;
        let t = a;
        t = Math.imul(t ^ (t >>> 15), t | 1);
        t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
        return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
}

// Sizes the canvas backing store to the device pixel ratio and returns a context drawn in screen units.
export function setupCanvas(canvas, dpr = globalThis.devicePixelRatio || 1) {
    canvas.width = SIZE * dpr;
    canvas.height = SIZE * dpr;
    const ctx = canvas.getContext('2d');
    ctx.scale(dpr, dpr);
    return ctx;
}

// Spring-like smoothing, frame-rate independent.
const approach = (v, target, k, dt) => v + (target - v) * (1 - Math.exp(-k * dt));
const easeIn = (p) => p * p;
const easeOut = (p) => 1 - (1 - p) ** 3;
const clamp = (v, lim) => Math.max(-lim, Math.min(lim, v));
const settled = (emotion) => Object.fromEntries(Object.keys(BASE).map((k) => [k, EMOTIONS[emotion][k]]));

function drawHeart(ctx, s) {
    ctx.beginPath();
    ctx.moveTo(0, s * 0.9);
    ctx.bezierCurveTo(-s * 1.4, 0, -s * 0.9, -s * 1.1, 0, -s * 0.35);
    ctx.bezierCurveTo(s * 0.9, -s * 1.1, s * 1.4, 0, 0, s * 0.9);
    ctx.fill();
}

function drawSparkle(ctx, s) {
    ctx.beginPath();
    ctx.moveTo(0, -s);
    ctx.quadraticCurveTo(0, 0, s, 0);
    ctx.quadraticCurveTo(0, 0, 0, s);
    ctx.quadraticCurveTo(0, 0, -s, 0);
    ctx.quadraticCurveTo(0, 0, 0, -s);
    ctx.fill();
}

// Egg shape, wider at the bottom (official "acorn" pupil)
function acornPath(ctx, x, y, w, h) {
    ctx.beginPath();
    ctx.moveTo(x, y - h);
    ctx.bezierCurveTo(x + w * 0.75, y - h, x + w, y + h * 0.1, x + w, y + h * 0.35);
    ctx.bezierCurveTo(x + w, y + h * 0.85, x + w * 0.5, y + h, x, y + h);
    ctx.bezierCurveTo(x - w * 0.5, y + h, x - w, y + h * 0.85, x - w, y + h * 0.35);
    ctx.bezierCurveTo(x - w, y + h * 0.1, x - w * 0.75, y - h, x, y - h);
}

export class Eyes {
    constructor(ctx, { seed = Date.now() } = {}) {
        this.ctx = ctx;
        this.rand = createRng(seed);

        this.look = null;
        this.emotion = 'neutral';
        this.emotionTimer = 0; // > 0: temporary emotion, back to neutral when it hits 0
        this.cur = settled('neutral'); // animated emotion values
        this.onEmotion = null; // called with the new emotion, also when a temporary one ends

        this.shape = { ...DEFAULT_SHAPE };
        this.gaze = { x: 0, y: 0, tx: 0, ty: 0, vx: 0, vy: 0 };
        this.follow = true; // follow `mouse` (face tracking stand-in) instead of idle saccades
        this.mouse = null; // normalized pointer position, null when outside
        this.nextSaccade = 0;
        this.pendingGaze = null; // gaze change hidden behind the next blink

        // Head pose (radians). Eyes sit on a virtual 3D face: turning makes the near eye bigger and
        // the far one smaller and flatter, tilting rotates the pair. The head follows the gaze, slower.
        this.head = { yaw: 0, pitch: 0, roll: 0, rollT: 0 };
        this.headAmount = 0.6;
        this.nextRoll = 0;

        this.blinkState = { phase: null, t: 0, hold: 0, open: 0, double: false };
        this.nextBlink = 1.5;

        // Particles: hearts rising from the bottom, twinkling 4-point sparkles
        this.particles = [];
        this.emitDebt = 0; // fractional particles owed by the current emitter
    }

    setLook(look) {
        this.look = look;
    }

    setEmotion(name, duration = 0) {
        this.emotion = name;
        this.emotionTimer = duration;
        this.onEmotion?.(name);
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
        if (type === 'heart') {
            this.particles.push({
                type,
                x: C + (r() - 0.5) * 300,
                y: SIZE - 40 + r() * 30,
                vy: -(45 + r() * 35),
                sway: r() * Math.PI * 2,
                size: 12 + r() * 7,
                age: 0,
                life: 2.6 + r() * 1.4
            });
        } else {
            // Sparkles pop around the eyes, never on top of them
            const a = r() * Math.PI * 2,
                dist = 150 + r() * 60;
            this.particles.push({
                type,
                x: C + Math.cos(a) * dist,
                y: C + Math.sin(a) * dist * 0.8,
                size: 6 + r() * 7,
                spin: (r() - 0.5) * 2,
                age: 0,
                life: 0.6 + r() * 0.5
            });
        }
    }

    update(dt, t) {
        const { cur, gaze, head, blinkState } = this;

        // Emotion values ease toward their target
        const target = EMOTIONS[this.emotion];
        for (const key in BASE) cur[key] = approach(cur[key], target[key], 9, dt);
        if (this.emotionTimer > 0 && (this.emotionTimer -= dt) <= 0) this.setEmotion('neutral');

        // Gaze target: pointer (face tracking stand-in) or idle saccades
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
        const px = gaze.x,
            py = gaze.y;
        const k = tracking ? 14 : 22;
        gaze.x = approach(gaze.x, gaze.tx, k, dt);
        gaze.y = approach(gaze.y, gaze.ty + cur.gy, k, dt);
        gaze.vx = (gaze.x - px) / dt;
        gaze.vy = (gaze.y - py) / dt;

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
        head.yaw = approach(head.yaw, gaze.tx * 0.55 * this.headAmount, hk, dt);
        head.pitch = approach(head.pitch, (gaze.ty + cur.gy) * 0.4 * this.headAmount, hk, dt);
        if (t > this.nextRoll) {
            head.rollT = (this.rand() - 0.5) * 0.4;
            this.nextRoll = t + 2 + this.rand() * 4;
        }
        head.roll = approach(head.roll, head.rollT * this.headAmount - head.yaw * 0.4, 3, dt);
    }

    render(t) {
        const ctx = this.ctx;
        ctx.fillStyle = '#000';
        ctx.fillRect(0, 0, SIZE, SIZE);
        if (!this.look) return;

        const bp = this.#blinkPose();
        // Squash & stretch along the gaze movement, plus a slow "breathing"
        const { vx, vy } = this.gaze;
        const speed = Math.hypot(vx, vy);
        const st = Math.min(speed * 0.025, 0.12);
        const ax = speed ? Math.abs(vx) / speed : 0,
            ay = speed ? Math.abs(vy) / speed : 0;
        const breath = 1 + 0.012 * Math.sin(t * 2.1);
        const squash = { x: 1 + st * ax - st * ay * 0.5, y: (1 + st * ay - st * ax * 0.5) * breath };

        this.#drawEye(-1, bp, squash);
        this.#drawEye(1, bp, squash);
        this.#drawParticles();
    }

    // Renders one reference scene, fully determined by its fields (format: prototype/scenes.json).
    // Emotion values are settled, the gaze is still and particles are replayed from a fixed seed.
    renderScene(scene, look) {
        const emotion = scene.emotion ?? 'neutral';
        this.look = look;
        this.emotion = emotion;
        this.emotionTimer = 0;
        Object.assign(this.cur, settled(emotion));
        this.shape = { ...DEFAULT_SHAPE, ...scene.shape };
        Object.assign(this.head, { yaw: 0, pitch: 0, roll: 0, ...scene.head });
        const g = { x: 0, y: 0, ...scene.gaze };
        Object.assign(this.gaze, { x: g.x, y: g.y + this.cur.gy, tx: g.x, ty: g.y, vx: 0, vy: 0 });
        Object.assign(this.blinkState, {
            phase: scene.blink?.phase ?? null,
            t: scene.blink?.t ?? 0,
            hold: 0.2,
            open: 0.2,
            double: false
        });
        this.particles.length = 0;
        this.emitDebt = 0;
        if (scene.particles) {
            this.rand = createRng(scene.particles.seed);
            const steps = Math.round(scene.particles.time * 60);
            for (let i = 0; i < steps; i++) this.#updateParticles(1 / 60);
        }
        this.render(0);
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

    #updateParticles(dt) {
        const emit = EMOTIONS[this.emotion].emit;
        if (emit) {
            this.emitDebt += emit.rate * dt;
            for (; this.emitDebt >= 1; this.emitDebt--) this.spawn(emit.type);
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

    // Lid closure (0 open, 1 shut) and eye scale for the current blink frame
    #blinkPose() {
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
    #eyePose(side) {
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

    #drawEye(side, bp, squash) {
        // side: -1 = left eye, +1 = right eye. "Inner" corner is toward the screen center.
        const { ctx, cur, look } = this;
        const { rx: RX, ry: RY, tilt } = this.shape;
        const inner = -side;
        const pose = this.#eyePose(side);
        ctx.save();
        ctx.translate(pose.x, pose.y);
        ctx.rotate(this.head.roll);
        ctx.scale(pose.persp * pose.fx, pose.persp * pose.fy);
        ctx.rotate(inner * (cur.rot - tilt));
        ctx.scale(cur.es * squash.x * bp.sx * (1 + cur.sq * 0.1), cur.es * squash.y * bp.sy * (1 - cur.sq));

        if (bp.closed) {
            this.#drawClosedEye(RX, RY, inner * (cur.rot - tilt));
            ctx.restore();
            return;
        }

        // Eyelids are clip regions, not black covers: covering an anti-aliased edge leaves a faint outline.
        const W = RX + 10,
            FAR = RX + RY + 40;
        const keepBelow = (edge) => {
            ctx.beginPath();
            ctx.moveTo(-W, edge(-W));
            ctx.lineTo(W, edge(W));
            ctx.lineTo(W, FAR);
            ctx.lineTo(-W, FAR);
            ctx.closePath();
            ctx.clip();
        };

        ctx.beginPath();
        ctx.ellipse(0, 0, RX, RY, 0, 0, Math.PI * 2);
        ctx.clip();

        // Emotion top lid
        const y0 = -RY + cur.tl * 2 * RY;
        keepBelow((x) => y0 + cur.ts * RY * ((x * inner) / RX));

        // Crescent: a copy of the eye shifted down cuts it, leaving only the top arc
        if (cur.cr > 0.002) {
            ctx.beginPath();
            ctx.rect(-FAR, -FAR, 2 * FAR, 2 * FAR);
            ctx.ellipse(0, 2 * RY * (1 - cur.cr), RX * 1.05, RY, 0, 0, Math.PI * 2);
            ctx.clip('evenodd');
        }

        if (cur.bl > 0.002) {
            // Big ellipse rising from below gives the curved "^^" happy shape
            const R = RY * 1.1;
            ctx.beginPath();
            ctx.rect(-FAR, -FAR, 2 * FAR, 2 * FAR);
            ctx.ellipse(0, RY + R - cur.bl * 2 * RY, RX * 1.6, R, 0, 0, Math.PI * 2);
            ctx.clip('evenodd');
        }

        // Blink lid: straight edge coming down from the top, pupils stay put and get covered.
        // The edge stays level with the face, so it cancels the eye tilt.
        if (bp.lid > 0) {
            const m = Math.tan(inner * (tilt - cur.rot)),
                slack = Math.abs(m) * W;
            keepBelow((x) => -RY - slack + bp.lid * (2 * RY + 2 * slack) + m * x);
        }

        ctx.fillStyle = look.sclera;
        ctx.fillRect(-FAR, -FAR, 2 * FAR, 2 * FAR);
        this.#drawPupil(look.pupilColor[side < 0 ? 0 : 1], RX, RY, inner, inner * (cur.rot - tilt));
        ctx.restore();
    }

    #drawPupil(color, RX, RY, inner, uprightAngle) {
        const { ctx, gaze, look } = this;
        const ps = this.cur.ps;
        const type = look.pupil;
        let prx, pry;
        switch (type) {
            case 'dot':
                prx = RX * 0.105 * ps;
                pry = RY * 0.13 * ps;
                break;
            case 'slit':
                prx = RX * 0.17 * ps;
                pry = RY * 0.72;
                break;
            case 'acorn':
                prx = RX * 0.5 * ps;
                pry = RY * 0.88 * Math.min(ps, 1.05);
                break;
            case 'round':
                prx = pry = RX * 0.42 * ps;
                break;
        }
        // Pupil travel is limited by how much room is left inside the eye. Big acorn pupils may
        // run past the edge and get clipped, small ones stay fully visible.
        const [rx0, ry0] = PUPIL_REST[type];
        const limX = type === 'acorn' ? RX : RX - prx - 3,
            limY = type === 'acorn' ? RY : RY - pry - 3;
        const x = clamp(inner * rx0 * RX + gaze.x * Math.max(RX - prx, RX * 0.15) * 0.85, limX);
        const y = clamp(ry0 * RY + gaze.y * Math.max(RY - pry, RY * 0.12) * 0.85, limY);
        // The eye is tilted but pupils stay upright on the original: cancel the eye rotation
        ctx.rotate(-uprightAngle);
        ctx.fillStyle = color;
        if (type === 'acorn') acornPath(ctx, x, y, prx, pry);
        else {
            ctx.beginPath();
            ctx.ellipse(x, y, prx, pry, 0, 0, Math.PI * 2);
        }
        ctx.fill();
        if (look.highlight || this.cur.hl > 0.5) {
            const r = Math.max(Math.min(prx, pry) * 0.28, 4);
            ctx.fillStyle = '#fff';
            ctx.beginPath();
            ctx.arc(x - prx * 0.35, y - pry * 0.38, r, 0, Math.PI * 2);
            ctx.fill();
        }
    }

    // Closed eye: lower half of a flat ellipse, level with the face, stroked with round tips
    #drawClosedEye(RX, RY, uprightAngle) {
        const ctx = this.ctx;
        const th = CLOSED.thickness,
            a = CLOSED.halfWidth * RX,
            b = CLOSED.depth * RY,
            yc = (CLOSED.bottom - CLOSED.depth) * RY;
        ctx.rotate(-uprightAngle);
        ctx.beginPath();
        ctx.ellipse(0, yc, a, b - th / 2, 0, 0, Math.PI); // centerline: the outer edge sits at depth b
        ctx.lineWidth = th;
        ctx.lineCap = 'round';
        ctx.strokeStyle = this.look.sclera;
        ctx.stroke();
    }

    #drawParticles() {
        const ctx = this.ctx;
        for (const p of this.particles) {
            const k = p.age / p.life;
            ctx.save();
            ctx.translate(p.x, p.y);
            if (p.type === 'heart') {
                ctx.globalAlpha = Math.min(k * 6, 1, (1 - k) * 4); // quick fade in, fade out at the end
                ctx.fillStyle = '#FF2A3A';
                drawHeart(ctx, p.size);
            } else {
                const twinkle = Math.sin(k * Math.PI); // grow then shrink
                ctx.rotate(p.spin * p.age);
                ctx.fillStyle = '#FFFFFF';
                drawSparkle(ctx, p.size * twinkle);
            }
            ctx.restore();
        }
    }
}
