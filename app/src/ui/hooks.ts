// React state around the engine, the device connection and the browser
import { useEffect, useState, useSyncExternalStore } from 'react';
import { browserStore, type KeyValueStore } from '../data/favorites';
import { portPicker } from '../device/connection';
import { DeviceSession } from '../device/session';
import { loadEyeEngine, type Engine } from '../engine/preview';

export type EngineState = { status: 'loading' } | { status: 'ready'; engine: Engine } | { status: 'error' };

export function useEngine(): EngineState {
    const [state, setState] = useState<EngineState>({ status: 'loading' });
    useEffect(() => {
        loadEyeEngine().then(
            (engine) => setState({ status: 'ready', engine }),
            (err: unknown) => {
                console.error('Cannot load the eye engine', err);
                setState({ status: 'error' });
            }
        );
    }, []);
    return state;
}

// The connection to lilpet (device/session.ts), closed when the app goes away
export function useDevice() {
    const [session] = useState(() => new DeviceSession(portPicker()));
    const { state, log } = useSyncExternalStore(session.subscribe, session.snapshot);
    useEffect(() => () => void session.disconnect(), [session]);
    const { connect, disconnect, send, emotion, react } = session;
    return { state, log, connect, disconnect, send, emotion, react };
}

// A value kept in the browser: read once, written on every change. shared: read again when another tab writes it, so
// that a change here starts from theirs
export function useStored<T>(
    read: (store: KeyValueStore) => T,
    write: (store: KeyValueStore, value: T) => void,
    shared = false
) {
    const [value, setValue] = useState<T>(() => read(browserStore));
    useEffect(() => {
        if (!shared) return;
        const reload = () => setValue(read(browserStore));
        addEventListener('storage', reload); // fired in the other tabs, never in the one that wrote
        return () => removeEventListener('storage', reload);
    }, [read, shared]);
    const update = (next: T) => {
        setValue(next);
        write(browserStore, next);
    };
    return [value, update] as const;
}

const lessMotion = () => matchMedia('(prefers-reduced-motion: reduce)');

function onMotionChange(listener: () => void): () => void {
    const query = lessMotion();
    query.addEventListener('change', listener);
    return () => query.removeEventListener('change', listener);
}

// Whether the system asks for less motion
export const useLessMotion = (): boolean => useSyncExternalStore(onMotionChange, () => lessMotion().matches);
