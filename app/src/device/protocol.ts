// Text protocol with the device over USB (notes/app.md): commands start with "> ", replies with "< ", any other line
// is the device's log. Pure functions, tested without hardware.
import type { Look } from '../data/looks';

export interface DeviceInfo {
    firmware: string;
    engine: string;
}

type Line = { kind: 'reply'; text: string } | { kind: 'log'; text: string };

export const HELLO = '> hello';

const hex = (color: string): string => color.replace('#', '').toUpperCase();

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

// "ok", or "err <reason>"
export function replyError(reply: string): string | null {
    if (reply === 'ok') return null;
    return reply.startsWith('err') ? reply.slice(3).trim() || 'unknown error' : `unexpected reply: ${reply}`;
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
