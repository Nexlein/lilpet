// Favorite looks and the last chosen look, kept in the browser (localStorage)
import { findSwatch, isPupilType, type PupilType, type Swatch } from './looks';

// The browser's localStorage throws when it is blocked or full: the app then goes on, and forgets on reload
function setItem(key: string, value: string): void {
    try {
        localStorage.setItem(key, value);
    } catch (err) {
        console.warn('Cannot save the choice in the browser', err);
    }
}

export interface LookChoice {
    swatch: Swatch;
    pupil: PupilType;
}

const FAVORITES = 'lilpet.favorites';
const CURRENT = 'lilpet.look';

// "cherry/dot"
export const lookKey = (swatchId: string, pupil: PupilType): string => `${swatchId}/${pupil}`;

export function parseLookKey(key: string): LookChoice | null {
    const [id = '', pupil = '', ...extra] = key.split('/');
    const swatch = findSwatch(id);
    return swatch && isPupilType(pupil) && extra.length === 0 ? { swatch, pupil } : null;
}

// What is stored under `key`, or null when it is missing, malformed or the storage is blocked
function readJson(key: string): unknown {
    try {
        return JSON.parse(localStorage.getItem(key) ?? 'null');
    } catch {
        return null;
    }
}

// Unknown or malformed entries are dropped, so an old or edited value never breaks the app
export function loadFavorites(): string[] {
    const value = readJson(FAVORITES);
    if (!Array.isArray(value)) return [];
    return [...new Set(value.filter((k): k is string => typeof k === 'string' && parseLookKey(k) !== null))];
}

export function saveFavorites(keys: readonly string[]): void {
    setItem(FAVORITES, JSON.stringify(keys));
}

export function loadCurrent(): LookChoice | null {
    const value = readJson(CURRENT);
    return typeof value === 'string' ? parseLookKey(value) : null;
}

export function saveCurrent(choice: LookChoice): void {
    setItem(CURRENT, JSON.stringify(lookKey(choice.swatch.id, choice.pupil)));
}
