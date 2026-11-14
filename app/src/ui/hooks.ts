// React state around the engine, the device connection and the browser
import { useEffect, useState, useSyncExternalStore } from 'react';
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

// The connection to lilpet (device/session.ts), closed when the app goes away. Its log is followed apart
// (useDeviceLog), so that a line redraws only what shows it
export function useDevice() {
    const [session] = useState(() => new DeviceSession(portPicker()));
    const state = useSyncExternalStore(session.subscribe, () => session.snapshot().state);
    useEffect(() => () => void session.disconnect(), [session]);
    const { connect, disconnect, send, emotion, react } = session;
    return { session, state, connect, disconnect, send, emotion, react };
}

// The lines lilpet wrote, for the component that shows them
export const useDeviceLog = (session: DeviceSession): readonly string[] =>
    useSyncExternalStore(session.subscribe, () => session.snapshot().log);

// A value kept in the browser: read once, written on every change. shared: read again when another tab writes it, so
// that a change here starts from theirs
export function useStored<T>(read: () => T, write: (value: T) => void, shared = false) {
    const [value, setValue] = useState<T>(read);
    useEffect(() => {
        if (!shared) return;
        const reload = () => setValue(read());
        addEventListener('storage', reload); // fired in the other tabs, never in the one that wrote
        return () => removeEventListener('storage', reload);
    }, [read, shared]);
    const update = (next: T) => {
        setValue(next);
        write(next);
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
