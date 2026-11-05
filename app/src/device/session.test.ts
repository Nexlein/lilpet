import { describe, expect, it, vi } from 'vitest';
import { findSwatch, makeLook } from '../data/looks';
import { fakePort, firmware } from './fake-port';
import { DeviceSession } from './session';

const kite = makeLook(findSwatch('kite')!, 'dot');

describe('device session', () => {
    it('connects to lilpet, sends looks and logs the rest', async () => {
        const device = fakePort(firmware());
        const session = new DeviceSession(async () => device.port);
        const seen: string[] = [];
        session.subscribe(() => seen.push(session.snapshot().state.status));
        await session.connect();
        expect(session.snapshot().state).toEqual({
            status: 'connected',
            info: { firmware: 'abc1234', engine: '0.6.0' }
        });
        expect(seen).toContain('connecting');
        await session.send(kite);
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

    it('says when Web Serial is missing', async () => {
        const session = new DeviceSession(null);
        expect(session.snapshot().state).toEqual({ status: 'unsupported' });
        await session.connect();
        expect(session.snapshot().state).toEqual({ status: 'unsupported' });
    });
});
