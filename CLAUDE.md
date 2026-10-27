# lilpet

Personal, non-commercial keychain "digital pet" inspired by the Starboy (lilguy / CREATURE): a round screen with
expressive eyes that react through a camera. Stage: browser prototype of the eyes. Hardware not bought yet.

## Layout

- `prototype/` — eye renderer: `index.html` (UI, styles) + `main.js` (classic script, globals on purpose so the page
  can be driven from the browser console). Canvas is 466×466, the resolution of the target round AMOLED.
- `eyes/looks.json` — eye looks (sclera color, pupil type `dot | slit | acorn | round`, left/right pupil colors,
  glint). Validated by `scripts/validate-looks.mjs`.
- `notes/` — research, hardware and eye analysis, in French. `notes/yeux.md` holds the measured style of the
  originals (proportions, blink timing, emotions): read it before changing how the eyes look or move.
- `references/` — frame sheets extracted from videos (not formatted, not code).

## Commands

- `npm run serve` — serve the repo on http://127.0.0.1:8466/prototype/ (fetching `looks.json` needs HTTP).
- `npm run check` — Prettier check, oxlint, looks.json validation. Same as CI (`.github/workflows/ci.yml`).
- `npm run format` / `npm run lint:fix` — auto-fix.

## Conventions

- Code, comments, commits: English. Notes and README: French.
- No build step and no runtime dependency in the prototype: plain HTML, CSS and JS.
- Eye style comes from measurements of the originals (see `notes/yeux.md`); recreate the style, never copy their
  assets.
- Done means: `npm run check` passes and the prototype loads with no console error.
