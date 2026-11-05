// Draws the eyes of the design prototype on a canvas, from the state of an Eyes object (eyes.js): what render.c does
// in the C engine. Units are logical pixels of the 466×466 round screen.
import { SIZE, clamp } from './eyes.js';

// Official pupil names (notes/yeux.md), in the order of the C engine's lp_pupil
export const PUPIL_TYPES = ['dot', 'cat', 'acorn', 'circle'];

// Resting pupil position, as a fraction of the eye radius: x toward the inner corner, y down.
// Measured on the official catalogue (notes/yeux.md): pupils converge, a slightly cross-eyed look.
const PUPIL_REST = { dot: [0.57, 0.12], cat: [0.35, 0.11], circle: [0.36, 0], acorn: [0.18, 0.25] };
// Pupils lean a little, top toward the nose (rad), against the eye tilt
const PUPIL_LEAN = 0.09;

// Closed eye, measured on 7 frames of the official footage: the lower half of a flat, level ellipse (flat
// bottom, tips turning up), drawn as a stroke of even thickness with round tips. Fractions of the eye
// radii: half width of RX, lowest point and depth of RY (outer edge); thickness in pixels.
const CLOSED = { halfWidth: 0.76, bottom: 0.7, depth: 0.245, thickness: 4.5 };

// Sizes the canvas backing store to the device pixel ratio and returns a context drawn in screen units.
export function setupCanvas(canvas, dpr = globalThis.devicePixelRatio || 1) {
    canvas.width = SIZE * dpr;
    canvas.height = SIZE * dpr;
    const ctx = canvas.getContext('2d');
    ctx.scale(dpr, dpr);
    return ctx;
}

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

function drawPupil(ctx, eyes, color, RX, RY, inner, uprightAngle) {
    const { gaze, look, cur } = eyes;
    const ps = cur.ps;
    const type = look.pupil;
    let prx, pry;
    switch (type) {
        case 'dot':
            prx = RX * 0.105 * ps;
            pry = RY * 0.125 * ps;
            break;
        case 'cat':
            prx = RX * 0.21 * ps;
            pry = RY * 0.72;
            break;
        case 'acorn':
            // A tall ellipse set low: the bottom of the eye cuts it into its acorn shape
            prx = RX * 0.5 * ps;
            pry = RY * 0.98 * Math.min(ps, 1.05);
            break;
        case 'circle':
            prx = pry = RX * 0.52 * ps;
            break;
    }
    // Pupil travel is limited by how much room is left inside the eye. Big acorn pupils may
    // run past the edge and get clipped, small ones stay fully visible.
    const [rx0, ry0] = PUPIL_REST[type];
    const limX = type === 'acorn' ? RX : RX - prx - 3,
        limY = type === 'acorn' ? RY : RY - pry - 3;
    const x = clamp(inner * rx0 * RX + gaze.x * Math.max(RX - prx, RX * 0.15) * 0.85, limX);
    const y = clamp(ry0 * RY + gaze.y * Math.max(RY - pry, RY * 0.12) * 0.85, limY);
    // The eye is tilted but pupils stay nearly upright on the original: cancel the eye rotation, then lean
    ctx.rotate(-uprightAngle);
    ctx.translate(x, y);
    ctx.rotate(inner * PUPIL_LEAN);
    ctx.fillStyle = color;
    ctx.beginPath();
    ctx.ellipse(0, 0, prx, pry, 0, 0, Math.PI * 2);
    ctx.fill();
    if (cur.hl > 0.5) {
        const r = Math.max(Math.min(prx, pry) * 0.28, 4);
        ctx.fillStyle = '#fff';
        ctx.beginPath();
        ctx.arc(-prx * 0.35, -pry * 0.38, r, 0, Math.PI * 2);
        ctx.fill();
    }
}

// Closed eye: lower half of a flat ellipse, level with the face, stroked with round tips
function drawClosedEye(ctx, color, RX, RY, uprightAngle) {
    const th = CLOSED.thickness,
        a = CLOSED.halfWidth * RX,
        b = CLOSED.depth * RY,
        yc = (CLOSED.bottom - CLOSED.depth) * RY;
    ctx.rotate(-uprightAngle);
    ctx.beginPath();
    ctx.ellipse(0, yc, a, b - th / 2, 0, 0, Math.PI); // centerline: the outer edge sits at depth b
    ctx.lineWidth = th;
    ctx.lineCap = 'round';
    ctx.strokeStyle = color;
    ctx.stroke();
}

function drawEye(ctx, eyes, side, bp, squash) {
    // side: -1 = left eye, +1 = right eye. "Inner" corner is toward the screen center.
    const { cur, look } = eyes;
    const { rx: RX, ry: RY, tilt } = eyes.shape;
    const inner = -side;
    const pose = eyes.eyePose(side);
    ctx.save();
    ctx.translate(pose.x, pose.y);
    ctx.rotate(eyes.head.roll);
    ctx.scale(pose.persp * pose.fx, pose.persp * pose.fy);
    ctx.rotate(inner * (cur.rot - tilt));
    ctx.scale(cur.es * squash.x * bp.sx * (1 + cur.sq * 0.1), cur.es * squash.y * bp.sy * (1 - cur.sq));

    if (bp.closed) {
        drawClosedEye(ctx, look.sclera, RX, RY, inner * (cur.rot - tilt));
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
    drawPupil(ctx, eyes, look.pupilColor[side < 0 ? 0 : 1], RX, RY, inner, inner * (cur.rot - tilt));
    ctx.restore();
}

function drawParticles(ctx, particles) {
    for (const p of particles) {
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

// Draws the state of `eyes` at time t (seconds, for the breathing) on a context made by setupCanvas
export function drawEyes(ctx, eyes, t) {
    ctx.fillStyle = '#000';
    ctx.fillRect(0, 0, SIZE, SIZE);
    if (!eyes.look) return;

    const bp = eyes.blinkPose();
    // Squash & stretch along the gaze movement, plus a slow "breathing"
    const { vx, vy } = eyes.gaze;
    const speed = Math.hypot(vx, vy);
    const st = Math.min(speed * 0.025, 0.12);
    const ax = speed ? Math.abs(vx) / speed : 0,
        ay = speed ? Math.abs(vy) / speed : 0;
    const breath = 1 + 0.012 * Math.sin(t * 2.1);
    const squash = { x: 1 + st * ax - st * ay * 0.5, y: (1 + st * ay - st * ax * 0.5) * breath };

    drawEye(ctx, eyes, -1, bp, squash);
    drawEye(ctx, eyes, 1, bp, squash);
    drawParticles(ctx, eyes.particles);
}
