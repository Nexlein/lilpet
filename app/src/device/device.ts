// What the app asks the device, on top of a connection
import type { Look } from '../data/looks';
import type { Connection } from './connection';
import { HELLO, lookCommand, parseHello, replyError, type DeviceInfo } from './protocol';

const HELLO_ATTEMPTS = 3;

// Asked a few times: a board that reboots as the port opens misses the first one
export async function hello(connection: Connection): Promise<DeviceInfo> {
    let reply: string | null = null;
    for (let i = 0; i < HELLO_ATTEMPTS && reply === null; i++)
        // oxlint-disable-next-line no-await-in-loop -- each attempt waits for the previous one to time out
        reply = await connection
            .request(HELLO, 1000)
            .catch((err: unknown) => (i === HELLO_ATTEMPTS - 1 ? Promise.reject(err) : null));
    const info = parseHello(reply ?? '');
    if (!info) throw new Error(`not a lilpet device (reply: ${reply})`);
    return info;
}

// A command that answers "ok" or "err <reason>"
export async function command(connection: Connection, line: string): Promise<void> {
    const error = replyError(await connection.request(line));
    if (error) throw new Error(error);
}

export type LookSent = 'kept' | 'shown';

// The device shows the look at once and keeps it in flash; 'shown': the flash refused it, the look is only on screen
export async function sendLook(connection: Connection, look: Look): Promise<LookSent> {
    const error = replyError(await connection.request(lookCommand(look)));
    if (error?.startsWith('shown')) return 'shown';
    if (error) throw new Error(error);
    return 'kept';
}
