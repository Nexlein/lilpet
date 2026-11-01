// lilpet PC app: pick the eyes, see them live, send them to the device
import type { CSSProperties } from 'react';
import { findSwatch, hsl, type PupilType, type Swatch } from '../data/looks';
import { loadCurrent, loadFavorites, lookKey, saveCurrent, saveFavorites, type LookChoice } from '../data/favorites';
import { DeviceBar } from './DeviceBar';
import { Gallery } from './Gallery';
import { Stage } from './Stage';
import { useDevice, useEngine, useStored } from './hooks';

const DEFAULT: LookChoice = { swatch: findSwatch('cherry')!, pupil: 'dot' };

// The interface takes the colors of the chosen eyes
function theme(swatch: Swatch): CSSProperties {
    const [, , l] = hsl(swatch.sclera);
    return {
        '--eye': swatch.sclera,
        '--pupil': swatch.pupilColor[0],
        '--on-eye': l > 0.62 ? '#1A1430' : '#FFFFFF'
    } as CSSProperties;
}

export function App() {
    const engine = useEngine();
    const device = useDevice();
    const [choice, setChoice] = useStored((s) => loadCurrent(s) ?? DEFAULT, saveCurrent);
    const [favorites, setFavorites] = useStored(loadFavorites, saveFavorites);
    const { swatch, pupil } = choice;
    const key = lookKey(swatch.id, pupil);

    const toggleFavorite = () =>
        setFavorites(favorites.includes(key) ? favorites.filter((k) => k !== key) : [...favorites, key]);

    return (
        <div className="app" style={theme(swatch)}>
            <header className="top">
                <p className="wordmark">lilpet</p>
                <DeviceBar state={device.state} onConnect={device.connect} onDisconnect={device.disconnect} />
            </header>

            <main className="layout">
                <Stage
                    engine={engine}
                    swatch={swatch}
                    pupil={pupil}
                    favorite={favorites.includes(key)}
                    device={device.state}
                    onPupil={(p: PupilType) => setChoice({ swatch, pupil: p })}
                    onFavorite={toggleFavorite}
                    onSend={device.send}
                />
                <Gallery
                    swatch={swatch}
                    pupil={pupil}
                    favorites={favorites}
                    onPick={(s, p) => setChoice({ swatch: s, pupil: p ?? pupil })}
                />
            </main>

            {(device.state.status === 'connected' || device.log.length > 0) && (
                <details className="log">
                    <summary>Journal de lilpet</summary>
                    <pre aria-live="off">{device.log.length ? device.log.join('\n') : 'Rien pour l’instant.'}</pre>
                </details>
            )}
        </div>
    );
}
