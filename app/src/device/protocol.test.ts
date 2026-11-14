import { describe, expect, it, vi } from 'vitest';
import COMMANDS_C from '../../../firmware/main/commands.c?raw';
import { findSwatch, makeLook } from '../data/looks';
import {
    HELLO,
    LineSplitter,
    MAX_LINE,
    classify,
    emotionCommand,
    lookCommand,
    parseHello,
    parseReply,
    reactCommand
} from './protocol';
import { Connection, ConnectionClosed, portPicker } from './connection';
import { NotLilpet, command, hello, sendLook } from './device';
import { FAKE_HELLO, FAKE_INFO, fakePort, firmware } from './fake-port';

// The field names of a hello reply
const fieldNames = (reply: string) => reply.split(' ').map((word) => word.split('=')[0]);

// What the app reads of the firmware's replies, compared with what firmware/main/commands.c prints
describe('the firmware', () => {
    const format = /"< (lilpet [^"]*)\\n"/.exec(COMMANDS_C)?.[1] ?? '';

    it('writes a hello the app reads, whatever the order of its fields', () => {
        const reply = format.replace(/(\w+)=%[sd]/g, '$1=v-$1'); // "lilpet fw=v-fw engine=v-engine ..."
        expect(parseHello(reply)).toEqual({ firmware: 'v-fw', engine: 'v-engine', board: 'v-board' });
    });

    it('has the fields of the fake board', () => {
        expect(fieldNames(FAKE_HELLO).toSorted()).toEqual(fieldNames(format).toSorted());
    });

    it('writes the replies the app tells apart', () => {
        expect(COMMANDS_C).toContain('printf("< err busy\\n");');
        expect(COMMANDS_C).toContain('printf("< err shown, ');
    });
});

describe('protocol', () => {
    const socks = makeLook(findSwatch('socks')!, 'acorn');

    it('writes the look command the firmware reads', () => {
        expect(lookCommand(socks)).toBe('> look B865A8 76D8EB 8F1716 acorn');
    });

    it('writes the commands that move the eyes', () => {
        expect(emotionCommand('happy')).toBe('> emotion happy');
        expect(reactCommand('tap')).toBe('> react tap');
        // @ts-expect-error -- tsc -p app fails here if the type stops coming from the reactions of eyes/emotions.json
        expect(reactCommand('purr')).toBe('> react purr');
    });

    it('tells replies from log lines', () => {
        expect(classify('< ok')).toEqual({ kind: 'reply', text: 'ok' });
        expect(classify('frames/s 60.0')).toEqual({ kind: 'log', text: 'frames/s 60.0' });
    });

    it('reads the device info', () => {
        expect(parseHello('lilpet fw=0.5.0 engine=0.13.0 board=atoms3r camera=0 motion=1')).toEqual({
            firmware: '0.5.0',
            engine: '0.13.0',
            board: 'atoms3r'
        });
    });

    it('rejects a hello that is not a lilpet, or from firmware older than its board name', () => {
        expect(parseHello('something else')).toBeNull();
        expect(parseHello('lilpet fw=1')).toBeNull();
        expect(parseHello('lilpet fw=2e62a96 engine=0.6.0')).toBeNull();
    });

    it('reads replies', () => {
        expect(parseReply('ok')).toEqual({ kind: 'ok' });
        expect(parseReply('err shown, but cannot save the look in flash')).toEqual({ kind: 'shown' });
        expect(parseReply('err busy')).toEqual({ kind: 'busy' });
        expect(parseReply('err bad color')).toEqual({ kind: 'refused', reason: 'bad color' });
        expect(parseReply('err')).toEqual({ kind: 'refused', reason: 'unknown error' });
        expect(parseReply('what')).toEqual({ kind: 'refused', reason: 'unexpected reply: what' });
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
        expect(await hello(connection)).toMatchObject(FAKE_INFO);
        expect(device.writes[0]).toBe('\n');
        const look = makeLook(findSwatch('kite')!, 'cat');
        // Sent together: the second waits for the first reply
        await Promise.all([sendLook(connection, look), sendLook(connection, look)]);
        expect(device.sent).toEqual([HELLO, '> look 7DB5F4 C10787 C10787 cat', '> look 7DB5F4 C10787 C10787 cat']);
        expect(logs).toEqual(Array(3).fill('draw 1.2 ms'));
        await connection.close();
        expect(device.closed()).toBe(true);
    });

    it('asks hello again when the board misses the first one while it boots', async () => {
        let hellos = 0;
        const answer = firmware();
        const device = fakePort((line) => (line === HELLO && ++hellos === 1 ? null : answer(line)));
        const connection = await Connection.open(device.port);
        expect((await hello(connection)).firmware).toBe(FAKE_INFO.firmware);
        expect(hellos).toBe(2);
        await connection.close();
    });

    it('asks hello again when a reply left from before a reload answers the first one', async () => {
        const device = fakePort(firmware(), { atOpen: '< ok\n', delay: () => 5 });
        const logs: string[] = [];
        const connection = await Connection.open(device.port, { onLog: (l) => logs.push(l) });
        expect((await hello(connection)).firmware).toBe(FAKE_INFO.firmware);
        expect(device.sent).toEqual([HELLO, HELLO]);
        // The second hello's reply comes after: the look behind it does not take it for its own
        expect(await sendLook(connection, makeLook(findSwatch('kite')!, 'cat'))).toBe('kept');
        expect(logs).toContain(`< ${FAKE_HELLO}`);
        await connection.close();
    });

    it('logs a hello reply that comes after its timeout, instead of taking it for the next command reply', async () => {
        vi.useFakeTimers();
        const waits = [1200, 0, 300, 300]; // the first hello answers late, after the second one
        const device = fakePort(firmware(), { delay: () => waits.shift() ?? 0 });
        const logs: string[] = [];
        const connection = await Connection.open(device.port, { onLog: (l) => logs.push(l) });
        const look = makeLook(findSwatch('kite')!, 'cat');
        const sending = (async () => {
            await hello(connection);
            return [await sendLook(connection, look), await sendLook(connection, look)];
        })();
        await vi.advanceTimersByTimeAsync(3000);
        vi.useRealTimers();
        expect(await sending).toEqual(['kept', 'kept']);
        expect(logs).toContain(`< ${FAKE_HELLO}`);
        await connection.close();
    });

    it('gives up on a device that never answers like lilpet, after the last attempt', async () => {
        const device = fakePort(() => 'something else');
        const connection = await Connection.open(device.port);
        await expect(hello(connection)).rejects.toThrow(NotLilpet);
        expect(device.sent).toHaveLength(3);
        await connection.close();
    });

    it('stops asking hello once the board is gone', async () => {
        const device = fakePort(() => null);
        const connection = await Connection.open(device.port);
        const request = vi.spyOn(connection, 'request');
        const asking = hello(connection);
        await vi.waitFor(() => expect(device.sent).toHaveLength(1));
        device.readError({ fatal: true });
        await expect(asking).rejects.toThrow(ConnectionClosed);
        expect(device.sent).toHaveLength(1);
        expect(request).toHaveBeenCalledOnce(); // writing fails by now: only the count tells a retry
    });

    it('refuses commands once closed, without writing', async () => {
        const device = fakePort(firmware());
        const connection = await Connection.open(device.port);
        await connection.close();
        await expect(connection.request(HELLO)).rejects.toThrow(ConnectionClosed);
        expect(device.sent).toEqual([]);
    });

    it('fails a command at once when writing to the port fails', async () => {
        const device = fakePort(firmware());
        const connection = await Connection.open(device.port);
        device.failWrites();
        await expect(connection.request(HELLO, 5000)).rejects.toThrow(ConnectionClosed);
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
        await expect(command(connection, emotionCommand('furious'))).rejects.toThrow('malformed look');
        await connection.close();
    });

    it('fails a command that gets no reply, then keeps working', async () => {
        const device = fakePort(firmware());
        const connection = await Connection.open(device.port);
        await expect(connection.request('> unknown', 50)).rejects.toThrow('no reply to "> unknown"');
        expect((await hello(connection)).engine).toBe(FAKE_INFO.engine);
        await connection.close();
    });

    it('stops the clock of an answered command, which would cut off the next one', async () => {
        vi.useFakeTimers();
        let wait = 0;
        const device = fakePort(firmware(), { delay: () => wait });
        const connection = await Connection.open(device.port);
        await connection.request(HELLO, 100);
        wait = 200;
        const answered = expect(connection.request(HELLO, 1000)).resolves.toContain('lilpet');
        await vi.advanceTimersByTimeAsync(1000);
        vi.useRealTimers();
        await answered;
        await connection.close();
    });

    it('keeps reading after a passing read error', async () => {
        const device = fakePort(firmware());
        const connection = await Connection.open(device.port);
        device.readError({ fatal: false }); // buffer overrun: the port gets a new stream
        expect((await hello(connection)).firmware).toBe(FAKE_INFO.firmware);
        expect(device.closed()).toBe(false);
        await connection.close();
    });

    it('stops reading when a read error leaves the port the same stream', async () => {
        const device = fakePort(firmware());
        let closes = 0;
        await Connection.open(device.port, { onClose: () => closes++ });
        device.readError({ fatal: true, sameStream: true });
        await vi.waitFor(() => expect(closes).toBe(1));
        expect(device.closed()).toBe(true);
    });

    it('closes the port and says so when the board is unplugged', async () => {
        const device = fakePort(() => null); // never answers: the request is still waiting when the board goes
        let closes = 0;
        const connection = await Connection.open(device.port, { onClose: () => closes++ });
        const waiting = connection.request(HELLO, 5000);
        device.readError({ fatal: true });
        await expect(waiting).rejects.toThrow('connection closed');
        await connection.close(); // safe after an unplug, and twice
        await connection.close();
        expect(device.closed()).toBe(true);
        expect(closes).toBe(1);
    });

    it('offers Espressif boards only in the port picker, and no picker without Web Serial', async () => {
        const requestPort = vi.fn(async () => fakePort(firmware()).port);
        vi.stubGlobal('navigator', { serial: { requestPort } });
        await portPicker()?.();
        expect(requestPort).toHaveBeenCalledWith({ filters: [{ usbVendorId: 0x303a }] });
        vi.stubGlobal('navigator', {});
        expect(portPicker()).toBeNull();
        vi.unstubAllGlobals();
    });

    it('closes the port when it cannot set its signals', async () => {
        const device = fakePort(firmware(), { failSignals: true });
        await expect(Connection.open(device.port)).rejects.toThrow('signals');
        expect(device.closed()).toBe(true);
    });
});
