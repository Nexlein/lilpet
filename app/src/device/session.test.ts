import { describe, expect, it, vi } from 'vitest';
import { findSwatch, makeLook } from '../data/looks';
import type { PortLike } from './connection';
import { fakePort, firmware } from './fake-port';
import { DeviceSession } from './session';

const kite = makeLook(findSwatch('kite')!, 'dot');

const busyPort = () => Promise.reject(new DOMException('Failed to open serial port.', 'NetworkError'));

describe('device session', () => {
    it('connects to lilpet, sends looks and logs the rest', async () => {
        const device = fakePort(firmware());
        const session = new DeviceSession(async () => device.port);
        const seen: string[] = [];
        session.subscribe(() => seen.push(session.snapshot().state.status));
        await session.connect();
        await session.connect(); // already connected: nothing happens
        expect(session.snapshot().state).toEqual({
            status: 'connected',
            info: { firmware: 'abc1234', engine: '0.6.0', board: 'atoms3r' }
        });
        expect(seen).toContain('connecting');
        expect(await session.send(kite)).toBe('kept');
        expect(session.snapshot().log).toContain('draw 1.2 ms');
        await session.disconnect();
        expect(session.snapshot().state).toEqual({ status: 'idle' });
        expect(device.closed()).toBe(true);
        await expect(session.send(kite)).rejects.toThrow('plus branché');
    });

    it('stays idle, with no message, when the port picker is closed', async () => {
        const session = new DeviceSession(() => Promise.reject(new DOMException('no port', 'NotFoundError')));
        await session.connect();
        expect(session.snapshot().state).toEqual({ status: 'idle' });
    });

    it('opens one port picker at a time, and a new one once it is closed', async () => {
        const device = fakePort(firmware());
        let choose!: (port: PortLike) => void;
        let cancel!: (reason: unknown) => void;
        const requestPort = vi.fn(
            () =>
                new Promise<PortLike>((resolve, reject) => {
                    choose = resolve;
                    cancel = reject;
                })
        );
        const session = new DeviceSession(requestPort);
        const first = session.connect();
        await session.connect(); // the picker is open and the state is still idle: nothing happens
        expect(requestPort).toHaveBeenCalledOnce();
        cancel(new DOMException('no port', 'NotFoundError'));
        await first;
        const second = session.connect();
        expect(requestPort).toHaveBeenCalledTimes(2);
        choose(device.port);
        await second;
        expect(session.snapshot().state.status).toBe('connected');
        await session.disconnect();
    });

    it('closes the port and explains when the device is not lilpet', async () => {
        const device = fakePort(() => 'something else');
        const session = new DeviceSession(async () => device.port);
        await session.connect();
        const state = session.snapshot().state;
        expect(state.status === 'idle' && state.error).toMatch(/ne répond pas comme lilpet/);
        expect(device.closed()).toBe(true);
    });

    it('goes back to idle when the board is unplugged', async () => {
        const device = fakePort(firmware());
        const session = new DeviceSession(async () => device.port);
        await session.connect();
        device.readError({ fatal: true });
        await new Promise((r) => setTimeout(r, 10));
        expect(session.snapshot().state).toEqual({ status: 'idle' });
        await session.connect(); // and can connect again
        expect(session.snapshot().state.status).toBe('connected');
        await session.disconnect();
    });

    it('goes back to idle when only writing to the board fails', async () => {
        const device = fakePort(firmware());
        const session = new DeviceSession(async () => device.port);
        await session.connect();
        device.failWrites();
        await expect(session.send(kite)).rejects.toThrow('plus branché');
        await vi.waitFor(() => expect(session.snapshot().state).toEqual({ status: 'idle' }));
        expect(device.closed()).toBe(true);
    });

    it('hears nothing from a reply cut off by an unplug, even plugged in again', async () => {
        let wait = 0;
        const device = fakePort(firmware(), { delay: () => wait });
        const session = new DeviceSession(async () => device.port);
        await session.connect();
        vi.useFakeTimers();
        wait = 5;
        const sending = expect(session.send(kite)).rejects.toThrow('plus branché');
        await vi.advanceTimersByTimeAsync(1); // the look is written, and its reply on its way
        device.readError({ fatal: true });
        await sending;
        await vi.advanceTimersByTimeAsync(1);
        wait = 0;
        await session.connect();
        await vi.advanceTimersByTimeAsync(5);
        vi.useRealTimers();
        expect(session.snapshot().state.status).toBe('connected');
        expect(session.snapshot().log.join()).not.toContain('< ok');
        await session.disconnect();
    });

    it('explains an unplug during hello, once, without writing again', async () => {
        const device = fakePort(() => null);
        const session = new DeviceSession(async () => device.port);
        const seen = new Set<unknown>(); // log lines also notify: each state counts once
        session.subscribe(() => seen.add(session.snapshot().state));
        const connecting = session.connect();
        await vi.waitFor(() => expect(device.sent).toHaveLength(1));
        device.readError({ fatal: true });
        await connecting;
        expect(device.sent).toHaveLength(1);
        expect([...seen]).toEqual([{ status: 'connecting' }, { status: 'idle', error: 'lilpet n’est plus branché.' }]);
    });

    it('says when another program holds the port', async () => {
        const session = new DeviceSession(async () => ({ ...fakePort(firmware()).port, open: busyPort }));
        await session.connect();
        const state = session.snapshot().state;
        expect(state.status === 'idle' && state.error).toMatch(/déjà ouvert ailleurs/);
    });

    it('explains a failed send in French', async () => {
        const answer = firmware();
        let reply: string | null = 'err malformed look';
        const device = fakePort((c) => (c === '> hello' ? answer(c) : reply));
        const session = new DeviceSession(async () => device.port);
        await session.connect();
        await expect(session.send(kite)).rejects.toThrow('Erreur de lilpet : malformed look');
        reply = null;
        vi.useFakeTimers();
        const silent = expect(session.send(kite)).rejects.toThrow('lilpet ne répond pas');
        await vi.advanceTimersByTimeAsync(2000);
        vi.useRealTimers();
        await silent;
        const unplugged = expect(session.send(kite)).rejects.toThrow('lilpet n’est plus branché');
        await vi.waitFor(() => expect(device.sent).toHaveLength(4));
        device.readError({ fatal: true });
        await unplugged;
    });

    it('explains a busy board in French', async () => {
        const answer = firmware();
        const device = fakePort((c) => (c === '> hello' ? answer(c) : 'err busy'));
        const session = new DeviceSession(async () => device.port);
        await session.connect();
        await expect(session.send(kite)).rejects.toThrow('lilpet est occupé');
        session.react('tap');
        await vi.waitFor(() => expect(session.snapshot().log.join()).toContain('> react tap : lilpet est occupé'));
        await session.disconnect();
    });

    it('keeps the last log lines, without the blank ones', async () => {
        const device = fakePort(firmware());
        const session = new DeviceSession(async () => device.port);
        await session.connect();
        for (let i = 0; i < 250; i++) device.log(`line ${i}`);
        await vi.waitFor(() => expect(session.snapshot().log.at(-1)).toBe('line 249'));
        expect(session.snapshot().log).toHaveLength(200);
        expect(session.snapshot().log).not.toContain('');
        await session.disconnect();
    });

    it('tells the board moods and reactions in the order they came, behind the looks', async () => {
        const device = fakePort(firmware());
        const session = new DeviceSession(async () => device.port);
        await session.connect();
        session.emotion('happy');
        session.react('tap');
        await session.send(kite);
        expect(device.sent.slice(1)).toEqual(['> emotion happy', '> react tap', '> look 7DB5F4 C10787 C10787 dot']);
        await session.disconnect();
    });

    it('gives the last mood picked to a board plugged in later, each time', async () => {
        const device = fakePort(firmware());
        const session = new DeviceSession(async () => device.port);
        session.emotion('happy'); // nothing is connected yet
        session.emotion('sad');
        await session.connect();
        await vi.waitFor(() => expect(device.sent).toEqual(['> hello', '> emotion sad']));
        await session.disconnect();
        await session.connect();
        await vi.waitFor(() => expect(device.sent.slice(2)).toEqual(['> hello', '> emotion sad']));
        await session.disconnect();
    });

    it('gives the mood again to a board that restarted with the port open', async () => {
        const device = fakePort(firmware());
        const session = new DeviceSession(async () => device.port);
        session.emotion('sad');
        await session.connect();
        await vi.waitFor(() => expect(device.sent).toEqual(['> hello', '> emotion sad']));
        device.log('I (250) main_task: Calling app_main()'); // a crash or the watchdog: the port stayed open
        await vi.waitFor(() => expect(device.sent).toEqual(['> hello', '> emotion sad', '> hello', '> emotion sad']));
        expect(session.snapshot().state.status).toBe('connected');
        await session.disconnect();
    });

    it('asks only hello of a board that restarted before any mood was picked', async () => {
        const device = fakePort(firmware());
        const session = new DeviceSession(async () => device.port);
        await session.connect();
        device.log('I (250) main_task: Calling app_main()');
        await vi.waitFor(() => expect(device.sent).toEqual(['> hello', '> hello']));
        await session.disconnect();
    });

    it('sends a mood picked while connecting, once', async () => {
        const device = fakePort(firmware());
        const session = new DeviceSession(async () => device.port);
        session.subscribe(() => {
            if (session.snapshot().state.status === 'connecting') session.emotion('sad');
        });
        await session.connect();
        await vi.waitFor(() => expect(device.sent).toEqual(['> hello', '> emotion sad']));
        await session.disconnect();
    });

    it('tells a board no mood until one is picked, and a failed one goes to the log', async () => {
        const answer = firmware();
        const device = fakePort((c) => (c === '> hello' ? answer(c) : 'err unknown emotion'));
        const session = new DeviceSession(async () => device.port);
        await session.connect();
        await new Promise((r) => setTimeout(r, 10));
        expect(device.sent).toEqual(['> hello']);
        session.emotion('furious');
        await vi.waitFor(() =>
            expect(session.snapshot().log).toContain('> emotion furious : Erreur de lilpet : unknown emotion')
        );
        expect(session.snapshot().state.status).toBe('connected');
        await session.disconnect();
    });

    it('gives up on moods soon, so a silent board does not hold back the look behind them for long', async () => {
        const answer = firmware();
        const device = fakePort((c) => (c === '> hello' ? answer(c) : null));
        const session = new DeviceSession(async () => device.port);
        await session.connect();
        vi.useFakeTimers();
        for (let i = 0; i < 10; i++) session.emotion('happy');
        const look = expect(session.send(kite)).rejects.toThrow('lilpet ne répond pas');
        await vi.advanceTimersByTimeAsync(10 * 500 + 2000);
        vi.useRealTimers();
        await look;
        await session.disconnect();
    });

    it('says when Web Serial is missing', async () => {
        const session = new DeviceSession(null);
        expect(session.snapshot().state).toEqual({ status: 'unsupported' });
        await session.connect();
        expect(session.snapshot().state).toEqual({ status: 'unsupported' });
    });
});
