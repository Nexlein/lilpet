// A fake board on the other end of a serial port, for the tests: answers commands like the firmware, logs in between,
// and can fail like a real port (a passing read error, an unplug).
import type { PortLike } from './connection';

export function firmware(): (command: string) => string | null {
    return (command) => {
        if (command === '> hello') return 'lilpet fw=abc1234 engine=0.6.0';
        if (command.startsWith('> look ')) return 'ok';
        return null; // no reply: the request must time out
    };
}

export function fakePort(answer: (command: string) => string | null, { failSignals = false } = {}) {
    const encoder = new TextEncoder();
    let device!: ReadableStreamDefaultController<Uint8Array>;
    const stream = () => new ReadableStream<Uint8Array>({ start: (c) => void (device = c) });
    const sent: string[] = [];
    const writes: string[] = [];
    const signals: object[] = [];
    let options: { baudRate: number; bufferSize?: number } | undefined;
    let readable: ReadableStream<Uint8Array> | null = null;
    let closed = false;

    const port: PortLike = {
        open: async (opts) => {
            options = opts;
            readable = stream();
        },
        close: async () => {
            closed = true;
            readable = null;
        },
        setSignals: async (s) => {
            if (failSignals) throw new Error('cannot set the signals');
            signals.push(s);
        },
        get readable() {
            return readable;
        },
        writable: new WritableStream<Uint8Array>({
            write(chunk) {
                const text = new TextDecoder().decode(chunk);
                writes.push(text);
                const command = text.trim();
                if (!command) return; // the newline sent on opening
                sent.push(command);
                const reply = answer(command);
                // A log line first, and the reply split in two chunks, as a real serial port may deliver it
                device.enqueue(encoder.encode('draw 1.2 ms\r\n'));
                if (reply === null) return;
                device.enqueue(encoder.encode('< '));
                device.enqueue(encoder.encode(`${reply}\r\n`));
            }
        })
    };

    // Chrome gives the port a new stream after a passing error, and none once the device is gone
    const readError = ({ fatal }: { fatal: boolean }) => {
        const failing = device;
        readable = fatal ? null : stream();
        failing.error(new Error(fatal ? 'device lost' : 'buffer overrun'));
    };

    return { port, sent, writes, signals, readError, options: () => options, closed: () => closed };
}
