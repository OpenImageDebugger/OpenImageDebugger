import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import test from 'node:test';

import { genBuffers } from '../demo-buffers.js';

const GOLDEN = {
  sample_buffer_1: {
    length: 240_000,
    sha256: '68dbc6ece27b168902a4e77933b497294d921e38aee5c85e88f2adeef87c00de',
  },
  sample_buffer_2: {
    length: 320_000,
    sha256: '56e8ad48b022c6856679905abf322f35f615439024670bcc53813a1a0f61478b',
  },
};

function sha256(bytes) {
  return createHash('sha256').update(bytes).digest('hex');
}

test('genBuffers matches resources/oidscripts/test.py at 400x200', () => {
  const buffers = genBuffers(400, 200);

  for (const name of ['sample_buffer_1', 'sample_buffer_2']) {
    const buffer = buffers[name];
    const golden = GOLDEN[name];

    assert.equal(buffer.pixels.length, golden.length, `${name} byte length`);
    assert.equal(sha256(buffer.pixels), golden.sha256, `${name} sha256`);
  }
});

test('genBuffers metadata matches test.py DummyDebugger defaults', () => {
  const buffers = genBuffers(400, 200);

  assert.equal(buffers.sample_buffer_1.variableName, 'sample_buffer_1');
  assert.equal(buffers.sample_buffer_1.displayName, 'uint8* sample_buffer_1');
  assert.equal(buffers.sample_buffer_1.channels, 3);
  assert.equal(buffers.sample_buffer_1.bufferType, 0);
  assert.equal(buffers.sample_buffer_1.stride, 400);
  assert.equal(buffers.sample_buffer_1.pixelLayout, 'rgba');
  assert.equal(buffers.sample_buffer_1.transpose, false);

  assert.equal(buffers.sample_buffer_2.variableName, 'sample_buffer_2');
  assert.equal(buffers.sample_buffer_2.displayName, 'float* sample_buffer_2');
  assert.equal(buffers.sample_buffer_2.channels, 1);
  assert.equal(buffers.sample_buffer_2.bufferType, 5);
  assert.equal(buffers.sample_buffer_2.stride, 400);
});
