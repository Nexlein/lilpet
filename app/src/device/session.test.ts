import { describe, expect, it, vi } from 'vitest';
import { findSwatch, makeLook } from '../data/looks';
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
        expect(session.snapshot().state).toEqual({
            status: 'connected',
            info: { firmware: 'abc1234', engine: '0.6.0', board: 'atoms3r', camera: false, motion: true }
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

    it('tells the board moods, reactions and gazes in the order they came, behind the looks', async () => {
        const device = fakePort(firmware());
        const session = new DeviceSession(async () => device.port);
        await session.connect();
        session.emotion('happy');
        session.react('tap');
        session.gaze(null);
        await session.send(kite);
        expect(device.sent.slice(1)).toEqual([
            '> emotion happy',
            '> react tap',
            '> gaze off',
            '> look 7DB5F4 C10787 C10787 dot'
        ]);
        await session.disconnect();
    });

    it('writes a failed mood to the log, and tells nothing without a board', async () => {
        const answer = firmware();
        const device = fakePort((c) => (c === '> hello' ? answer(c) : 'err unknown emotion'));
        const session = new DeviceSession(async () => device.port);
        session.emotion('happy'); // nothing is connected yet
        await session.connect();
        expect(device.sent).toEqual(['> hello']);
        expect(session.snapshot().log.join()).not.toContain('happy');
        session.emotion('furious');
        await vi.waitFor(() =>
            expect(session.snapshot().log).toContain('> emotion furious : Erreur de lilpet : unknown emotion')
        );
        expect(session.snapshot().state.status).toBe('connected');
        await session.disconnect();
    });

    it('says when Web Serial is missing', async () => {
        const session = new DeviceSession(null);
        expect(session.snapshot().state).toEqual({ status: 'unsupported' });
        await session.connect();
        expect(session.snapshot().state).toEqual({ status: 'unsupported' });
    });
});
