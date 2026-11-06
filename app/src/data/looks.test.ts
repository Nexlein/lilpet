import { afterEach, describe, expect, it, vi } from 'vitest';
import {
    FAMILIES,
    SWATCHES,
    contrast,
    family,
    filterSwatches,
    findSwatch,
    hasTwoPupilColors,
    hsl,
    inkOn,
    makeLook,
    type SwatchFilter
} from './looks';
import {
    browserStore,
    loadCurrent,
    loadFavorites,
    lookKey,
    parseLookKey,
    saveCurrent,
    saveFavorites
} from './favorites';

// In-memory stand-in for localStorage
function memory() {
    const items = new Map<string, string>();
    return { getItem: (k: string) => items.get(k) ?? null, setItem: (k: string, v: string) => items.set(k, v) };
}

const ids = (filter: SwatchFilter) => filterSwatches(filter).map((s) => s.id);

function refuse(): never {
    throw new DOMException('refused', 'SecurityError');
}

describe('swatches', () => {
    it('loads the 100 official swatches', () => {
        expect(SWATCHES).toHaveLength(100);
        expect(SWATCHES.filter(hasTwoPupilColors)).toHaveLength(14);
    });

    it('builds a look like makeLook in the prototype', () => {
        const cherry = findSwatch('cherry')!;
        expect(makeLook(cherry, 'cat')).toEqual({
            id: 'cherry',
            sclera: '#AE0002',
            pupil: 'cat',
            pupilColor: ['#FF5252', '#FF5252']
        });
    });

    it('converts colors to hue, saturation and lightness', () => {
        expect(hsl('#FF0000')).toEqual([0, 1, 0.5]);
        expect(hsl('#00FF00')[0]).toBe(120);
        expect(hsl('#808080')[1]).toBe(0);
    });

    it('puts every swatch in a color family, and every family has swatches', () => {
        expect(family(findSwatch('cherry')!)).toBe('red');
        expect(family(findSwatch('og')!)).toBe('neutral'); // white
        expect(family(findSwatch('pebble')!)).toBe('neutral'); // greyish
        const used = new Set(SWATCHES.map(family));
        expect([...used].toSorted()).toEqual(FAMILIES.toSorted());
    });
});

describe('ink on a color', () => {
    it('picks the ink that reads best on an eye color', () => {
        expect(contrast('#000000', '#FFFFFF')).toBeCloseTo(21);
        expect(inkOn('#F6FD21')).toBe('#000000'); // HSL lightness alone calls this yellow dark
        expect(inkOn('#AE0002')).toBe('#FFFFFF');
        const worst = Math.min(...SWATCHES.map((s) => contrast(s.sclera, inkOn(s.sclera))));
        expect(worst).toBeGreaterThan(4.5);
    });
});

describe('gallery filter', () => {
    const none = { query: '', families: new Set<never>(), twoColors: false };

    it('shows every swatch without a filter', () => {
        expect(filterSwatches(none)).toHaveLength(100);
    });

    it('searches names, whatever the case and the spaces around', () => {
        expect(ids({ ...none, query: '  CHERR ' })).toEqual(['cherry']);
        expect(ids({ ...none, query: 'no such color' })).toEqual([]);
    });

    it('keeps the chosen families, and the swatches with two pupil colors', () => {
        const reds = filterSwatches({ ...none, families: new Set(['red']) });
        expect(reds.length).toBeGreaterThan(0);
        expect(reds.every((s) => family(s) === 'red')).toBe(true);
        expect(filterSwatches({ ...none, twoColors: true })).toHaveLength(14);
        expect(ids({ ...none, query: 'cherry', twoColors: true })).toEqual([]);
    });
});

describe('favorites and current look', () => {
    it('round-trips favorites and drops unknown entries', () => {
        const store = memory();
        saveFavorites(store, [lookKey('cherry', 'dot'), lookKey('socks', 'acorn')]);
        expect(loadFavorites(store)).toEqual(['cherry/dot', 'socks/acorn']);
        store.setItem('lilpet.favorites', JSON.stringify(['cherry/dot', 'nope/dot', 'cherry/slit', 42]));
        expect(loadFavorites(store)).toEqual(['cherry/dot']);
        store.setItem('lilpet.favorites', '{broken');
        expect(loadFavorites(store)).toEqual([]);
    });

    it('drops favorites that could not be taken off the list', () => {
        const store = memory();
        store.setItem('lilpet.favorites', JSON.stringify(['cherry/dot/extra', 'cherry/cat', 'cherry/cat']));
        expect(loadFavorites(store)).toEqual(['cherry/cat']);
    });

    it('remembers the current look', () => {
        const store = memory();
        expect(loadCurrent(store)).toBeNull();
        saveCurrent(store, parseLookKey('kite/circle')!);
        expect(loadCurrent(store)?.swatch.id).toBe('kite');
        expect(loadCurrent(store)?.pupil).toBe('circle');
    });
});

describe('browser storage', () => {
    afterEach(() => vi.unstubAllGlobals());

    it('reads and writes localStorage', () => {
        vi.stubGlobal('localStorage', memory());
        browserStore.setItem('key', 'value');
        expect(browserStore.getItem('key')).toBe('value');
    });

    it('goes on when the browser blocks or fills the storage', () => {
        const warn = vi.spyOn(console, 'warn').mockImplementation(() => undefined);
        vi.stubGlobal('localStorage', { getItem: refuse, setItem: refuse });
        expect(browserStore.getItem('key')).toBeNull();
        expect(() => browserStore.setItem('key', 'value')).not.toThrow();
        expect(warn).toHaveBeenCalledOnce();
        warn.mockRestore();
    });
});
