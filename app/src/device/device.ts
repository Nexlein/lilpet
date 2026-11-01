// What the app asks the device, on top of a connection
import type { Look } from '../data/looks';
import type { Connection } from './connection';
import { HELLO, LOOK_QUERY, lookCommand, parseHello, parseLook, replyError, type DeviceInfo } from './protocol';

export type { DeviceInfo };

export async function hello(connection: Connection): Promise<DeviceInfo> {
    const reply = await connection.request(HELLO);
    const info = parseHello(reply);
    if (!info) throw new Error(`not a lilpet device (reply: ${reply})`);
    return info;
}

// The device shows the look at once and keeps it in flash
export async function sendLook(connection: Connection, look: Look): Promise<void> {
    const error = replyError(await connection.request(lookCommand(look)));
    if (error) throw new Error(error);
}

export async function storedLook(connection: Connection): Promise<Look | null> {
    return parseLook(await connection.request(LOOK_QUERY));
}
