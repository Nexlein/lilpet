# lilpet

Personal, non-commercial keychain "digital pet" inspired by the Starboy (lilguy / CREATURE): a round screen with
expressive eyes that react to what a camera sees (gestures, faces, the person's expressions; no voice). The owner picks
the eyes (no rarity, no random draw); looks and, later, animations stay data, not code, for the PC app to edit and send.

Stage: the C eye engine runs on the host, in WebAssembly and on the ESP32-S3; PC app v1 picks the eyes and sends them
over USB. The final hardware (Waveshare ESP32-S3-Touch-AMOLED-1.32 + XIAO ESP32S3 Sense camera over UART,
`docs/materiel.md`) is not bought yet: until then the eyes run on an M5Stack AtomS3R (128 px LCD, no camera). The AI
model that will choose the reactions does not exist yet; what it will send is ready (`docs/protocole.md`).

## Read first

- `.claude/codemap.md`: every source file with its purpose, exports, what it uses and what uses it, then the commands
  (mise tasks, npm scripts) and the npm packages. Read it before searching, and open only the files it points to. It is
  generated (`npm run codemap`, run by the pre-commit hook) from the purpose comment each source file starts with.
- `docs/architecture.md` before structural work: the parts, the decisions (`docs/adr/`), the open points.
- `docs/yeux.md` before changing how the eyes look or move: the measured style of the originals.
- `docs/protocole.md` before changing a command, `docs/firmware.md` before firmware work, `docs/app.md` for the app's
  spec and roadmap, `docs/materiel.md` for the hardware, `docs/recherche.md` for what is known of the original.
  `README.md` has the setup and the commands for a person.

## Rules

- Code, comments, commits and this file: English. `docs/` and README: French.
- File names: C and Python in snake_case (`look_store.c`, `board_link.py`: Python imports need it, and a Python test is
  `test_*.py` by convention), but the other scripts that are only run in kebab-case (`check-device.py`), like JS and TS
  files (`gen-scenes-table.mjs`, `fake-port.ts`); React components in PascalCase (`SendPanel.tsx`). A JS test sits next
  to its module, `<module>.test.mjs` (Node) or `<module>.test.ts` (Vitest), or is named after what it checks when that
  is not a module (`scripts/docs.test.mjs`); the app's browser tests are in `app/tests`, the engine's in `engine/tests`,
  the firmware's pure parts in `firmware/tests` (run with the engine's).
- Branches: `dev` is the default and integration branch, `main` only takes merges of `dev`. Both change only through
  pull requests with every required CI check green, merged with a merge commit (GitHub rulesets: no direct push, no
  force push, no deletion). The rulesets name the required checks by job name: renaming a CI job, or adding, renaming
  or removing a board (also in ci.yml's board list, which a test checks), means updating both rulesets.
- C: `lp_` prefix, float literals with `f`, no `double`, no `malloc`, no globals; warnings are errors.
- Only `app/src/ui` and `main.tsx` may import React (a lint rule checks it).
- Tool versions are exact, in `mise.toml` and the Dockerfiles; Renovate and Dependabot propose each update as a pull
  request against `dev` (`docs/adr/0008-outils-figes.md`).
- Eye style comes from measurements of the originals: recreate the style, never copy their assets. `references/`
  (frame sheets of third-party videos, behind `docs/yeux.md`) stays local, ignored by git, never published.
- After changing `eyes/*.json`, `engine/tests/scenes.json`, `scripts/expression.mjs` or `sceneFields` in
  `engine/wasm/engine.mjs`: `npm run tables` (CI fails on a stale generated file).
- A visual change to the eyes goes with a scene in `engine/tests/scenes.json` that shows it. After changing how the eyes
  look or move: `mise run engine:wasm`, `npm run goldens`, then look at the images it rewrote before committing them.

## Pitfalls

- A new clone or worktree needs `mise trust` then `mise run setup`: without the `.husky/_` that `npm ci` creates, git
  silently skips every hook.
- Never install emsdk through mise: its clang and node shadow the system ones and send ccache into a loop. The
  WebAssembly build runs in the image of `engine/wasm/Dockerfile`, ESP-IDF in the one of `firmware/Dockerfile`.
- The commands that format or fix files (`npm run format`, `lint:fix`, `engine:format`) and the linters run on the
  files git tracks only, so the owner's untracked local notes are never touched: keep it that way.
- The pre-commit hook refuses a commit while a data file, a generated table or a script that reads them holds unstaged
  changes: its checks read the disk. Stage them too, or stash them.
- The reference renders come from the WebAssembly build: a host build may use FMA and differ by a few pixels. The host
  build uses the system clang and refuses one older than CI's (21).
- The engine is not thread-safe: the firmware queues the commands for its drawing loop.
- On the chip, loads must be aligned: GCC turns a copy it cannot prove aligned into a call to `memcpy`, which the host
  and WebAssembly builds hide. In a hot loop, read a float's bits through a union, and check a speed-up on the board.
- The Playwright tests serve on port 8468, so two runs at once collide; the app needs `mise run engine:wasm` first.
- `firmware:flash` writes the bootloader, the partition table and the app only, so the look kept in NVS survives;
  `merged-binary.bin`, for a blank board, erases it. `FORCE=1` flashes over another board's image: only when the owner
  asks.

## Done

`mise run check` passes: what CI runs, as parallel jobs. Firmware changes: `mise run firmware:build-all` too.
`firmware:check-device` on a plugged board is the one check CI cannot run.
