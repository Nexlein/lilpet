# lilpet

Personal, non-commercial keychain "digital pet" inspired by the Starboy (lilguy / CREATURE): a round screen with
expressive eyes that react to what a camera sees: gestures, faces and the person's expressions (no voice). Stage: eye
engine in C done (host, WebAssembly, ESP32-S3); PC app v1 (pick the eyes, send them over USB); hardware chosen in
`notes/materiel.md` (Waveshare ESP32-S3-Touch-AMOLED-1.32 + XIAO ESP32S3 Sense camera over UART), not bought yet; until
then the eyes run on an M5Stack AtomS3R (128 px LCD, no camera). The goal is an AI model inside the object that processes what
the camera sees and chooses the reactions; it does not exist yet, what it will send is ready (`> react`, `> emotion`,
`> face` in `notes/app.md`).
No rarity or random eye draw: the owner picks. Long-term, a small PC app edits the eyes and animations and sends
them to the device, so keep looks (and later animations) as data, not code.

## Architecture

Decided in `notes/architecture.md` (read it before structural work): one eye engine in C17 (`engine/`),
compiled for the ESP32-S3 with ESP-IDF and to WebAssembly with Emscripten; custom scanline renderer, RGB565, dirty
regions, `float` never `double`; PC app in TypeScript + Vite with Web Serial (`app/`). The JS prototype is the visual
reference until the engine takes over each feature.

## Layout

- `prototype/` — design reference, ES modules, canvas 466×466 (the target round AMOLED):
    - `eyes.js` — class `Eyes`: state, animation, its own clock (`update(dt)`), seeded PRNG (mulberry32). `setScene`
      starts over, as `lp_eyes_set_scene` does. No DOM access, so Node can import it
      (`scripts/validate-data.mjs` does). `draw.js` — drawing, as `render.c` in the engine (`drawEyes`, `setupCanvas`,
      the pupil table).
    - `ui.js` + `index.html` — interactive page. The "Rendu" switch shows the prototype, the C engine (WebAssembly)
      or both side by side, driven by the same inputs; side by side they advance in fixed 1/64 s steps to stay in sync.
      `window.eyes` exposes the prototype for console debugging, `window.live` the hooks of `tests/live.spec.mjs`
      (`resync(seed)`, `pickLook`, `react`), which fails CI when the two drift apart while animating, from fixed seeds.
    - `scenes.json` + `reference.html`/`reference.js` — frozen reference scenes (look, emotion, blink phase, head,
      gaze, seeded particles), rendered side by side by the prototype and the C engine (WebAssembly), with a score
      per scene. Tolerances, `compareFrame`, `fetchJson` and `ENGINE_URL` in `parity.js`, shared styles in `style.css`.
      `tests/parity.spec.mjs` (Playwright) fails CI when a scene is out of tolerance.
- `engine/` — C17 eye engine. State and animation: `src/eyes.c` (emotions, gaze, head, blink, particles, reactions:
  `lp_eyes_react` with the event table: the emotion changes at once, and sparkles burst at most every 0.5 s whatever the
  emotion, so a chatty model cannot flood the screen; upside down from gravity; a temporary emotion goes back to the mood, the last
  one set for good), `src/emotions.c` over the tables of `src/emotions_table.h` (generated from `eyes/emotions.json`),
  `src/motion.c` (`lp_shake_update`: a shake told from raw accelerometer samples), `src/senses.c` (`lp_senses`: what the
  hardware reports becomes eyes state, same on every board: smoothing, button debounce, a sensor or a face that stopped
  reporting, a new face firing `face_seen`), `src/pose.c`
  (blink pose, eye pose on the 3D head), `src/rng.c` (mulberry32). Rendering: `src/render.c` builds each eye's geometry
  (same transform chain as the prototype) and composes the frame row by row into RGB565, redrawing only the dirty
  region, whole (`lp_render`) or in bands (`lp_render_begin` + `lp_render_rows`: it never reads the previous frame back,
  so the device needs no whole frame in memory), at 466 px or any smaller square size (`lp_renderer_set_size`: same
  layout, scaled at output), high byte first for the display controllers (`lp_renderer_set_big_endian`);
  `src/raster.c` turns ellipses, lids, cuts and polygons into sub-row spans and exact horizontal coverage (`LP_SUBROWS`
  vertical samples). Input that is not finite is dropped, the time step is capped at 1 s, `lp_eyes_set_scene` starts
  over. `tools/bench.c` + `mise run engine:bench` time the renderer per scene and over an
  animation (`tools/bench_script.h`, shared with the firmware benchmark; host-release build; CI puts the tables in the
  job summary, no time limit). Public API in `include/lilpet/lilpet.h`; the caller owns `lp_eyes` and `lp_renderer`, whose fields sit in
  `render_state.h` (internal; `screen.h` holds what both headers need). `sources.cmake` lists
  sources and warning flags shared by the host build, the WebAssembly module (`wasm/smoke.mjs` checks it loads) and the
  ESP-IDF component. Tests use the tiny runner in `tests/test.h` (list new tests in `LP_TESTS`). Presets: `host-debug`
  (ASan + UBSan), `host-release`, `wasm`.
- `engine/tests/js_vectors.h` and `engine/src/emotions_table.h` — generated by `npm run vectors`: the first by
  `scripts/gen-engine-vectors.mjs` from the prototype (RNG values, poses, seeded simulations with the whole expression,
  blink and particle sums, scene states), the second by `scripts/gen-emotions-table.mjs` from `eyes/emotions.json`;
  helpers in `scripts/gen-common.mjs`. `tests/test_parity.c` checks the engine against them. Random numbers are drawn
  in the same order in JS and C: keep it that way when changing either side.
- `engine/wasm/` — WebAssembly bindings (`bindings.c`, `lpw_*` functions) and `engine.mjs`, the JS wrapper that
  loads `engine/build/wasm/lilpet.mjs` and takes the same scene and look objects as the prototype; unknown names are
  ignored; `renderView` gives the pixels without a copy, `PUPILS` the pupil list; `engine.d.mts` types it for the app, with `EyeEvent` read from `eyes/emotions.json`. `src/look.c` reads and writes a look as text (`lp_look_parse`, `lp_look_format`), the USB format.
- `app/` — PC app v1 (spec and protocol in `notes/app.md`), an npm workspace: React 19 + TypeScript strict + Vite,
  font Bricolage Grotesque. `src/data` (swatches, looks, favorites in localStorage), `src/device` (Web Serial
  connection and text protocol of `notes/app.md`: `hello`, `look`, `emotion`, `react`; replies start with `< `;
  `protocol.ts` reads a reply once into a typed result (`parseReply`); `session.ts` is the connect / send / unplug state
  machine, tested with the fake board of `fake-port.ts`, which stands alone: `tests/app.spec.mjs` sends its text to the
  page),
  `src/engine` (live preview with the WebAssembly engine), `src/ui` (React; only this part and `main.tsx` may import it,
  a lint rule checks it; `SendPanel` owns the state of a send). Picking a mood or petting also reaches the board, and
  the last mood goes again after `hello`. The
  engine build is served and bundled from `engine/build/wasm` by a plugin in `vite.config.mjs`. Vitest tests next to
  the code (`*.test.ts`); `tests/app.spec.mjs` opens the built app in Playwright, with a fake `navigator.serial`.
- `firmware/` — ESP-IDF app for the ESP32-S3, one image per board (`atoms3r`, `waveshare_132`, `stamp_s3a`), chosen at
  build time. A board is one folder, nothing else to touch: `boards/<board>/board.cmake` lists its parts and
  `board_config.h` its pins, mounting, display size, chip package and motion sensor, an optional `sdkconfig.defaults`
  its own ESP-IDF options; CMake, CI and the scripts read the folders. The
  chip package cannot tell boards apart (the AtomS3R and the Waveshare 1.32" are both ESP32-S3-PICO), but `board_init`
  stops on the wrong package before touching a pin, and `firmware:flash` refuses a board that answers `hello` with
  another name (`scripts/flash-guard.py`, which also gives esptool the port it asked). An image flashed on another board drives that board's pins.
  `components/lilpet` wraps the engine. Each header is an interface and each part a `.c` file, with no function tables:
  `display.h` is `display_co5300.c` for the Waveshare 1.32" AMOLED (CO5300 over QSPI with ESP-IDF's `esp_lcd`; values
  from Waveshare's demo, not yet run on the board, also used without a display) or `display_atoms3r.c` for the AtomS3R
  LCD (128 px SPI, ST7735S or GC9107 told apart by their ID; values from M5GFX), `display_paced.c` timing the frames of
  those with no TE signal; `imu.h` is `imu_bmi270.c` (BMI270, which needs Bosch's configuration file,
  `bmi270_config.c`, BSD-3, copied unmodified) or `imu_none.c` (the Waveshare 1.32" has no motion sensor). `board.c`
  holds the board itself: power hold, button, I2C bus, backlight.
  `main/main.c` shows the eyes live, idling on their own (saccades, blinks, the head following the gaze): each frame
  waits for `display_wait_frame`, applies the queued commands, gives the sensors to `lp_senses_update`, draws band by
  band into two DMA buffers in internal RAM while the previous band is sent, and prints timings over USB after each
  round of 780 frames. A board with no display (`BOARD_BENCH`, a bare M5Stamp S3A) plays the benchmark animation
  (`engine/tools/bench_script.h`) at 62.5 frames/s instead, with the host's look, seed and fixed step, so that image
  measures the engine on the chip, comparable with `mise run engine:bench`.
  `main/commands.c` reads the commands on the same USB port and queues them for the drawing loop (the engine is not
  thread-safe); `src/command.c` in the engine parses them (`lp_command_parse`, tested on the host). The camera board
  will send the same lines over UART. `main/look_store.c` keeps the look in NVS.
  Gravity (`lp_eyes_set_gravity`) turns the face to stay level and slides the eyes toward the low side (prototype:
  `gravity` in `eyes.js`, "Objet tourné" slider). Reactions (`eyes/emotions.json`): held upside down it gets angry,
  shaken dizzy while it lasts (the gaze circles, the head wobbles), pressed (the AtomS3R's screen is a button) or petted
  in the app happy, and the gestures and expressions a camera sends each have their own.
- `eyes/emotions.json` — emotions (French label, expression, particles) and reactions, by event name: the one source
  for the prototype, the C tables and the app's labels.
- `eyes/looks.json` — the 100 official swatches (sclera color, left/right pupil colors; catalogue in `notes/yeux.md`).
  A look is a swatch worn with a pupil type `dot | cat | acorn | circle` (`makeLook` in `eyes.js`); scenes name both.
  Looks carry no glint: only emotions add one.
- `scripts/validate-data.mjs` — validates `looks.json`, `emotions.json` and `scenes.json` (ids, colors, exactly 100
  swatches, known pupil types and emotions, `neutral`, `shake` and `upside_down` present, ranges, no comment marks in
  a `note`).
- `notes/` — research, hardware, architecture and eye analysis, in French. `notes/yeux.md` holds the measured style
  of the originals (proportions, blink timing, emotions): read it before changing how the eyes look or move.
- `references/` — frame sheets extracted from videos (not formatted, not code).

## Commands

- `npm run serve` — serve the repo on http://127.0.0.1:8466/prototype/ (modules and data fetches need HTTP).
- `npm run check` — Prettier check, oxlint, data validation, generated files up to date, app types and Vitest tests.
  Same as CI (`.github/workflows/ci.yml`).
- `npm run app:dev` — app on http://localhost:8467/ (needs `mise run engine:wasm`); `app:build` into `app/dist`,
  `app:test` for Vitest.
- `npm run format` / `npm run lint:fix` — auto-fix.
- `npm run test:parity` — Playwright tests: reference scenes, live side by side, data and state checks, and the built
  app, served on their own port 8468, which must be free: two runs at once collide (needs `mise run engine:wasm`, and `npm run app:build` for the app test, skipped locally otherwise, failed
  in CI; first time: `npx playwright install --only-shell chromium`).
- `mise run engine:check` — C side, same as CI: clang-format check, tests with sanitizers, clang-tidy, WebAssembly
  build. Single steps: `engine:format-check`, `engine:test`, `engine:tidy`, `engine:wasm`; `engine:format` fixes the
  formatting.
- `mise run engine:bench` — renderer timings (compare runs on the same machine only); see `notes/architecture.md`.
- The WebAssembly build runs in the Emscripten image pinned in `engine/wasm/Dockerfile` (latest release; Dependabot
  proposes new ones). Do not install emsdk through mise: its clang and node shadow the system ones and send ccache into
  a loop.
- `BOARD=<board> mise run firmware:deploy` — takes care of one object: builds, flashes, checks. Alone:
  `firmware:build` into `firmware/build/<board>/` (`flash_args` and its three images, plus `merged-binary.bin` for a
  blank board, which also erases the stored look), in the ESP-IDF image pinned in `firmware/Dockerfile` (latest release;
  Dependabot proposes new ones); `firmware:flash` writes the bootloader, partition table and app only, so the look kept
  in NVS survives; `firmware:check-device` (`scripts/device-check.py`, through `uv`: no reboot on opening, hello and its
  board, a look kept across a reboot, emotion, react, face, bad commands and lines too long rejected, about 1 g from a
  motion sensor; puts the board's look back, the one check CI cannot run). `PORT` picks the board, otherwise the only
  one plugged in; `FORCE=1` flashes over another board's image. `firmware:monitor` reads the port. esptool and uv come
  from mise, pyserial from `scripts/requirements.txt` through uv: no local ESP-IDF install. CI builds every folder of `firmware/boards` and keeps one artifact each.
  The build keeps its ccache in `firmware/.ccache` (CI caches it too).
- A new git worktree needs `mise trust` and `npm ci`: Git silently skips every hook without `.husky/_`, which only
  `npm ci` creates.

## Conventions

- Code, comments, commits: English. Notes and README: French.
- C: `lp_` prefix, float literals with `f`, no `double`, no `malloc`, no globals; warnings are errors.
- No build step and no runtime dependency in the prototype: plain HTML, CSS and JS. The app has its build (Vite); only
  `app/src/ui` and `main.tsx` may import React.
- Tool versions live in `mise.toml`, exact (Node on the LTS line), used locally and by CI through `jdx/mise-action`
  (the Python library of the scripts: `scripts/requirements.txt`);
  Renovate proposes each new release as a pull request (`.github/renovate.json5`), Dependabot the rest
  (`.github/dependabot.yml`: npm, Docker images, Actions).
- Eye style comes from measurements of the originals (see `notes/yeux.md`); recreate the style, never copy their
  assets.
- A visual change to the prototype that the engine must follow goes with a scene in `prototype/scenes.json`.
- After changing `prototype/eyes.js`, `scenes.json`, `looks.json` or `emotions.json`: `npm run vectors` (CI fails on a stale file), then
  port the change to the engine until `mise run engine:test` passes.
- Done means: `npm run check`, `mise run engine:check` and `npm run test:parity` pass, and `prototype/` loads with no
  console error. Firmware changes: `BOARD=<board> mise run firmware:build` for each board too.
