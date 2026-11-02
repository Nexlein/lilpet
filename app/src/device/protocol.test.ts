import { describe, expect, it } from 'vitest';
import { findSwatch, makeLook } from '../data/looks';
import { LineSplitter, MAX_LINE, classify, lookCommand, parseHello, replyError } from './protocol';
import { Connection } from './connection';
import { hello, sendLook } from './device';
import { fakePort, firmware } from './fake-port';

describe('protocol', () => {
    const socks = makeLook(findSwatch('socks')!, 'acorn');

    it('writes the look command the firmware reads', () => {
        expect(lookCommand(socks)).toBe('> look B865A8 76D8EB 8F1716 acorn');
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

    it('cuts text that never ends a line into pieces', () => {
        const s = new LineSplitter();
        const lines = s.push('x'.repeat(MAX_LINE * 2 + 5));
        expect(lines).toEqual(['x'.repeat(MAX_LINE), 'x'.repeat(MAX_LINE)]);
        expect(s.push('\n')).toEqual(['xxxxx']);
    });
});

describe('connection', () => {
    it('opens like the board needs, talks to it and keeps log lines apart', async () => {
        const device = fakePort(firmware());
        const logs: string[] = [];
        const connection = await Connection.open(device.port, { onLog: (l) => logs.push(l) });
        // RTS released before DTR: the other order reboots an ESP32-S3; then a newline ends any half line
        expect(device.signals).toEqual([{ requestToSend: false }, { dataTerminalReady: false }]);
        expect(device.options()?.bufferSize).toBeGreaterThanOrEqual(4096);
        expect(await hello(connection)).toEqual({ firmware: 'abc1234', engine: '0.6.0' });
        expect(device.writes[0]).toBe('\n');
        const look = makeLook(findSwatch('kite')!, 'cat');
        // Sent together: the second waits for the first reply
        await Promise.all([sendLook(connection, look), sendLook(connection, look)]);
        expect(device.sent).toEqual(['> hello', '> look 7DB5F4 C10787 C10787 cat', '> look 7DB5F4 C10787 C10787 cat']);
        expect(logs).toEqual(Array(3).fill('draw 1.2 ms'));
        await connection.close();
        expect(device.closed()).toBe(true);
    });

    it('asks hello again when the board misses the first one while it boots', async () => {
        let hellos = 0;
        const answer = firmware();
        const device = fakePort((command) => (command === '> hello' && ++hellos === 1 ? null : answer(command)));
        const connection = await Connection.open(device.port);
        expect((await hello(connection)).firmware).toBe('abc1234');
        expect(hellos).toBe(2);
        await connection.close();
    });

    it('fails a command that gets no reply, then keeps working', async () => {
        const device = fakePort(firmware());
        const connection = await Connection.open(device.port);
        await expect(connection.request('> unknown', 50)).rejects.toThrow('no reply to "> unknown"');
        expect((await hello(connection)).engine).toBe('0.6.0');
        await connection.close();
    });

    it('keeps reading after a passing read error', async () => {
        const device = fakePort(firmware());
        const connection = await Connection.open(device.port);
        device.readError({ fatal: false }); // buffer overrun: the port gets a new stream
        expect((await hello(connection)).firmware).toBe('abc1234');
        expect(device.closed()).toBe(false);
        await connection.close();
    });

    it('closes the port and says so when the board is unplugged', async () => {
        const device = fakePort(() => null); // never answers: the request is still waiting when the board goes
        let closes = 0;
        const connection = await Connection.open(device.port, { onClose: () => closes++ });
        const waiting = connection.request('> hello', 5000);
        device.readError({ fatal: true });
        await expect(waiting).rejects.toThrow('connection closed');
        await connection.close(); // safe after an unplug, and twice
        await connection.close();
        expect(device.closed()).toBe(true);
        expect(closes).toBe(1);
    });

    it('closes the port when it cannot set its signals', async () => {
        const device = fakePort(firmware(), { failSignals: true });
        await expect(Connection.open(device.port)).rejects.toThrow('signals');
        expect(device.closed()).toBe(true);
    });
});
