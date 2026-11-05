// Right column: the 100 official colors, as small pairs of eyes wearing the chosen pupil
import { useMemo, useState } from 'react';
import { FAMILIES, filterSwatches, type Family, type PupilType, type Swatch } from '../data/looks';
import { lookKey, parseLookKey } from '../data/favorites';
import { EyeGlyph } from './EyeGlyph';
import { FAMILY_COLORS, FAMILY_LABELS, PUPIL_LABELS } from './labels';

interface Props {
    swatch: Swatch;
    pupil: PupilType;
    favorites: readonly string[];
    onPick(swatch: Swatch, pupil?: PupilType): void;
}

export function Gallery({ swatch, pupil, favorites, onPick }: Props) {
    const [query, setQuery] = useState('');
    const [families, setFamilies] = useState<ReadonlySet<Family>>(new Set());
    const [twoColors, setTwoColors] = useState(false);

    const shown = useMemo(() => filterSwatches({ query, families, twoColors }), [query, families, twoColors]);

    const toggleFamily = (f: Family) => {
        const next = new Set(families);
        if (!next.delete(f)) next.add(f);
        setFamilies(next);
    };

    const favoriteLooks = favorites.map(parseLookKey).filter((c) => c !== null);

    return (
        <section className="gallery" aria-label="Couleurs">
            {favoriteLooks.length > 0 && (
                <div className="favorites">
                    <h2>Tes favoris</h2>
                    <ul>
                        {favoriteLooks.map((c) => (
                            <li key={lookKey(c.swatch.id, c.pupil)}>
                                <button
                                    type="button"
                                    className="chip"
                                    aria-current={c.swatch === swatch && c.pupil === pupil}
                                    onClick={() => onPick(c.swatch, c.pupil)}
                                >
                                    <EyeGlyph swatch={c.swatch} pupil={c.pupil} />
                                    <span>
                                        {c.swatch.id} <small>{PUPIL_LABELS[c.pupil].toLowerCase()}</small>
                                    </span>
                                </button>
                            </li>
                        ))}
                    </ul>
                </div>
            )}

            <div className="filters">
                <h2>Les 100 couleurs</h2>
                <input
                    type="search"
                    placeholder="Chercher un nom"
                    aria-label="Chercher une couleur par son nom"
                    value={query}
                    onChange={(e) => setQuery(e.target.value)}
                />
                <div className="families" role="group" aria-label="Filtrer par teinte">
                    {FAMILIES.map((f) => (
                        <button
                            key={f}
                            type="button"
                            aria-pressed={families.has(f)}
                            onClick={() => toggleFamily(f)}
                            title={FAMILY_LABELS[f]}
                        >
                            <span className="dot" style={{ background: FAMILY_COLORS[f] }} />
                            <span className="visually-hidden">{FAMILY_LABELS[f]}</span>
                        </button>
                    ))}
                    <button type="button" aria-pressed={twoColors} onClick={() => setTwoColors(!twoColors)}>
                        Deux pupilles différentes
                    </button>
                </div>
            </div>

            {shown.length === 0 ? (
                <p className="empty">Aucune couleur ne correspond. Retire un filtre ou change ta recherche.</p>
            ) : (
                <ul className="grid">
                    {shown.map((s) => (
                        <li key={s.id}>
                            <button
                                type="button"
                                className="chip"
                                aria-current={s === swatch}
                                onClick={() => onPick(s)}
                            >
                                <EyeGlyph swatch={s} pupil={pupil} />
                                <span>{s.id}</span>
                            </button>
                        </li>
                    ))}
                </ul>
            )}
        </section>
    );
}
