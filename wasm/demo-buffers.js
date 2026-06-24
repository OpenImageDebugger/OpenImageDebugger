/** @typedef {'sample_buffer_1' | 'sample_buffer_2'} SampleBufferName */

/**
 * @typedef {object} SampleBuffer
 * @property {SampleBufferName} variableName
 * @property {string} displayName
 * @property {string} pixelLayout
 * @property {boolean} transpose
 * @property {number} width
 * @property {number} height
 * @property {number} channels
 * @property {number} stride
 * @property {number} bufferType
 * @property {Uint8Array} pixels
 */

const OID_TYPES_UINT8 = 0;
const OID_TYPES_FLOAT32 = 5;

/**
 * @param {[number, number]} pos
 * @param {[number, number]} k
 * @param {(n: number) => number} fA
 * @param {(n: number) => number} fB
 */
function genColor(pos, k, fA, fB) {
  const p0 = pos[0];
  const p1 = pos[1];
  return ((fA((p0 * fB(p1 / k[0])) / k[1]) + 1.0) * 255.0) / 2.0;
}

/**
 * Port of resources/oidscripts/test.py::_gen_buffers.
 * @param {number} width
 * @param {number} height
 * @returns {Record<SampleBufferName, SampleBuffer>}
 */
export function genBuffers(width, height) {
  const channels = [3, 1];
  const tex1 = new Uint8Array(width * height * channels[0]);
  const tex2 = new Float32Array(width * height * channels[1]);

  const cX = (width * 2.0) / 3.0;
  const cY = height * 0.5;
  const scale = 3.0 / width;

  for (let posY = 0; posY < height; posY += 1) {
    for (let posX = 0; posX < width; posX += 1) {
      const pixelPos = [posX, posY];
      const bufferPos1 = posY * channels[0] * width + channels[0] * posX;

      tex1[bufferPos1 + 0] = genColor(pixelPos, [20.0, 80.0], Math.cos, Math.cos);
      tex1[bufferPos1 + 1] = genColor(pixelPos, [50.0, 200.0], Math.sin, Math.cos);
      tex1[bufferPos1 + 2] = genColor(pixelPos, [30.0, 120.0], Math.cos, Math.cos);

      const re = (posX - cX) * scale;
      const im = (posY - cY) * scale;
      const bufferPos2 = posY * channels[1] * width + channels[1] * posX;

      let zRe = 0;
      let zIm = 0;
      for (let i = 0; i < 20; i += 1) {
        const nextRe = zRe * zRe - zIm * zIm + re;
        const nextIm = 2 * zRe * zIm + im;
        zRe = nextRe;
        zIm = nextIm;
      }

      const zNormSquared = zRe * zRe + zIm * zIm;
      const zThreshold = 5.0;
      const capped =
        zNormSquared < zThreshold && !Number.isNaN(zNormSquared)
          ? zNormSquared
          : zThreshold;
      const value = zThreshold - capped;

      for (let channel = 0; channel < channels[1]; channel += 1) {
        tex2[bufferPos2 + channel] = value;
      }
    }
  }

  const tex1Bytes = new Uint8Array(tex1.buffer, tex1.byteOffset, tex1.byteLength);
  const tex2Bytes = new Uint8Array(tex2.buffer, tex2.byteOffset, tex2.byteLength);
  const rowStride = width;

  return {
    sample_buffer_1: {
      variableName: 'sample_buffer_1',
      displayName: 'uint8* sample_buffer_1',
      pixelLayout: 'rgba',
      transpose: false,
      width,
      height,
      channels: channels[0],
      stride: rowStride,
      bufferType: OID_TYPES_UINT8,
      pixels: tex1Bytes,
    },
    sample_buffer_2: {
      variableName: 'sample_buffer_2',
      displayName: 'float* sample_buffer_2',
      pixelLayout: 'rgba',
      transpose: false,
      width,
      height,
      channels: channels[1],
      stride: rowStride,
      bufferType: OID_TYPES_FLOAT32,
      pixels: tex2Bytes,
    },
  };
}
