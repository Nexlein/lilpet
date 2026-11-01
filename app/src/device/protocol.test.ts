import { describe, expect, it } from 'vitest';
import { findSwatch, makeLook } from '../data/looks';
import { LineSplitter, classify, lookCommand, parseHello, parseLook, replyError } from './protocol';
import { Connection, type PortLike } from './connection';
import { hello, sendLook, storedLook } from './device';

describe('protocol', () => {
    const socks = makeLook(findSwatch('socks')!, 'acorn');

    it('writes the look command the firmware reads', () => {
        expect(lookCommand(socks)).toBe('> look B865A8 76D8EB 8F1716 acorn');
    });

    it('reads back a look, and rejects malformed ones', () => {
        expect(parseLook('look B865A8 76D8EB 8F1716 acorn')).toEqual({
            sclera: '#B865A8',
            pupil: 'acorn',
            pupilColor: ['#76D8EB', '#8F1716']
        });
        expect(parseLook('look B865A8 76D8EB acorn')).toBeNull();
        expect(parseLook('look B865A8 76D8EB 8F1716 slit')).toBeNull();
        expect(parseLook('look B865AZ 76D8EB 8F1716 dot')).toBeNull();
    });

    it('tells replies from log lines', () => {
        expect(classify('< ok')).toEqual({ kind: 'reply', text: 'ok' });
        expect(classify('frames/s 60.0')).toEqual({ kind: 'log', text: 'frames/s 60.0' });
    });

    it('reads the device info', () => {
        expect(parseHello('lilpet fw=2e62a96 engine=0.6.0')).toEqual({ firmware: '2e62a96', engine: '0.6.0' });
        expect(parseHello('something else')).toBeNull();
        expect(parseHello('lilpet fw=1')).toBeNull();
    });

    it('reads errors', () => {
        expect(replyError('ok')).toBeNull();
        expect(replyError('err bad color')).toBe('bad color');
        expect(replyError('what')).toBe('unexpected reply: what');
    });

    it('cuts chunks into lines, whatever the line ending', () => {
        const s = new LineSplitter();
        expect(s.push('a\r\nb')).toEqual(['a']);
        expect(s.push('c\nd\n')).toEqual(['bc', 'd']);
        expect(s.push('')).toEqual([]);
    });
});

// A fake device on the other end of the port: answers commands like the firmware, and logs in between
function fakePort(answer: (command: string) => string | null) {
    const encoder = new TextEncoder();
    let device!: ReadableStreamDefaultController<Uint8Array>;
    const sent: string[] = [];
    let closed = false;
    const port: PortLike = {
        open: async () => undefined,
        close: async () => {
            closed = true;
        },
        readable: new ReadableStream<Uint8Array>({ start: (c) => void (device = c) }),
        writable: new WritableStream<Uint8Array>({
            write(chunk) {
                const command = new TextDecoder().decode(chunk).trim();
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
    return { port, sent, isClosed: () => closed };
}

describe('connection', () => {
    let stored = '';
    const firmware = (command: string): string | null => {
        if (command === '> hello') return 'lilpet fw=abc1234 engine=0.6.0';
        if (command === '> look?') return stored || 'err no look';
        if (command.startsWith('> look ')) {
            stored = command.slice(2);
            return 'ok';
        }
        return null; // no reply: the request must time out
    };

    it('talks to the device and keeps log lines apart', async () => {
        const { port, sent, isClosed } = fakePort(firmware);
        const logs: string[] = [];
        const connection = await Connection.open(port, { onLog: (l) => logs.push(l) });
        expect(await hello(connection)).toEqual({ firmware: 'abc1234', engine: '0.6.0' });
        const look = makeLook(findSwatch('kite')!, 'cat');
        // Sent together: the second waits for the first reply
        await Promise.all([sendLook(connection, look), storedLook(connection)]);
        expect(await storedLook(connection)).toEqual({ ...look, id: undefined, pupilColor: [...look.pupilColor] });
        expect(sent).toEqual(['> hello', '> look 7DB5F4 C10787 C10787 cat', '> look?', '> look?']);
        expect(logs).toEqual(Array(4).fill('draw 1.2 ms'));
        await connection.close();
        expect(isClosed()).toBe(true);
    });

    it('asks hello again when the board misses the first one while it boots', async () => {
        let hellos = 0;
        const { port } = fakePort((command) => (command === '> hello' && ++hellos === 1 ? null : firmware(command)));
        const connection = await Connection.open(port);
        expect((await hello(connection)).firmware).toBe('abc1234');
        expect(hellos).toBe(2);
        await connection.close();
    });

    it('fails a command that gets no reply, then keeps working', async () => {
        const { port } = fakePort(firmware);
        const connection = await Connection.open(port);
        await expect(connection.request('> unknown', 50)).rejects.toThrow('no reply to "> unknown"');
        expect((await hello(connection)).engine).toBe('0.6.0');
        await connection.close();
    });
});
