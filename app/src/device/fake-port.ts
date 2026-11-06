// A fake board on the other end of a serial port, for the tests: answers commands like the firmware, logs in between,
// and can fail like a real port (a passing read error, an unplug, a failed write).
import type { PortLike } from './connection';

export function firmware(): (command: string) => string | null {
    return (command) => {
        if (command === '> hello') return 'lilpet fw=abc1234 engine=0.6.0 board=atoms3r camera=0 motion=1';
        if (/^> (look|emotion|react) /.test(command)) return 'ok';
        return null; // no reply: the request must time out
    };
}

// Stands alone, using nothing from around it: tests/app.spec.mjs sends its text to the page, for the browser tests
// atOpen: what the board still had to say when the port opened, which comes a moment later, as on a real port
export function fakePort(
    answer: (command: string) => string | null,
    { failSignals = false, delay = () => 0, atOpen = '' } = {}
) {
    const encoder = new TextEncoder();
    let device!: ReadableStreamDefaultController<Uint8Array>;
    // As in Chrome, a cancelled stream makes way for a new one: only closing the port ends reading
    const stream = (): ReadableStream<Uint8Array> =>
        new ReadableStream<Uint8Array>({
            start: (c) => void (device = c),
            cancel: () => void (readable = stream())
        });
    const sent: string[] = [];
    const writes: string[] = [];
    const signals: object[] = [];
    let options: { baudRate: number; bufferSize?: number } | undefined;
    let readable: ReadableStream<Uint8Array> | null = null;
    let writable: WritableStream<Uint8Array> | null = null;
    let closed = false;
    let writesFail = false;

    const output = () =>
        new WritableStream<Uint8Array>({
            write(chunk) {
                if (writesFail) throw new Error('device lost');
                const text = new TextDecoder().decode(chunk);
                writes.push(text);
                const command = text.trim();
                if (!command) return; // the newline sent on opening
                sent.push(command);
                const reply = answer(command);
                // A log line first, then the reply on a line of its own, split in two chunks as a real serial port may
                device.enqueue(encoder.encode('draw 1.2 ms\r\n'));
                if (reply === null) return;
                const from = readable;
                const send = () => {
                    if (readable !== from) return; // a late reply goes with its stream: unplugged, or lost in an error
                    device.enqueue(encoder.encode('\n< '));
                    device.enqueue(encoder.encode(`${reply}\r\n`));
                };
                const wait = delay();
                if (wait > 0) setTimeout(send, wait);
                else send();
            }
        });

    const port: PortLike = {
        // As in Chrome, a port does not open twice
        open: async (opts) => {
            if (writable) throw new DOMException('The port is already open.', 'InvalidStateError');
            options = opts;
            closed = false;
            readable = stream();
            writable = output();
            if (atOpen) setTimeout(() => device.enqueue(encoder.encode(atOpen)));
        },
        // As in Chrome, a port does not close while a stream is locked
        close: async () => {
            if (readable?.locked || writable?.locked) throw new TypeError('a stream of the port is locked');
            closed = true;
            readable = null;
            writable = null;
        },
        setSignals: async (s) => {
            if (failSignals) throw new Error('cannot set the signals');
            signals.push(s);
        },
        get readable() {
            return readable;
        },
        get writable() {
            return writable;
        }
    };

    // Chrome gives the port a new stream after a passing error, and none once the device is gone. sameStream: a browser
    // that goes on handing out the dead stream instead
    const readError = ({ fatal, sameStream = false }: { fatal: boolean; sameStream?: boolean }) => {
        const failing = device;
        if (!sameStream) readable = fatal ? null : stream();
        failing.error(new Error(fatal ? 'device lost' : 'buffer overrun'));
    };

    return {
        port,
        sent,
        writes,
        signals,
        readError,
        log: (line: string) => device.enqueue(encoder.encode(`${line}\r\n`)),
        failWrites: () => void (writesFail = true),
        options: () => options,
        closed: () => closed
    };
}
