// Live preview: the C engine (WebAssembly) animated in a canvas, in fixed 1/64 s steps (exact in float).
import { loadEngine, type Engine, type Look, type Region, type Scene } from '../../../engine/wasm/engine.mjs';

export type { Engine };

const STEP = 1 / 64;
const MAX_STEPS = 8; // per frame, so a slow frame does not snowball

let loading: Promise<Engine> | undefined;

// The WebAssembly build is served next to the app, under engine/ (see vite.config.mjs). Loaded once: StrictMode asks twice
export function loadEyeEngine(): Promise<Engine> {
    loading ??= loadEngine(new URL(`${import.meta.env.BASE_URL}engine/lilpet.mjs`, location.href).href);
    return loading;
}

export class Preview {
    readonly #engine: Engine;
    readonly #ctx: CanvasRenderingContext2D;
    #frame = 0;
    #last = 0;
    #pending = 0;
    #whole = true; // a new canvas does not hold the engine's last frame

    constructor(engine: Engine, canvas: HTMLCanvasElement) {
        const ctx = canvas.getContext('2d');
        if (!ctx) throw new Error('2D canvas unavailable');
        canvas.width = canvas.height = engine.size;
        // Its context was lost and it came back blank: the whole frame again, at once, moving or still
        canvas.addEventListener('contextrestored', () => this.#draw(this.#engine.renderView()));
        this.#engine = engine;
        this.#ctx = ctx;
    }

    start(): void {
        this.#last = performance.now();
        const tick = (now: number) => {
            this.#pending = Math.min(this.#pending + (now - this.#last) / 1000, MAX_STEPS * STEP);
            this.#last = now;
            let stepped = false;
            while (this.#pending >= STEP) {
                this.#pending -= STEP;
                this.#engine.update(STEP);
                stepped = true;
            }
            // A fast screen asks for more frames than the animation has: only draw the new ones
            if (stepped) this.#draw(this.#engine.renderView(), this.#engine.dirty());
            this.#frame = requestAnimationFrame(tick);
        };
        this.#frame = requestAnimationFrame(tick);
    }

    stop(): void {
        cancelAnimationFrame(this.#frame);
    }

    // One frozen frame instead of the animation
    still(scene: Scene, look: Look): void {
        this.#draw(this.#engine.renderScene(scene, look));
    }

    // Puts a frame on the canvas, only the region that changed when the canvas holds the frame before it
    #draw(pixels: Uint8ClampedArray<ArrayBuffer>, changed?: Region): void {
        const { size } = this.#engine;
        const { x, y, width, height } = this.#whole || !changed ? { x: 0, y: 0, width: size, height: size } : changed;
        this.#ctx.putImageData(new ImageData(pixels, size, size), 0, 0, x, y, width, height);
        this.#whole = false;
    }
}

// Pointer position over an element, as the engine wants it: -1..1 from the center, kept inside the circle
export function pointerIn(element: Element, clientX: number, clientY: number): { x: number; y: number } {
    const r = element.getBoundingClientRect();
    let x = (clientX - r.left - r.width / 2) / (r.width / 2);
    let y = (clientY - r.top - r.height / 2) / (r.height / 2);
    const len = Math.hypot(x, y);
    if (len > 1) {
        x /= len;
        y /= len;
    }
    return { x, y };
}
