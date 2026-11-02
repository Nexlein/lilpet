// What the app asks the device, on top of a connection
import type { Look } from '../data/looks';
import type { Connection } from './connection';
import { HELLO, lookCommand, parseHello, replyError, type DeviceInfo } from './protocol';

export type { DeviceInfo };

// Asked a few times: a board that reboots as the port opens misses the first one
export async function hello(connection: Connection, attempts = 3): Promise<DeviceInfo> {
    let reply: string | null = null;
    for (let i = 0; i < attempts && reply === null; i++)
        // oxlint-disable-next-line no-await-in-loop -- each attempt waits for the previous one to time out
        reply = await connection
            .request(HELLO, 1000)
            .catch((err: unknown) => (i === attempts - 1 ? Promise.reject(err) : null));
    const info = parseHello(reply ?? '');
    if (!info) throw new Error(`not a lilpet device (reply: ${reply})`);
    return info;
}

// The device shows the look at once and keeps it in flash
export async function sendLook(connection: Connection, look: Look): Promise<void> {
    const error = replyError(await connection.request(lookCommand(look)));
    if (error) throw new Error(error);
}
