// The connection to lilpet as a small state machine, outside React so it can be tested: pick a port, check it is a
// lilpet, send looks, notice when it goes away. The UI subscribes to its snapshot (ui/hooks.ts).
import type { Look } from '../data/looks';
import { Connection, type PortLike } from './connection';
import { hello, sendLook, type DeviceInfo } from './device';

export type DeviceState =
    | { status: 'unsupported' }
    | { status: 'idle'; error?: string }
    | { status: 'connecting' }
    | { status: 'connected'; info: DeviceInfo };

export interface DeviceSnapshot {
    readonly state: DeviceState;
    readonly log: readonly string[];
}

const LOG_LINES = 200;

export class DeviceSession {
    readonly #requestPort: (() => Promise<PortLike>) | null;
    readonly #listeners = new Set<() => void>();
    #snapshot: DeviceSnapshot;
    #connection: Connection | null = null;

    // requestPort: the browser's port picker, or null without Web Serial
    constructor(requestPort: (() => Promise<PortLike>) | null) {
        this.#requestPort = requestPort;
        this.#snapshot = { state: requestPort ? { status: 'idle' } : { status: 'unsupported' }, log: [] };
    }

    subscribe = (listener: () => void): (() => void) => {
        this.#listeners.add(listener);
        return () => this.#listeners.delete(listener);
    };

    snapshot = (): DeviceSnapshot => this.#snapshot;

    connect = async (): Promise<void> => {
        if (!this.#requestPort || this.#snapshot.state.status !== 'idle') return;
        let opened: Connection | null = null;
        try {
            const port = await this.#requestPort();
            this.#set({ state: { status: 'connecting' } });
            opened = await Connection.open(port, {
                onLog: (line) => this.#set({ log: [...this.#snapshot.log.slice(1 - LOG_LINES), line] }),
                onClose: () => {
                    if (this.#connection !== opened) return; // a connection that never got through hello
                    this.#connection = null;
                    this.#set({ state: { status: 'idle' } });
                }
            });
            const info = await hello(opened);
            this.#connection = opened;
            this.#set({ state: { status: 'connected', info } });
        } catch (err) {
            await opened?.close();
            // Closing the port picker without choosing is not an error
            const cancelled = err instanceof DOMException && err.name === 'NotFoundError';
            this.#set({ state: cancelled ? { status: 'idle' } : { status: 'idle', error: describe(err) } });
        }
    };

    disconnect = async (): Promise<void> => {
        await this.#connection?.close();
    };

    send = async (look: Look): Promise<void> => {
        if (!this.#connection) throw new Error('lilpet n’est plus branché');
        await sendLook(this.#connection, look);
    };

    #set(next: Partial<DeviceSnapshot>): void {
        this.#snapshot = { ...this.#snapshot, ...next };
        for (const listener of this.#listeners) listener();
    }
}

function describe(err: unknown): string {
    const message = err instanceof Error ? err.message : String(err);
    if (message.startsWith('not a lilpet device') || message.startsWith('no reply'))
        return 'Cet appareil ne répond pas comme lilpet. Flashe-le avec « mise run firmware:flash », puis réessaie.';
    if (err instanceof DOMException && err.name === 'NetworkError')
        return 'Le port est déjà ouvert ailleurs (moniteur série ?). Ferme-le, puis réessaie.';
    return message;
}
