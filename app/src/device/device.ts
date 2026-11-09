// What the app asks the device, on top of a connection
import type { Look } from '../data/looks';
import { ConnectionClosed, type Connection } from './connection';
import { HELLO, lookCommand, parseHello, parseReply, type DeviceInfo } from './protocol';

const HELLO_ATTEMPTS = 3;
// A command told in passing waits less than a look: a silent board would hold back the look queued behind ten moods
const COMMAND_TIMEOUT_MS = 500;

// The board is there, but its drawing loop cannot take a command now
export class BoardBusy extends Error {
    constructor() {
        super('busy');
    }
}

// Something answered hello, but not lilpet
export class NotLilpet extends Error {
    constructor(reply: string | null) {
        super(`not a lilpet device (reply: ${reply})`);
    }
}

// Asked a few times: a board that reboots as the port opens misses the first one, and a reply still on its way from
// before a page reload answers it. Once the board is gone, asking is over
export async function hello(connection: Connection): Promise<DeviceInfo> {
    let reply: string | null = null;
    for (let i = 0; i < HELLO_ATTEMPTS; i++) {
        // oxlint-disable-next-line no-await-in-loop -- each attempt waits for the previous one to time out
        reply = await connection
            .request(HELLO, 1000)
            .catch((err: unknown) =>
                err instanceof ConnectionClosed || i === HELLO_ATTEMPTS - 1 ? Promise.reject(err) : null
            );
        const info = parseHello(reply ?? '');
        if (info) return info;
    }
    throw new NotLilpet(reply);
}

// Sends a line and fails unless the board took it
async function ask(connection: Connection, line: string, timeoutMs?: number): Promise<'ok' | 'shown'> {
    const reply = parseReply(await connection.request(line, timeoutMs));
    if (reply.kind === 'busy') throw new BoardBusy();
    if (reply.kind === 'refused') throw new Error(reply.reason);
    return reply.kind;
}

// A command that answers "ok" or "err <reason>"
export async function command(connection: Connection, line: string): Promise<void> {
    await ask(connection, line, COMMAND_TIMEOUT_MS);
}

export type LookSent = 'kept' | 'shown';

// The device shows the look at once and keeps it in flash, at once or in its turn (docs/protocole.md); 'shown': the
// flash refused it, the look is only on screen
export async function sendLook(connection: Connection, look: Look): Promise<LookSent> {
    return (await ask(connection, lookCommand(look))) === 'shown' ? 'shown' : 'kept';
}
