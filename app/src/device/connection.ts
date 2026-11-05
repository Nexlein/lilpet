// Serial connection to the device (Web Serial): reads its lines, sends commands one at a time and waits for each reply.
import { classify, LineSplitter } from './protocol';

// The parts of the Web Serial API we use, which TypeScript's DOM types do not include
export interface PortLike {
    open(options: { baudRate: number; bufferSize?: number }): Promise<void>;
    close(): Promise<void>;
    setSignals(signals: { dataTerminalReady?: boolean; requestToSend?: boolean }): Promise<void>;
    readonly readable: ReadableStream<Uint8Array> | null;
    readonly writable: WritableStream<Uint8Array> | null;
}

interface SerialApi {
    requestPort(options?: { filters: { usbVendorId: number }[] }): Promise<PortLike>;
}

// Espressif's USB vendor id: the ESP32-S3 native USB port of every board we use (M5Stamp S3A, AtomS3R, Waveshare).
// Without it, Linux also lists its built-in ttyS ports, which are never lilpet.
const ESPRESSIF_USB = 0x303a;

// The browser's port picker (needs a user click), listing Espressif boards only; null without Web Serial
export function portPicker(): (() => Promise<PortLike>) | null {
    const { serial } = navigator as Navigator & { serial?: SerialApi };
    return serial ? () => serial.requestPort({ filters: [{ usbVendorId: ESPRESSIF_USB }] }) : null;
}

// The port is gone, or going: no command can get through any more
export class ConnectionClosed extends Error {
    constructor() {
        super('connection closed');
    }
}

interface ConnectionEvents {
    onLog?(line: string): void;
    onClose?(): void;
}

interface Pending {
    resolve(reply: string): void;
    reject(error: Error): void;
}

export class Connection {
    readonly #port: PortLike;
    readonly #writer: WritableStreamDefaultWriter<Uint8Array>;
    readonly #events: ConnectionEvents;
    readonly #reading: Promise<void>;
    #reader: ReadableStreamDefaultReader<Uint8Array> | null = null;
    #closing = false;
    #pending: Pending | null = null;
    #queue: Promise<unknown> = Promise.resolve();

    static async open(port: PortLike, events: ConnectionEvents = {}): Promise<Connection> {
        // A larger buffer than the default 255 bytes: the board's startup log comes in one burst
        await port.open({ baudRate: 115200, bufferSize: 8192 });
        try {
            // On the ESP32-S3 USB port, RTS asserted without DTR resets the chip. Opening asserts both: release RTS
            // first, then DTR (checked on an AtomS3R: the other order reboots it).
            await port.setSignals({ requestToSend: false });
            await port.setSignals({ dataTerminalReady: false });
            return new Connection(port, events);
        } catch (err) {
            await port.close().catch(() => undefined);
            throw err;
        }
    }

    private constructor(port: PortLike, events: ConnectionEvents) {
        if (!port.readable || !port.writable) throw new Error('serial port is not open');
        this.#port = port;
        this.#events = events;
        this.#writer = port.writable.getWriter();
        this.#reading = this.#readLoop();
        // Ends any half line a previous connection left in the board's input, which would swallow the first command
        this.#writer.write(new TextEncoder().encode('\n')).catch(() => undefined);
    }

    // Reads lines until closed or unplugged. A passing read error (buffer overrun, framing) gives the port a new
    // stream: reading goes on with it. Once over, the port is released and closed, whatever ended it.
    async #readLoop(): Promise<void> {
        const splitter = new LineSplitter();
        const decoder = new TextDecoder();
        for (let stream = this.#port.readable; stream && !this.#closing;) {
            const reader = stream.getReader();
            this.#reader = reader;
            try {
                for (;;) {
                    // oxlint-disable-next-line no-await-in-loop -- a stream is read one chunk after the other
                    const { value, done } = await reader.read();
                    if (done) break;
                    for (const line of splitter.push(decoder.decode(value, { stream: true }))) this.#handle(line);
                }
            } catch {
                // Read error: a fresh stream means a passing one, the same or none means the port is gone
            } finally {
                reader.releaseLock();
            }
            const next = this.#port.readable;
            stream = next === stream ? null : next;
        }
        this.#reader = null;
        this.#pending?.reject(new ConnectionClosed());
        this.#writer.releaseLock();
        await this.#port.close().catch(() => undefined);
        this.#events.onClose?.();
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
                    resolve: (reply) => {
                        done();
                        resolve(reply);
                    },
                    reject: (error) => {
                        done();
                        reject(error);
                    }
                };
                // Once closed or broken, the stream refuses every write
                this.#writer.write(new TextEncoder().encode(`${command}\n`)).catch(() => {
                    this.#pending?.reject(new ConnectionClosed());
                });
            });
        const result = this.#queue.then(run);
        this.#queue = result.catch(() => undefined);
        return result;
    }

    // Safe to call more than once, or after the board was unplugged
    async close(): Promise<void> {
        this.#closing = true;
        await this.#reader?.cancel().catch(() => undefined);
        await this.#reading;
    }
}
