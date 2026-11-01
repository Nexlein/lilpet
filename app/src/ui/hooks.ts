// React state around the engine, the device connection and the browser storage
import { useCallback, useEffect, useRef, useState } from 'react';
import { Connection, requestPort, serialSupported } from '../device/connection';
import { hello, sendLook, type DeviceInfo } from '../device/device';
import { loadEyeEngine, type Engine } from '../engine/preview';
import type { Look } from '../data/looks';

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

export type DeviceState =
    | { status: 'unsupported' }
    | { status: 'idle'; error?: string }
    | { status: 'connecting' }
    | { status: 'connected'; info: DeviceInfo };

const LOG_LINES = 200;

export function useDevice() {
    const [state, setState] = useState<DeviceState>(() =>
        serialSupported() ? { status: 'idle' } : { status: 'unsupported' }
    );
    const [log, setLog] = useState<string[]>([]);
    const connection = useRef<Connection | null>(null);

    const connect = useCallback(async () => {
        let opened: Connection | null = null;
        try {
            const port = await requestPort();
            setState({ status: 'connecting' });
            opened = await Connection.open(port, {
                onLog: (line) => setLog((lines) => [...lines.slice(1 - LOG_LINES), line]),
                onClose: () => {
                    connection.current = null;
                    setState((s) => (s.status === 'connected' ? { status: 'idle' } : s));
                }
            });
            const info = await hello(opened);
            connection.current = opened;
            setState({ status: 'connected', info });
        } catch (err) {
            await opened?.close().catch(() => undefined);
            // Closing the port picker without choosing is not an error
            if (err instanceof DOMException && err.name === 'NotFoundError') setState({ status: 'idle' });
            else setState({ status: 'idle', error: describe(err) });
        }
    }, []);

    const disconnect = useCallback(async () => {
        await connection.current?.close();
    }, []);

    const send = useCallback(async (look: Look) => {
        if (!connection.current) throw new Error('lilpet n’est plus branché');
        await sendLook(connection.current, look);
    }, []);

    useEffect(() => () => void connection.current?.close(), []);

    return { state, log, connect, disconnect, send };
}

function describe(err: unknown): string {
    const message = err instanceof Error ? err.message : String(err);
    if (message.startsWith('not a lilpet device') || message.startsWith('no reply'))
        return 'Cet appareil ne répond pas comme lilpet. Flashe-le avec « mise run firmware:flash », puis réessaie.';
    if (err instanceof DOMException && err.name === 'NetworkError')
        return 'Le port est déjà ouvert ailleurs (moniteur série ?). Ferme-le, puis réessaie.';
    return message;
}

// A value kept in localStorage: read once, written on every change
export function useStored<T>(read: (store: Storage) => T, write: (store: Storage, value: T) => void) {
    const [value, setValue] = useState<T>(() => read(localStorage));
    const update = useCallback(
        (next: T) => {
            setValue(next);
            write(localStorage, next);
        },
        [write]
    );
    return [value, update] as const;
}
