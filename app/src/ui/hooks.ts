// React state around the engine, the device connection and the browser
import { useEffect, useState, useSyncExternalStore } from 'react';
import { browserStore, type KeyValueStore } from '../data/favorites';
import { portPicker } from '../device/connection';
import { DeviceSession, type DeviceState } from '../device/session';
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

export type { DeviceState };

// The connection to lilpet (device/session.ts), closed when the app goes away
export function useDevice() {
    const [session] = useState(() => new DeviceSession(portPicker()));
    const { state, log } = useSyncExternalStore(session.subscribe, session.snapshot);
    useEffect(() => () => void session.disconnect(), [session]);
    return { state, log, connect: session.connect, disconnect: session.disconnect, send: session.send };
}

// A value kept in the browser: read once, written on every change
export function useStored<T>(read: (store: KeyValueStore) => T, write: (store: KeyValueStore, value: T) => void) {
    const [value, setValue] = useState<T>(() => read(browserStore));
    const update = (next: T) => {
        setValue(next);
        write(browserStore, next);
    };
    return [value, update] as const;
}
