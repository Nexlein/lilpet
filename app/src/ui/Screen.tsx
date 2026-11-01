// The device's round screen, with the eyes drawn live by the C engine. The eyes follow the mouse anywhere on the page,
// and a click pets lilpet.
import { useEffect, useRef } from 'react';
import { Preview, pointerIn, type Engine } from '../engine/preview';
import type { Look } from '../data/looks';

interface Props {
    engine: Engine;
    look: Look;
}

export function Screen({ engine, look }: Props) {
    const canvas = useRef<HTMLCanvasElement>(null);

    useEffect(() => {
        if (!canvas.current) return;
        engine.init(Date.now() >>> 0);
        const preview = new Preview(engine, canvas.current);
        preview.start();
        const el = canvas.current;
        const move = (e: PointerEvent) => engine.setPointer(pointerIn(el, e.clientX, e.clientY));
        const leave = () => engine.setPointer(null);
        window.addEventListener('pointermove', move);
        document.documentElement.addEventListener('pointerleave', leave);
        return () => {
            preview.stop();
            window.removeEventListener('pointermove', move);
            document.documentElement.removeEventListener('pointerleave', leave);
        };
    }, [engine]);

    useEffect(() => engine.setLook(look), [engine, look]);

    const pet = () => {
        engine.setEmotion('happy', 1.8);
        for (let i = 0; i < 4; i++) engine.spawn('sparkle');
    };

    return (
        <canvas
            ref={canvas}
            className="screen"
            onClick={pet}
            role="img"
            aria-label="Les yeux de lilpet, en direct. Clique pour le caresser."
        />
    );
}
