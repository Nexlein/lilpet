// Text protocol with the device over USB (docs/protocole.md): commands start with "> ", replies with "< ", any
// other line is the device's log. Pure functions, tested without hardware.
import type { EyeEvent } from '../../../engine/wasm/engine.mjs';
import type { Look } from '../data/looks';

export interface DeviceInfo {
    firmware: string;
    engine: string;
    board: string;
}

type Line = { kind: 'reply'; text: string } | { kind: 'log'; text: string };

export const HELLO = '> hello';

const hex = (color: string): string => color.replace('#', '').toUpperCase();

// "> look AE0002 FF5252 FF5252 dot": eye, left pupil, right pupil, pupil type
export function lookCommand(look: Look): string {
    return `> look ${hex(look.sclera)} ${hex(look.pupilColor[0])} ${hex(look.pupilColor[1])} ${look.pupil}`;
}

// "> emotion happy": the new mood, for good
export const emotionCommand = (name: string): string => `> emotion ${name}`;

// "> react tap": the device picks the emotion from its reaction table
export const reactCommand = (event: EyeEvent): string => `> react ${event}`;

export function classify(line: string): Line {
    return line.startsWith('< ') ? { kind: 'reply', text: line.slice(2) } : { kind: 'log', text: line };
}

// "lilpet fw=0.6.0 engine=0.14.0 board=atoms3r camera=0 motion=1"
export function parseHello(reply: string): DeviceInfo | null {
    const words = reply.trim().split(/\s+/);
    if (words[0] !== 'lilpet') return null;
    const fields = new Map(words.slice(1).map((w) => w.split('=') as [string, string]));
    const firmware = fields.get('fw');
    const engine = fields.get('engine');
    const board = fields.get('board');
    if (!firmware || !engine || !board) return null;
    return { firmware, engine, board };
}

// The replies of the commands, as opposed to hello's: "ok" or "err <reason>" (docs/protocole.md)
export const isCommandReply = (reply: string): boolean => reply === 'ok' || reply.startsWith('err');

// A reply to a command, read once here: the rest of the app never looks at its text
type Reply =
    | { kind: 'ok' }
    | { kind: 'shown' } // the look is on screen, but the flash refused it for now: the device tries again
    | { kind: 'busy' }
    | { kind: 'refused'; reason: string };

// "ok", or "err <reason>"
export function parseReply(reply: string): Reply {
    if (reply === 'ok') return { kind: 'ok' };
    if (!reply.startsWith('err')) return { kind: 'refused', reason: `unexpected reply: ${reply}` };
    const reason = reply.slice(3).trim() || 'unknown error';
    if (reason === 'busy') return { kind: 'busy' };
    return reason.startsWith('shown') ? { kind: 'shown' } : { kind: 'refused', reason };
}

// Cuts a stream of text chunks into lines, whatever the line ending (\n or \r\n)
// Text that never ends a line (another board on the port) comes out in pieces of MAX_LINE, not as an endless line
export const MAX_LINE = 1024;

export class LineSplitter {
    #rest = '';

    push(chunk: string): string[] {
        const parts = (this.#rest + chunk).split(/\r?\n/);
        this.#rest = parts.pop() ?? '';
        while (this.#rest.length > MAX_LINE) {
            parts.push(this.#rest.slice(0, MAX_LINE));
            this.#rest = this.#rest.slice(MAX_LINE);
        }
        return parts;
    }
}
