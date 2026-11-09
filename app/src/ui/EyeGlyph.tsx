// A small drawing of a pair of eyes, for the gallery and the pupil picker. Same proportions as the engine: eyes tilted
// outward, pupils upright, set toward the nose and leaning a little (docs/yeux.md).
import { useId } from 'react';
import { EYE_SHAPE, PUPIL_LEAN, PUPIL_SHAPES, type LookColors, type PupilShape, type PupilType } from '../data/looks';

// The engine's shape (eyes/looks.json), scaled to the 100 x 60 drawing
const RY = 20;
const RX = (RY * EYE_SHAPE.rx) / EYE_SHAPE.ry;
const GAP = (RX * EYE_SHAPE.gap) / EYE_SHAPE.rx; // from the middle to each eye's center
const degrees = (rad: number) => (rad * 180) / Math.PI;
const TILT = degrees(EYE_SHAPE.tilt); // top of each eye outward
const LEAN = degrees(PUPIL_LEAN); // top of each pupil toward the nose

// A pupil's radii, in fractions of RX and RY: a disc's radius is a fraction of RX for both
function radii(p: PupilShape): { rx: number; ry: number } {
    return 'r' in p ? { rx: p.r, ry: (p.r * RX) / RY } : { rx: p.rx, ry: p.ry };
}

interface Props {
    colors: LookColors;
    pupil: PupilType;
}

export function EyeGlyph({ colors, pupil }: Props) {
    const id = useId();
    const p = PUPIL_SHAPES[pupil];
    const r = radii(p);
    return (
        <svg viewBox="0 0 100 60" aria-hidden="true">
            {([-1, 1] as const).map((side) => {
                const cx = 50 + side * GAP;
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
                            rx={r.rx * RX}
                            ry={r.ry * RY}
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
