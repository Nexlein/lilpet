// Sends the chosen look to the device and says how it went
import { useState } from 'react';
import { lookKey } from '../data/favorites';
import { makeLook, type Look, type PupilType, type Swatch } from '../data/looks';
import type { LookSent } from '../device/device';
import type { DeviceState } from '../device/session';

interface Props {
    swatch: Swatch;
    pupil: PupilType;
    device: DeviceState;
    onSend(look: Look): Promise<LookSent>;
}

// The last send: the look and the connection it was about, and how it went
type LastSend = { name: string; key: string; device: DeviceState } & (
    { result: LookSent } | { result: 'failed'; message: string }
);

export function SendPanel({ swatch, pupil, device, onSend }: Props) {
    const [sending, setSending] = useState(false);
    const [last, setLast] = useState<LastSend | null>(null);
    const key = lookKey(swatch.id, pupil);
    const blocked = device.status !== 'connected' || sending;

    // One at a time, even if another look is chosen meanwhile: the result keeps the look it is about
    const send = async () => {
        if (blocked) return;
        const sent = { name: swatch.id, key, device };
        setSending(true);
        setLast(null);
        try {
            setLast({ ...sent, result: await onSend(makeLook(swatch, pupil)) });
        } catch (err) {
            setLast({ ...sent, result: 'failed', message: err instanceof Error ? err.message : String(err) });
        }
        setSending(false);
    };

    return (
        <div className="send">
            {/* Not disabled, as in DeviceBar: the keyboard focus would go away with it. A blocked click does nothing */}
            <button type="button" className="primary" aria-disabled={blocked} onClick={send}>
                {sending ? 'Envoi…' : 'Envoyer à lilpet'}
            </button>
            <p className="send-note" role="status">
                {sendNote(device, last, key)}
            </p>
        </div>
    );
}

// A failure stays until the next send, or until another connection: its message may say the board is gone. A success
// only holds for its look, on its connection: a board plugged in since may show something else.
function sendNote(device: DeviceState, last: LastSend | null, key: string): string {
    if (last?.result === 'failed' && (device.status !== 'connected' || last.device === device))
        return `${last.name} n’est pas envoyé : ${last.message}`;
    if (last?.device === device && last.key === key)
        return last.result === 'kept'
            ? `${last.name} est sur lilpet, et y reste même éteint.`
            : `${last.name} est sur lilpet, mais ne sera pas gardé.`;
    if (device.status === 'unsupported') return 'Pour envoyer un look, ouvre l’app dans Chrome ou Edge sur ordinateur.';
    if (device.status !== 'connected') return 'Branche lilpet en USB pour lui envoyer ce look.';
    return '';
}
