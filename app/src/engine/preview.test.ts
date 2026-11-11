import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { loadEngine, type Engine } from '../../../engine/wasm/engine.mjs';
import { Preview, loadEyeEngine, pointerIn } from './preview';

vi.mock('../../../engine/wasm/engine.mjs', () => ({ loadEngine: vi.fn(async () => ({})) }));

const SIZE = 2;

// The parts of the browser and the engine a preview touches, with a clock driven by hand
function stage() {
    const pixels = new Uint8ClampedArray(SIZE * SIZE * 4);
    const engine = {
        size: SIZE,
        update: vi.fn(),
        renderView: vi.fn(() => pixels),
        renderScene: vi.fn(() => pixels),
        dirty: vi.fn(() => ({ x: 1, y: 0, width: 1, height: 2 })) // the right column
    };
    const drawn = vi.fn();
    const listeners = new Map<string, () => void>();
    const canvas = {
        getContext: () => ({ putImageData: drawn }),
        addEventListener: (type: string, listener: () => void) => listeners.set(type, listener)
    } as unknown as HTMLCanvasElement;
    const preview = new Preview(engine as unknown as Engine, canvas);
    let frame: FrameRequestCallback | null = null;
    vi.stubGlobal('requestAnimationFrame', (callback: FrameRequestCallback) => {
        frame = callback;
        return 1;
    });
    vi.stubGlobal('cancelAnimationFrame', () => (frame = null));
    const tick = (now: number) => frame?.(now);
    const clear = () => listeners.get('contextrestored')?.(); // the browser gives back a blank canvas
    return { engine, drawn, preview, tick, clear };
}

describe('preview', () => {
    beforeEach(() => {
        vi.stubGlobal('performance', { now: () => 0 });
        vi.stubGlobal(
            'ImageData',
            class {
                constructor(readonly data: Uint8ClampedArray) {}
            }
        );
    });
    afterEach(() => vi.unstubAllGlobals());

    it('advances in fixed steps and draws only new frames', () => {
        const { engine, drawn, preview, tick } = stage();
        preview.start();
        tick(1000 / 64);
        expect(engine.update).toHaveBeenCalledExactlyOnceWith(1 / 64);
        expect(drawn).toHaveBeenCalledOnce();
        tick(1000 / 64 + 1); // a fast screen: not a whole step later
        expect(engine.update).toHaveBeenCalledOnce();
        expect(drawn).toHaveBeenCalledOnce();
    });

    it('puts the whole first frame, then only the region the engine changed', () => {
        const { drawn, preview, tick } = stage();
        preview.start();
        tick(1000 / 64);
        tick(2000 / 64);
        expect(drawn.mock.calls.map((call) => call.slice(1))).toEqual([
            [0, 0, 0, 0, SIZE, SIZE],
            [0, 0, 1, 0, 1, 2]
        ]);
    });

    it('puts the whole frame again, at once, on a canvas the browser cleared', () => {
        const { drawn, preview, tick, clear } = stage();
        preview.start();
        tick(1000 / 64);
        clear();
        expect(drawn).toHaveBeenCalledTimes(2); // a still frame does not wait for the next step
        tick(2000 / 64);
        expect(drawn.mock.calls.map((call) => call.slice(1))).toEqual([
            [0, 0, 0, 0, SIZE, SIZE],
            [0, 0, 0, 0, SIZE, SIZE],
            [0, 0, 1, 0, 1, 2]
        ]);
    });

    it('puts a still frame again on a canvas the browser cleared, without animating', () => {
        const { engine, drawn, preview, clear } = stage();
        const look = { sclera: '#000000', pupil: 'dot', pupilColor: ['#FFFFFF', '#FFFFFF'] } as const;
        preview.still({ emotion: 'sad' }, look);
        clear(); // the engine still holds the frozen scene: drawn again as it is
        expect(engine.update).not.toHaveBeenCalled();
        expect(drawn.mock.calls.map((call) => call.slice(1))).toEqual([
            [0, 0, 0, 0, SIZE, SIZE],
            [0, 0, 0, 0, SIZE, SIZE]
        ]);
    });

    it('does not catch up an hour spent in a hidden tab', () => {
        const { engine, preview, tick } = stage();
        preview.start();
        tick(3_600_000);
        expect(engine.update).toHaveBeenCalledTimes(8);
    });

    it('stops', () => {
        const { engine, preview, tick } = stage();
        preview.start();
        preview.stop();
        tick(1000);
        expect(engine.update).not.toHaveBeenCalled();
    });

    it('draws a still frame without animating', () => {
        const { engine, drawn, preview } = stage();
        const look = { sclera: '#000000', pupil: 'dot', pupilColor: ['#FFFFFF', '#FFFFFF'] } as const;
        preview.still({ emotion: 'sad' }, look);
        expect(engine.renderScene).toHaveBeenCalledExactlyOnceWith({ emotion: 'sad' }, look);
        expect(engine.update).not.toHaveBeenCalled();
        expect(drawn).toHaveBeenCalledOnce();
        preview.still({ emotion: 'sad' }, look); // a new picture, not the one before with a region changed
        expect(drawn.mock.calls.map((call) => call.slice(1))).toEqual([
            [0, 0, 0, 0, SIZE, SIZE],
            [0, 0, 0, 0, SIZE, SIZE]
        ]);
    });
});

describe('engine loading', () => {
    afterEach(() => vi.unstubAllGlobals());

    it('loads the WebAssembly module once', async () => {
        vi.stubGlobal('location', { href: 'http://localhost/' });
        await Promise.all([loadEyeEngine(), loadEyeEngine()]);
        expect(loadEngine).toHaveBeenCalledOnce();
    });
});

describe('pointer', () => {
    const element = { getBoundingClientRect: () => ({ left: 100, top: 100, width: 200, height: 200 }) } as Element;

    it('gives the position from the center of the element', () => {
        expect(pointerIn(element, 200, 200)).toEqual({ x: 0, y: 0 });
        expect(pointerIn(element, 250, 150)).toEqual({ x: 0.5, y: -0.5 });
    });

    it('keeps it inside the circle', () => {
        const { x, y } = pointerIn(element, 900, 900);
        expect(Math.hypot(x, y)).toBeCloseTo(1);
    });
});
