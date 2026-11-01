import { describe, expect, it } from 'vitest';
import { FAMILIES, SWATCHES, family, findSwatch, hasTwoPupilColors, hsl, makeLook } from './looks';
import { loadCurrent, loadFavorites, lookKey, parseLookKey, saveCurrent, saveFavorites } from './favorites';

// In-memory stand-in for localStorage
function memory() {
    const items = new Map<string, string>();
    return { getItem: (k: string) => items.get(k) ?? null, setItem: (k: string, v: string) => items.set(k, v) };
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

    it('remembers the current look', () => {
        const store = memory();
        expect(loadCurrent(store)).toBeNull();
        saveCurrent(store, parseLookKey('kite/circle')!);
        expect(loadCurrent(store)?.swatch.id).toBe('kite');
        expect(loadCurrent(store)?.pupil).toBe('circle');
    });
});
