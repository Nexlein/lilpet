// Serial connection to the device (Web Serial): reads its lines, sends commands one at a time and waits for each reply.
import { classify, LineSplitter } from './protocol';

// The parts of the Web Serial API we use, which TypeScript's DOM types do not include
export interface PortLike {
    open(options: { baudRate: number }): Promise<void>;
    close(): Promise<void>;
    setSignals?(signals: { dataTerminalReady?: boolean; requestToSend?: boolean }): Promise<void>;
    readonly readable: ReadableStream<Uint8Array> | null;
    readonly writable: WritableStream<Uint8Array> | null;
}

interface SerialApi {
    requestPort(options?: { filters: { usbVendorId: number }[] }): Promise<PortLike>;
}

// Espressif's USB vendor id: the ESP32-S3 native USB port of every board we use (M5Stamp S3A, AtomS3R, Waveshare).
// Without it, Linux also lists its built-in ttyS ports, which are never lilpet.
const ESPRESSIF_USB = 0x303a;

const serialApi = (): SerialApi | undefined => (navigator as Navigator & { serial?: SerialApi }).serial;

export const serialSupported = (): boolean => serialApi() !== undefined;

// Opens the browser's port picker (needs a user click), listing Espressif boards only
export async function requestPort(): Promise<PortLike> {
    const serial = serialApi();
    if (!serial) throw new Error('Web Serial unavailable: use Chrome or Edge on a computer');
    return serial.requestPort({ filters: [{ usbVendorId: ESPRESSIF_USB }] });
}

export interface ConnectionEvents {
    onLog?(line: string): void;
    onClose?(error?: unknown): void;
}

interface Pending {
    command: string;
    resolve(reply: string): void;
    reject(error: Error): void;
}

export class Connection {
    readonly #port: PortLike;
    readonly #reader: ReadableStreamDefaultReader<Uint8Array>;
    readonly #writer: WritableStreamDefaultWriter<Uint8Array>;
    readonly #events: ConnectionEvents;
    readonly #reading: Promise<void>;
    #pending: Pending | null = null;
    #queue: Promise<unknown> = Promise.resolve();

    static async open(port: PortLike, events: ConnectionEvents = {}): Promise<Connection> {
        await port.open({ baudRate: 115200 });
        // On the ESP32-S3 USB port, RTS asserted without DTR resets the chip. Opening asserts both: release RTS first,
        // then DTR (checked on an AtomS3R: the other order reboots it).
        await port.setSignals?.({ requestToSend: false });
        await port.setSignals?.({ dataTerminalReady: false });
        return new Connection(port, events);
    }

    private constructor(port: PortLike, events: ConnectionEvents) {
        if (!port.readable || !port.writable) throw new Error('serial port is not open');
        this.#port = port;
        this.#events = events;
        this.#reader = port.readable.getReader();
        this.#writer = port.writable.getWriter();
        this.#reading = this.#readLoop();
        // Ends any half line a previous connection left in the board's input, which would swallow the first command
        this.#writer.write(new TextEncoder().encode('\n')).catch(() => undefined);
    }

    async #readLoop(): Promise<void> {
        const splitter = new LineSplitter();
        const decoder = new TextDecoder();
        let error: unknown;
        try {
            for (;;) {
                // oxlint-disable-next-line no-await-in-loop -- a stream is read one chunk after the other
                const { value, done } = await this.#reader.read();
                if (done) break;
                for (const line of splitter.push(decoder.decode(value, { stream: true }))) this.#handle(line);
            }
        } catch (err) {
            error = err;
        }
        this.#pending?.reject(new Error('connection closed'));
        this.#events.onClose?.(error);
    }

    #handle(line: string): void {
        const parsed = classify(line);
        if (parsed.kind === 'reply' && this.#pending) this.#pending.resolve(parsed.text);
        else if (line) this.#events.onLog?.(line);
    }

    // Sends a command and resolves with its reply. Commands wait for the previous one, so replies never mix up.
    request(command: string, timeoutMs = 2000): Promise<string> {
        const run = () =>
            new Promise<string>((resolve, reject) => {
                const done = () => {
                    clearTimeout(timer);
                    this.#pending = null;
                };
                const timer = setTimeout(() => {
                    done();
                    reject(new Error(`no reply to "${command}"`));
                }, timeoutMs);
                this.#pending = {
                    command,
                    resolve: (reply) => {
                        done();
                        resolve(reply);
                    },
                    reject: (error) => {
                        done();
                        reject(error);
                    }
                };
                this.#writer.write(new TextEncoder().encode(`${command}\n`)).catch((err: unknown) => {
                    this.#pending?.reject(err instanceof Error ? err : new Error(String(err)));
                });
            });
        const result = this.#queue.then(run, run);
        this.#queue = result.catch(() => undefined);
        return result;
    }

    async close(): Promise<void> {
        await this.#reader.cancel().catch(() => undefined);
        await this.#reading;
        this.#reader.releaseLock();
        this.#writer.releaseLock();
        await this.#port.close();
    }
}
