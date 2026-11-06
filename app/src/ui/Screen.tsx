// The device's round screen, with the eyes drawn live by the C engine. The eyes follow the mouse anywhere on the page,
// and a click pets lilpet. Out of sight the animation pauses; when the system asks for less motion it is a still frame.
import { useEffect, useRef, useState } from 'react';
import { Preview, pointerIn, type Engine } from '../engine/preview';
import type { Look } from '../data/looks';
import { useLessMotion } from './hooks';

interface Props {
    engine: Engine;
    look: Look;
    emotion: string;
    onPet(): void;
}

export function Screen({ engine, look, emotion, onPet }: Props) {
    const canvas = useRef<HTMLCanvasElement>(null);
    const preview = useRef<Preview>(null);
    const lessMotion = useLessMotion();
    const [inView, setInView] = useState(true);

    useEffect(() => {
        if (!canvas.current) return;
        engine.init(Date.now() >>> 0);
        preview.current = new Preview(engine, canvas.current);
        const el = canvas.current;
        const move = (e: PointerEvent) => engine.setPointer(pointerIn(el, e.clientX, e.clientY));
        const leave = () => engine.setPointer(null);
        const observer = new IntersectionObserver((entries) => setInView(entries.at(-1)!.isIntersecting));
        observer.observe(el);
        window.addEventListener('pointermove', move);
        document.documentElement.addEventListener('pointerleave', leave);
        return () => {
            observer.disconnect();
            window.removeEventListener('pointermove', move);
            document.documentElement.removeEventListener('pointerleave', leave);
        };
    }, [engine]);

    useEffect(() => engine.setLook(look), [engine, look]);

    useEffect(() => {
        if (lessMotion || !inView) return;
        preview.current?.start();
        return () => preview.current?.stop();
    }, [lessMotion, inView]);

    useEffect(() => {
        if (lessMotion) preview.current?.still({ emotion }, look);
    }, [lessMotion, emotion, look]);

    return (
        <canvas
            ref={canvas}
            className="screen"
            onClick={onPet}
            role="img"
            aria-label="Les yeux de lilpet, en direct. Clique pour le caresser."
        />
    );
}
