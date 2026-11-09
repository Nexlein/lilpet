// The fields of an emotion's expression (lp_expression in engine/include/lilpet/lilpet.h), in the order of the struct:
// the value an emotion of eyes/emotions.json takes when it leaves a field out, and the range the engine accepts. One
// table for the C generator (scripts/gen-emotions-table.mjs) and the data validator (scripts/validate-data.mjs).
export const EXPRESSION = {
    tl: { neutral: 0, range: [0, 1] }, // top lid, down from the top of the eye
    ts: { neutral: 0, range: [-1, 1] }, // slope of the top lid
    bl: { neutral: 0, range: [0, 1] },
    sq: { neutral: 0, range: [0, 0.95] }, // 1 squashes the eye flat: lp_affine_inverse would divide by zero
    cr: { neutral: 0, range: [0, 1] },
    rot: { neutral: 0, range: [-1.2, 1.5] }, // lower turns the eye past a quarter turn with its tilt: the lid comes up
    hl: { neutral: 0, range: [0, 1] }, // above 0.5, the glint shows
    ps: { neutral: 1, range: [0.2, 2] }, // pupil size
    es: { neutral: 1, range: [0.5, 1.5] },
    gy: { neutral: 0, range: [-1, 1] }, // added to the vertical gaze
    spin: { neutral: 0, range: [0, 9] } // dizziness: the gaze circles, the head wobbles
};

export const EXPRESSION_FIELDS = Object.keys(EXPRESSION);

/** An emotion of eyes/emotions.json with every expression field: the ones it leaves out at their neutral value. */
export const withNeutral = (emotion) => ({
    ...Object.fromEntries(EXPRESSION_FIELDS.map((k) => [k, EXPRESSION[k].neutral])),
    ...emotion
});
