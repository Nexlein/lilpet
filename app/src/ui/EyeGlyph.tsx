// A small drawing of a pair of eyes, for the gallery and the pupil picker. Same proportions as the engine: eyes tilted
// outward, pupils upright, set toward the nose and leaning a little (notes/yeux.md).
import { useId } from 'react';
import type { LookColors, PupilType } from '../data/looks';

const RX = 17;
const RY = 20;
const TILT = 21; // degrees, top of each eye outward
const LEAN = 5; // degrees, top of each pupil toward the nose

// Pupil size and resting position, in fractions of RX and RY (same values as PUPIL_REST in the engine)
const PUPILS: Record<PupilType, { rx: number; ry: number; x: number; y: number }> = {
    dot: { rx: 0.105, ry: 0.125, x: 0.57, y: 0.12 },
    cat: { rx: 0.21, ry: 0.72, x: 0.35, y: 0.11 },
    acorn: { rx: 0.5, ry: 0.98, x: 0.18, y: 0.25 },
    circle: { rx: 0.52, ry: (0.52 * RX) / RY, x: 0.36, y: 0 } // a disc: both radii are 0.52 RX
};

interface Props {
    colors: LookColors;
    pupil: PupilType;
}

export function EyeGlyph({ colors, pupil }: Props) {
    const id = useId();
    const p = PUPILS[pupil];
    return (
        <svg viewBox="0 0 100 60" aria-hidden="true">
            {([-1, 1] as const).map((side) => {
                const cx = 50 + side * 19;
                const inner = -side;
                const clip = `${id}-${side}`;
                const px = cx + inner * p.x * RX;
                const py = 30 + p.y * RY;
                return (
                    <g key={side}>
                        <clipPath id={clip}>
                            <ellipse cx={cx} cy={30} rx={RX} ry={RY} transform={`rotate(${side * TILT} ${cx} 30)`} />
                        </clipPath>
                        <ellipse
                            cx={cx}
                            cy={30}
                            rx={RX}
                            ry={RY}
                            fill={colors.sclera}
                            transform={`rotate(${side * TILT} ${cx} 30)`}
                        />
                        <ellipse
                            cx={px}
                            cy={py}
                            rx={p.rx * RX}
                            ry={p.ry * RY}
                            fill={colors.pupilColor[side < 0 ? 0 : 1]}
                            transform={`rotate(${inner * LEAN} ${px} ${py})`}
                            clipPath={`url(#${clip})`}
                        />
                    </g>
                );
            })}
        </svg>
    );
}
