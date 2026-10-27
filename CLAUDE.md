# lilpet

Personal, non-commercial keychain "digital pet" inspired by the Starboy (lilguy / CREATURE): a round screen with
expressive eyes that react through a camera. Stage: browser prototype of the eyes. Hardware not bought yet.
No rarity or random eye draw: the owner picks. Long-term, a small PC app edits the eyes and animations and sends
them to the device, so keep looks (and later animations) as data, not code.

## Architecture

Decided in `notes/architecture.md` (read it before structural work): one eye engine in C17 (`engine/`, next step),
compiled for the ESP32-S3 with ESP-IDF and to WebAssembly with Emscripten; custom scanline renderer, RGB565, dirty
regions, `float` never `double`; PC app in TypeScript + Vite with Web Serial (`app/`). The JS prototype is the visual
reference until the engine takes over each feature.

## Layout

- `prototype/` — design reference, ES modules, canvas 466×466 (the target round AMOLED):
    - `eyes.js` — renderer class `Eyes`: state, animation, drawing, seeded PRNG (mulberry32). No DOM access, so Node
      can import it (`scripts/validate-data.mjs` does).
    - `ui.js` + `index.html` — interactive page. `window.eyes` exposes the renderer for console debugging.
    - `scenes.json` + `reference.html`/`reference.js` — frozen reference scenes (look, emotion, blink phase, head,
      gaze, seeded particles) rendered with `Eyes.renderScene`. The C engine must reproduce them (parity rule).
- `eyes/looks.json` — eye looks (sclera color, pupil type `dot | slit | acorn | round`, left/right pupil colors,
  glint).
- `scripts/validate-data.mjs` — validates `looks.json` and `scenes.json` (ids, colors, known emotions, ranges).
- `notes/` — research, hardware, architecture and eye analysis, in French. `notes/yeux.md` holds the measured style
  of the originals (proportions, blink timing, emotions): read it before changing how the eyes look or move.
- `references/` — frame sheets extracted from videos (not formatted, not code).

## Commands

- `npm run serve` — serve the repo on http://127.0.0.1:8466/prototype/ (modules and data fetches need HTTP).
- `npm run check` — Prettier check, oxlint, data validation. Same as CI (`.github/workflows/ci.yml`).
- `npm run format` / `npm run lint:fix` — auto-fix.

## Conventions

- Code, comments, commits: English. Notes and README: French.
- No build step and no runtime dependency in the prototype: plain HTML, CSS and JS.
- Tool versions live in `mise.toml` (Node follows the LTS), used locally and by CI through `jdx/mise-action`.
- Eye style comes from measurements of the originals (see `notes/yeux.md`); recreate the style, never copy their
  assets.
- A visual change to the prototype that the engine must follow goes with a scene in `prototype/scenes.json`.
- Done means: `npm run check` passes, and `prototype/` and `prototype/reference.html` load with no console error.
