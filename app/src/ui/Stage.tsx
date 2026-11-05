// Left column: the device with the eyes, the chosen look and what can be done with it
import { useState } from 'react';
import { PUPIL_TYPES, makeLook, type PupilType, type Swatch } from '../data/looks';
import type { EngineState, DeviceState } from './hooks';
import type { Look } from '../data/looks';
import { EyeGlyph } from './EyeGlyph';
import { Screen } from './Screen';
import { EMOTION_LABELS, PUPIL_LABELS } from './labels';

interface Props {
    engine: EngineState;
    swatch: Swatch;
    pupil: PupilType;
    favorite: boolean;
    device: DeviceState;
    onPupil(pupil: PupilType): void;
    onFavorite(): void;
    onSend(look: Look): Promise<unknown>;
}

type SendState = { status: 'idle' } | { status: 'sending' } | { status: 'sent' } | { status: 'error'; message: string };
const IDLE: SendState = { status: 'idle' };

export function Stage({ engine, swatch, pupil, favorite, device, onPupil, onFavorite, onSend }: Props) {
    const look = makeLook(swatch, pupil);
    const [emotion, setEmotion] = useState('neutral');
    // The last send result only shows for the look and connection it was about
    const sendKey = `${swatch.id}/${pupil}/${device.status}`;
    const [lastSend, setLastSend] = useState<{ key: string; state: SendState }>({ key: '', state: IDLE });
    const sending = lastSend.key === sendKey ? lastSend.state : IDLE;
    const setSending = (state: SendState) => setLastSend({ key: sendKey, state });

    const ready = engine.status === 'ready' ? engine.engine : null;
    const chooseEmotion = (name: string) => {
        setEmotion(name);
        ready?.setEmotion(name);
    };

    const send = async () => {
        setSending({ status: 'sending' });
        try {
            await onSend(look);
            setSending({ status: 'sent' });
        } catch (err) {
            setSending({ status: 'error', message: err instanceof Error ? err.message : String(err) });
        }
    };

    return (
        <section className="stage" aria-label="Look choisi">
            <div className="case">
                <div className="glass">
                    {ready && <Screen engine={ready} look={look} />}
                    {engine.status === 'loading' && <p className="screen-note">Réveil des yeux…</p>}
                    {engine.status === 'error' && (
                        <p className="screen-note">
                            Moteur des yeux introuvable. Lance « mise run engine:wasm », puis recharge la page.
                        </p>
                    )}
                </div>
            </div>

            <div className="look-name">
                <h1>{swatch.id}</h1>
                <button
                    type="button"
                    className="favorite"
                    aria-pressed={favorite}
                    onClick={onFavorite}
                    title={favorite ? 'Retirer des favoris' : 'Ajouter aux favoris'}
                >
                    <svg viewBox="0 0 24 24" aria-hidden="true">
                        <path d="M12 20.5 4.4 13a4.6 4.6 0 0 1 6.5-6.5L12 7.6l1.1-1.1a4.6 4.6 0 0 1 6.5 6.5Z" />
                    </svg>
                    <span className="visually-hidden">Favori</span>
                </button>
            </div>

            <fieldset className="pupils">
                <legend>Pupille</legend>
                {PUPIL_TYPES.map((type) => (
                    <label key={type} className="pupil">
                        <input
                            type="radio"
                            name="pupil"
                            value={type}
                            checked={type === pupil}
                            onChange={() => onPupil(type)}
                        />
                        <EyeGlyph swatch={swatch} pupil={type} />
                        <span>{PUPIL_LABELS[type]}</span>
                    </label>
                ))}
            </fieldset>

            {ready && (
                <div className="moods" role="group" aria-label="Humeur de l'aperçu">
                    {ready.emotions.map((name) => (
                        <button
                            key={name}
                            type="button"
                            aria-pressed={name === emotion}
                            onClick={() => chooseEmotion(name)}
                        >
                            {EMOTION_LABELS[name] ?? name}
                        </button>
                    ))}
                    <button type="button" className="blink" onClick={() => ready.blink()}>
                        Cligner
                    </button>
                </div>
            )}

            <div className="send">
                <button
                    type="button"
                    className="primary"
                    disabled={device.status !== 'connected' || sending.status === 'sending'}
                    onClick={send}
                >
                    {sending.status === 'sending' ? 'Envoi…' : 'Envoyer à lilpet'}
                </button>
                <p className="send-note" role="status">
                    {sendNote(device, sending, swatch.id)}
                </p>
            </div>
        </section>
    );
}

function sendNote(device: DeviceState, sending: SendState, name: string): string {
    if (sending.status === 'sent') return `${name} est sur lilpet, et y reste même éteint.`;
    if (sending.status === 'error') return `Pas envoyé : ${sending.message}`;
    if (device.status === 'unsupported') return 'Pour envoyer un look, ouvre l’app dans Chrome ou Edge.';
    if (device.status !== 'connected') return 'Branche lilpet en USB pour lui envoyer ce look.';
    return '';
}
