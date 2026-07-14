// Generate a minimal .ico file for the Cnext IDE
const fs = require('fs');
const path = require('path');

// Create a simple 32x32 RGBA PNG manually
function createPNG(size) {
  // PNG file signature
  const signature = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);

  // IHDR chunk
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(size, 0); // width
  ihdr.writeUInt32BE(size, 4); // height
  ihdr[8] = 8;  // bit depth
  ihdr[9] = 2;  // color type (RGB)
  ihdr[10] = 0; // compression
  ihdr[11] = 0; // filter
  ihdr[12] = 0; // interlace

  const ihdrChunk = makeChunk('IHDR', ihdr);

  // IDAT chunk - raw image data
  const rawData = [];
  for (let y = 0; y < size; y++) {
    rawData.push(0); // filter byte
    for (let x = 0; x < size; x++) {
      // Simple "C" letter pattern on blue background
      const cx = x / size;
      const cy = y / size;
      const inCircle = Math.sqrt((cx - 0.5) ** 2 + (cy - 0.5) ** 2) < 0.42;
      const inInner = Math.sqrt((cx - 0.5) ** 2 + (cy - 0.5) ** 2) < 0.28;
      const inGap = cx > 0.55 && cy > 0.3 && cy < 0.7;

      if (inCircle && !inInner && !inGap) {
        // White letter
        rawData.push(255, 255, 255);
      } else if (inCircle) {
        // Blue circle background
        rawData.push(0, 122, 204);
      } else {
        // Transparent-looking dark background
        rawData.push(30, 30, 30);
      }
    }
  }

  const zlib = require('zlib');
  const compressed = zlib.deflateSync(Buffer.from(rawData));
  const idatChunk = makeChunk('IDAT', compressed);

  // IEND chunk
  const iendChunk = makeChunk('IEND', Buffer.alloc(0));

  return Buffer.concat([signature, ihdrChunk, idatChunk, iendChunk]);
}

function makeChunk(type, data) {
  const length = Buffer.alloc(4);
  length.writeUInt32BE(data.length, 0);
  const typeBuffer = Buffer.from(type, 'ascii');
  const crcData = Buffer.concat([typeBuffer, data]);
  const crc = crc32(crcData);
  const crcBuffer = Buffer.alloc(4);
  crcBuffer.writeUInt32BE(crc, 0);
  return Buffer.concat([length, typeBuffer, data, crcBuffer]);
}

function crc32(buf) {
  let crc = 0xFFFFFFFF;
  for (let i = 0; i < buf.length; i++) {
    crc ^= buf[i];
    for (let j = 0; j < 8; j++) {
      crc = (crc >>> 1) ^ (crc & 1 ? 0xEDB88320 : 0);
    }
  }
  return (crc ^ 0xFFFFFFFF) >>> 0;
}

function createICO(pngBuffer, sizes) {
  const numImages = sizes.length;
  // ICO header
  const header = Buffer.alloc(6);
  header.writeUInt16LE(0, 0);     // reserved
  header.writeUInt16LE(1, 2);     // type = ICO
  header.writeUInt16LE(numImages, 4); // image count

  const dirEntries = [];
  const imageData = [];
  let offset = 6 + numImages * 16;

  for (let i = 0; i < numImages; i++) {
    const entry = Buffer.alloc(16);
    const size = sizes[i];
    entry[0] = size === 256 ? 0 : size; // width (0 = 256)
    entry[1] = size === 256 ? 0 : size; // height
    entry[2] = 0;  // colors
    entry[3] = 0;  // reserved
    entry.writeUInt16LE(1, 4);     // planes
    entry.writeUInt16LE(32, 6);    // bits per pixel
    entry.writeUInt32LE(pngBuffer.length, 8);  // size
    entry.writeUInt32LE(offset, 12);           // offset
    dirEntries.push(entry);
    imageData.push(pngBuffer);
    offset += pngBuffer.length;
  }

  return Buffer.concat([header, ...dirEntries, ...imageData]);
}

// Generate a 256x256 PNG
const png = createPNG(256);

// Create ICO with single size
const ico = createICO(png, [256]);

// Write both files
fs.writeFileSync(path.join(__dirname, 'assets', 'icon.png'), png);
fs.writeFileSync(path.join(__dirname, 'assets', 'icon.ico'), ico);

console.log('Created assets/icon.png and assets/icon.ico');
