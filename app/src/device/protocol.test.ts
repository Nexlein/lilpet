import { describe, expect, it, vi } from 'vitest';
import { findSwatch, makeLook } from '../data/looks';
import {
    LineSplitter,
    MAX_LINE,
    classify,
    emotionCommand,
    lookCommand,
    parseHello,
    reactCommand,
    replyError
} from './protocol';
import { Connection, ConnectionClosed } from './connection';
import { command, hello, sendLook } from './device';
import { fakePort, firmware } from './fake-port';

describe('protocol', () => {
    const socks = makeLook(findSwatch('socks')!, 'acorn');

    it('writes the look command the firmware reads', () => {
        expect(lookCommand(socks)).toBe('> look B865A8 76D8EB 8F1716 acorn');
    });

    it('writes the commands that move the eyes', () => {
        expect(emotionCommand('happy')).toBe('> emotion happy');
        expect(reactCommand('tap')).toBe('> react tap');
    });

    it('tells replies from log lines', () => {
        expect(classify('< ok')).toEqual({ kind: 'reply', text: 'ok' });
        expect(classify('frames/s 60.0')).toEqual({ kind: 'log', text: 'frames/s 60.0' });
    });

    it('reads the device info', () => {
        expect(parseHello('lilpet fw=2e62a96 engine=0.6.0')).toEqual({ firmware: '2e62a96', engine: '0.6.0' });
        expect(parseHello('lilpet fw=0.5.0 engine=0.13.0 board=atoms3r camera=0 motion=1')).toEqual({
            firmware: '0.5.0',
            engine: '0.13.0',
            board: 'atoms3r',
            camera: false,
            motion: true
        });
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
        expect(await hello(connection)).toMatchObject({ firmware: 'abc1234', engine: '0.6.0', board: 'atoms3r' });
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
        const device = fakePort((line) => (line === '> hello' && ++hellos === 1 ? null : answer(line)));
        const connection = await Connection.open(device.port);
        expect((await hello(connection)).firmware).toBe('abc1234');
        expect(hellos).toBe(2);
        await connection.close();
    });

    it('stops asking hello once the board is gone', async () => {
        const device = fakePort(() => null);
        const connection = await Connection.open(device.port);
        const asking = hello(connection);
        await vi.waitFor(() => expect(device.sent).toHaveLength(1));
        device.readError({ fatal: true });
        await expect(asking).rejects.toThrow(ConnectionClosed);
        expect(device.sent).toHaveLength(1);
    });

    it('refuses commands once closed, without writing', async () => {
        const device = fakePort(firmware());
        const connection = await Connection.open(device.port);
        await connection.close();
        await expect(connection.request('> hello')).rejects.toThrow(ConnectionClosed);
        expect(device.sent).toEqual([]);
    });

    it('fails a command at once when writing to the port fails', async () => {
        const device = fakePort(firmware());
        const connection = await Connection.open(device.port);
        device.failWrites();
        await expect(connection.request('> hello', 5000)).rejects.toThrow(ConnectionClosed);
        await connection.close();
    });

    it('tells a look kept from a look the flash refused, and from a refused look', async () => {
        let reply = 'ok';
        const device = fakePort(() => reply);
        const connection = await Connection.open(device.port);
        const look = makeLook(findSwatch('kite')!, 'cat');
        expect(await sendLook(connection, look)).toBe('kept');
        reply = 'err shown, but cannot save the look in flash';
        expect(await sendLook(connection, look)).toBe('shown');
        reply = 'err malformed look';
        await expect(sendLook(connection, look)).rejects.toThrow('malformed look');
        await expect(command(connection, '> emotion furious')).rejects.toThrow('malformed look');
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
