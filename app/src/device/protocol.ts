// Text protocol with the device over USB (notes/app.md): commands start with "> ", replies with "< ", any other line
// is the device's log. Pure functions, tested without hardware.
import type { EyeEvent } from '../../../engine/wasm/engine.mjs';
import type { Look } from '../data/looks';

export interface DeviceInfo {
    firmware: string;
    engine: string;
    // Not given by older firmware
    board?: string;
    camera?: boolean;
    motion?: boolean;
}

type Line = { kind: 'reply'; text: string } | { kind: 'log'; text: string };

export const HELLO = '> hello';

const hex = (color: string): string => color.replace('#', '').toUpperCase();

// "> look AE0002 FF5252 FF5252 dot": eye, left pupil, right pupil, pupil type
export function lookCommand(look: Look): string {
    return `> look ${hex(look.sclera)} ${hex(look.pupilColor[0])} ${hex(look.pupilColor[1])} ${look.pupil}`;
}

// "> emotion happy" for good, "> emotion happy 2.5" for that many seconds
export const emotionCommand = (name: string, seconds?: number): string =>
    seconds === undefined ? `> emotion ${name}` : `> emotion ${name} ${seconds}`;

// "> react tap": the device picks the emotion from its reaction table
export const reactCommand = (event: EyeEvent): string => `> react ${event}`;

// "> gaze 0.500 -0.250": -1..1 from the center of the screen; null hands the gaze back to the device: "> gaze off"
export const gazeCommand = (point: { x: number; y: number } | null): string =>
    point ? `> gaze ${point.x.toFixed(3)} ${point.y.toFixed(3)}` : '> gaze off';

export function classify(line: string): Line {
    return line.startsWith('< ') ? { kind: 'reply', text: line.slice(2) } : { kind: 'log', text: line };
}

const flag = (value: string | undefined): boolean | undefined => (value === undefined ? undefined : value === '1');

// "lilpet fw=2e62a96 engine=0.6.0 board=atoms3r camera=0 motion=1"; older firmware stops after the engine
export function parseHello(reply: string): DeviceInfo | null {
    const words = reply.trim().split(/\s+/);
    if (words[0] !== 'lilpet') return null;
    const fields = new Map(words.slice(1).map((w) => w.split('=') as [string, string]));
    const firmware = fields.get('fw');
    const engine = fields.get('engine');
    if (!firmware || !engine) return null;
    return {
        firmware,
        engine,
        board: fields.get('board'),
        camera: flag(fields.get('camera')),
        motion: flag(fields.get('motion'))
    };
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
