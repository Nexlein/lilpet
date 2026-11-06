// The connection to lilpet as a small state machine, outside React so it can be tested: pick a port, check it is a
// lilpet, send looks, notice when it goes away. The UI subscribes to its snapshot (ui/hooks.ts).
import type { EyeEvent } from '../../../engine/wasm/engine.mjs';
import type { Look } from '../data/looks';
import { Connection, ConnectionClosed, type PortLike } from './connection';
import { command, hello, sendLook, type LookSent } from './device';
import { emotionCommand, reactCommand, type DeviceInfo } from './protocol';

export type DeviceState =
    | { status: 'unsupported' }
    | { status: 'idle'; error?: string }
    | { status: 'connecting' }
    | { status: 'connected'; info: DeviceInfo };

interface DeviceSnapshot {
    readonly state: DeviceState;
    readonly log: readonly string[];
}

const LOG_LINES = 200;

const NOT_LILPET =
    'Cet appareil ne répond pas comme lilpet. Flashe-le avec « mise run firmware:flash », puis réessaie.';
const NO_REPLY = 'lilpet ne répond pas. Débranche-le, rebranche-le, puis réessaie.';

export class DeviceSession {
    readonly #requestPort: (() => Promise<PortLike>) | null;
    readonly #listeners = new Set<() => void>();
    #snapshot: DeviceSnapshot;
    #connection: Connection | null = null;
    #mood: string | null = null; // the last one picked: the interface shows it, so a board plugged in later gets it

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
                onLog: (line) => this.#log(line),
                onClose: () => {
                    if (this.#connection !== opened) return; // a connection that never got through hello
                    this.#connection = null;
                    this.#set({ state: { status: 'idle' } });
                }
            });
            const info = await hello(opened);
            this.#connection = opened;
            this.#set({ state: { status: 'connected', info } });
            if (this.#mood) this.#tell(emotionCommand(this.#mood));
        } catch (err) {
            await opened?.close();
            // Closing the port picker without choosing is not an error
            const cancelled = err instanceof DOMException && err.name === 'NotFoundError';
            this.#set({ state: cancelled ? { status: 'idle' } : { status: 'idle', error: describe(err, NOT_LILPET) } });
        }
    };

    disconnect = async (): Promise<void> => {
        await this.#connection?.close();
    };

    // Fails with a message for the interface
    send = async (look: Look): Promise<LookSent> => {
        try {
            if (!this.#connection) throw new ConnectionClosed();
            return await sendLook(this.#connection, look);
        } catch (err) {
            throw new Error(describe(err), { cause: err });
        }
    };

    emotion = (name: string): void => {
        this.#mood = name;
        this.#tell(emotionCommand(name));
    };

    react = (event: EyeEvent): void => this.#tell(reactCommand(event));

    // Tells the board something in passing, if it is there: same queue as the looks, and a failure only goes to the log
    #tell(line: string): void {
        if (!this.#connection) return;
        command(this.#connection, line).catch((err: unknown) => this.#log(`${line} : ${describe(err)}`));
    }

    #log(line: string): void {
        this.#set({ log: [...this.#snapshot.log.slice(1 - LOG_LINES), line] });
    }

    #set(next: Partial<DeviceSnapshot>): void {
        this.#snapshot = { ...this.#snapshot, ...next };
        for (const listener of this.#listeners) listener();
    }
}

// In the words of the interface. silent: what no reply means, which depends on what was asked
function describe(err: unknown, silent = NO_REPLY): string {
    const message = err instanceof Error ? err.message : String(err);
    if (err instanceof ConnectionClosed) return 'lilpet n’est plus branché.';
    if (message === 'busy') return 'lilpet est occupé. Réessaie dans un instant.';
    if (message.startsWith('not a lilpet device') || message.startsWith('no reply')) return silent;
    if (err instanceof DOMException && err.name === 'NetworkError')
        return 'Le port est déjà ouvert ailleurs (moniteur série ?). Ferme-le, puis réessaie.';
    return `Erreur de lilpet : ${message}`;
}
