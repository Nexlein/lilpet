// Tests of scripts/png.mjs, the PNG files of the reference renders: what is written reads back pixel for pixel, a
// difference is counted per pixel, and a file it did not write is refused rather than misread. Built-in test runner,
// no dependency. The renders themselves are compared by mise run engine:wasm (scripts/goldens.mjs).
// Usage: npm run check:scripts
import assert from 'node:assert/strict';
import { test } from 'node:test';
import { deflateSync } from 'node:zlib';
import { decodePng, differingPixels, encodePng, rgbOf } from './png.mjs';

test('reads back every pixel it writes', () => {
    const [width, height] = [7, 5];
    const rgb = Buffer.alloc(width * height * 3);
    for (let i = 0; i < rgb.length; i++) rgb[i] = (i * 37 + (i >> 2) * 101) & 0xff; // every value, wrapping the Sub filter
    const image = decodePng(encodePng(width, height, rgb));
    assert.equal(image.width, width);
    assert.equal(image.height, height);
    assert.ok(image.rgb.equals(rgb));
});

test('keeps RGB from RGBA and counts the pixels that differ', () => {
    const rgba = Uint8ClampedArray.from([1, 2, 3, 255, 4, 5, 6, 0]);
    assert.deepEqual([...rgbOf(rgba)], [1, 2, 3, 4, 5, 6]);
    const a = Buffer.from([0, 0, 0, 9, 9, 9, 1, 2, 3]);
    const b = Buffer.from([0, 0, 0, 9, 9, 8, 1, 2, 4]);
    assert.equal(differingPixels(a, a), 0);
    assert.equal(differingPixels(a, b), 2);
});

test('refuses what it did not write', () => {
    assert.throws(() => decodePng(Buffer.from('not a png at all')), /not a PNG file/);
    // A valid PNG with the Paeth filter (4) on its only row: not one of the filters it writes
    const png = encodePng(1, 1, Buffer.from([10, 20, 30]));
    const idat = png.indexOf('IDAT');
    const forged = Buffer.concat([
        png.subarray(0, idat - 4),
        (() => {
            const data = deflateSync(Buffer.from([4, 10, 20, 30]));
            const head = Buffer.alloc(4);
            head.writeUInt32BE(data.length, 0);
            return Buffer.concat([head, Buffer.from('IDAT'), data, Buffer.alloc(4)]);
        })(),
        png.subarray(png.indexOf('IEND') - 4)
    ]);
    assert.throws(() => decodePng(forged), /filter 4/);
});
