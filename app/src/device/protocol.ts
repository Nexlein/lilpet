// Text protocol with the device over USB (notes/app.md): commands start with "> ", replies with "< ", any other line
// is the device's log. Pure functions, tested without hardware.
import { isPupilType, type Look } from '../data/looks';

export interface DeviceInfo {
    firmware: string;
    engine: string;
}

export type Line = { kind: 'reply'; text: string } | { kind: 'log'; text: string };

export const HELLO = '> hello';
export const LOOK_QUERY = '> look?';

const hex = (color: string): string => color.replace('#', '').toUpperCase();
const color = (digits: string): string => `#${digits.toUpperCase()}`;

// "> look AE0002 FF5252 FF5252 dot": eye, left pupil, right pupil, pupil type
export function lookCommand(look: Look): string {
    return `> look ${hex(look.sclera)} ${hex(look.pupilColor[0])} ${hex(look.pupilColor[1])} ${look.pupil}`;
}

export function classify(line: string): Line {
    return line.startsWith('< ') ? { kind: 'reply', text: line.slice(2) } : { kind: 'log', text: line };
}

// "lilpet fw=2e62a96 engine=0.6.0"
export function parseHello(reply: string): DeviceInfo | null {
    const words = reply.trim().split(/\s+/);
    if (words[0] !== 'lilpet') return null;
    const fields = new Map(words.slice(1).map((w) => w.split('=') as [string, string]));
    const firmware = fields.get('fw');
    const engine = fields.get('engine');
    return firmware && engine ? { firmware, engine } : null;
}

// "look AE0002 FF5252 FF5252 dot", the reply to LOOK_QUERY
export function parseLook(reply: string): Look | null {
    const words = reply.trim().split(/\s+/);
    if (words.length !== 5 || words[0] !== 'look') return null;
    const [, sclera = '', left = '', right = '', pupil = ''] = words;
    if (![sclera, left, right].every((c) => /^[0-9A-Fa-f]{6}$/.test(c)) || !isPupilType(pupil)) return null;
    return { sclera: color(sclera), pupil, pupilColor: [color(left), color(right)] };
}

// "ok", or "err <reason>"
export function replyError(reply: string): string | null {
    if (reply === 'ok') return null;
    return reply.startsWith('err') ? reply.slice(3).trim() || 'unknown error' : `unexpected reply: ${reply}`;
}

// Cuts a stream of text chunks into lines, whatever the line ending (\n or \r\n)
export class LineSplitter {
    #rest = '';

    push(chunk: string): string[] {
        const parts = (this.#rest + chunk).split(/\r?\n/);
        this.#rest = parts.pop() ?? '';
        return parts;
    }
}
