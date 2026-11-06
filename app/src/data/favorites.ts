// Favorite looks and the last chosen look, kept in the browser (localStorage)
import { findSwatch, isPupilType, type PupilType, type Swatch } from './looks';

export interface KeyValueStore {
    getItem(key: string): string | null;
    setItem(key: string, value: string): void;
}

// The browser's localStorage, which throws when it is blocked or full: the app then goes on, and forgets on reload
export const browserStore: KeyValueStore = {
    getItem(key) {
        try {
            return localStorage.getItem(key);
        } catch {
            return null;
        }
    },
    setItem(key, value) {
        try {
            localStorage.setItem(key, value);
        } catch (err) {
            console.warn('Cannot save the choice in the browser', err);
        }
    }
};

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

function readJson(store: KeyValueStore, key: string): unknown {
    try {
        return JSON.parse(store.getItem(key) ?? 'null');
    } catch {
        return null;
    }
}

// Unknown or malformed entries are dropped, so an old or edited value never breaks the app
export function loadFavorites(store: KeyValueStore): string[] {
    const value = readJson(store, FAVORITES);
    if (!Array.isArray(value)) return [];
    return [...new Set(value.filter((k): k is string => typeof k === 'string' && parseLookKey(k) !== null))];
}

export function saveFavorites(store: KeyValueStore, keys: readonly string[]): void {
    store.setItem(FAVORITES, JSON.stringify(keys));
}

export function loadCurrent(store: KeyValueStore): LookChoice | null {
    const value = readJson(store, CURRENT);
    return typeof value === 'string' ? parseLookKey(value) : null;
}

export function saveCurrent(store: KeyValueStore, choice: LookChoice): void {
    store.setItem(CURRENT, JSON.stringify(lookKey(choice.swatch.id, choice.pupil)));
}
