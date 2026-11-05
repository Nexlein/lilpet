// Left column: the device with the eyes, the chosen look and what can be done with it
import { useMemo, useState } from 'react';
import { PUPIL_TYPES, makeLook, type Look, type PupilType, type Swatch } from '../data/looks';
import type { LookSent } from '../device/device';
import type { DeviceState } from '../device/session';
import type { EngineState } from './hooks';
import { EyeGlyph } from './EyeGlyph';
import { Screen } from './Screen';
import { SendPanel } from './SendPanel';
import { EMOTION_LABELS, PUPIL_LABELS } from './labels';

interface Props {
    engine: EngineState;
    swatch: Swatch;
    pupil: PupilType;
    favorite: boolean;
    device: DeviceState;
    onPupil(pupil: PupilType): void;
    onFavorite(): void;
    onSend(look: Look): Promise<LookSent>;
    onEmotion(name: string): void;
    onPet(): void;
}

export function Stage({
    engine,
    swatch,
    pupil,
    favorite,
    device,
    onPupil,
    onFavorite,
    onSend,
    onEmotion,
    onPet
}: Props) {
    const look = useMemo(() => makeLook(swatch, pupil), [swatch, pupil]);
    const [emotion, setEmotion] = useState('neutral');

    const ready = engine.status === 'ready' ? engine.engine : null;
    const chooseEmotion = (name: string) => {
        setEmotion(name);
        ready?.setEmotion(name);
        onEmotion(name);
    };

    // Happy for a moment, then back to the emotion picked below
    const pet = () => {
        ready?.react('tap');
        onPet();
    };

    return (
        <section className="stage" aria-label="Look choisi">
            <div className="case">
                <div className="glass">
                    {ready && <Screen engine={ready} look={look} emotion={emotion} onPet={pet} />}
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
                <div className="moods" role="group" aria-label="Humeur de lilpet">
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
                    <div className="gestures">
                        <button type="button" onClick={() => ready.blink()}>
                            Cligner
                        </button>
                        <button type="button" onClick={pet}>
                            Caresser
                        </button>
                    </div>
                </div>
            )}

            <SendPanel swatch={swatch} pupil={pupil} device={device} onSend={onSend} />
        </section>
    );
}
