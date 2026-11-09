// PNG files of RGB images, 8 bits per channel, each row filtered with "Sub" (enough for flat eye colors on black), for
// the reference renders of scripts/goldens.mjs: written and read back with node:zlib only, no dependency. Only files
// like those encodePng writes are read (8-bit RGB, not interlaced, rows filtered with None or Sub, all there): anything
// else is refused rather than misread.
import { crc32, deflateSync, inflateSync } from 'node:zlib';

const PNG_SIGNATURE = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]);

function chunk(type, data) {
    const head = Buffer.alloc(8);
    head.writeUInt32BE(data.length, 0);
    head.write(type, 4, 'latin1');
    const crc = Buffer.alloc(4);
    crc.writeUInt32BE(crc32(Buffer.concat([head.subarray(4), data])), 0);
    return Buffer.concat([head, data, crc]);
}

/** A PNG of `rgb` (width × height × 3 bytes). */
export function encodePng(width, height, rgb) {
    const stride = width * 3;
    const raw = Buffer.alloc((stride + 1) * height);
    for (let y = 0; y < height; y++) {
        const row = y * (stride + 1);
        raw[row] = 1; // Sub: each byte minus the same byte of the pixel to its left
        for (let i = 0; i < stride; i++)
            raw[row + 1 + i] = (rgb[y * stride + i] - (i >= 3 ? rgb[y * stride + i - 3] : 0)) & 0xff;
    }
    const header = Buffer.alloc(13);
    header.writeUInt32BE(width, 0);
    header.writeUInt32BE(height, 4);
    header.set([8, 2, 0, 0, 0], 8); // 8 bits, RGB, deflate, adaptive filters, no interlace
    return Buffer.concat([
        PNG_SIGNATURE,
        chunk('IHDR', header),
        chunk('IDAT', deflateSync(raw, { level: 9 })),
        chunk('IEND', Buffer.alloc(0))
    ]);
}

/** The pixels of a PNG written by encodePng: { width, height, rgb }. */
export function decodePng(file) {
    if (!file.subarray(0, 8).equals(PNG_SIGNATURE)) throw new Error('not a PNG file');
    let width = 0;
    let height = 0;
    const data = [];
    for (let at = 8; at < file.length;) {
        const length = file.readUInt32BE(at);
        const type = file.toString('latin1', at + 4, at + 8);
        const body = file.subarray(at + 8, at + 8 + length);
        if (type === 'IHDR') {
            width = body.readUInt32BE(0);
            height = body.readUInt32BE(4);
            if (body[8] !== 8 || body[9] !== 2 || body[10] !== 0 || body[11] !== 0 || body[12] !== 0)
                throw new Error('only 8-bit RGB, deflate, standard filters, not interlaced');
        } else if (type === 'IDAT') data.push(body);
        at += 12 + length;
    }
    const raw = inflateSync(Buffer.concat(data));
    const stride = width * 3;
    if (raw.length !== (stride + 1) * height) throw new Error(`${raw.length} bytes of rows for ${width} × ${height}`);
    const rgb = Buffer.alloc(stride * height);
    for (let y = 0; y < height; y++) {
        const filter = raw[y * (stride + 1)];
        if (filter > 1) throw new Error(`row ${y}: filter ${filter}, only None and Sub are read`);
        for (let i = 0; i < stride; i++) {
            const left = filter === 1 && i >= 3 ? rgb[y * stride + i - 3] : 0;
            rgb[y * stride + i] = (raw[y * (stride + 1) + 1 + i] + left) & 0xff;
        }
    }
    return { width, height, rgb };
}

/** The RGB bytes of RGBA pixels. */
export function rgbOf(rgba) {
    const rgb = Buffer.alloc((rgba.length / 4) * 3);
    for (let p = 0, q = 0; p < rgba.length; p += 4, q += 3) rgb.set(rgba.subarray(p, p + 3), q);
    return rgb;
}

/** How many pixels of two RGB images of the same size differ. */
export function differingPixels(a, b) {
    let count = 0;
    for (let i = 0; i < a.length; i += 3) if (a[i] !== b[i] || a[i + 1] !== b[i + 1] || a[i + 2] !== b[i + 2]) count++;
    return count;
}
