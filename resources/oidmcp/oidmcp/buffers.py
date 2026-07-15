"""Decode raw endpoint payloads into numpy arrays."""

from __future__ import annotations

from collections import OrderedDict

import numpy as np

# Keep in sync with oidscripts/symbols.py (OID_TYPES_*). Value 1 is
# historically unused.
DTYPES = {
    0: np.uint8,
    2: np.uint16,
    3: np.int16,
    4: np.int32,
    5: np.float32,
    6: np.float64,
}


def decode_buffer(meta: dict, raw: bytes) -> np.ndarray:
    """
    Return an (H, W, C) array in display orientation; channel order is
    the storage order (pixel_layout is applied only when rendering).

    Metadata semantics (matching the debugger bridges): width/height/
    row_stride describe the buffer as stored; row_stride counts pixels,
    including padding; transpose_buffer means the stored rows run along
    the display's width axis.
    """
    if meta['type'] not in DTYPES:
        raise ValueError(f"unsupported buffer type {meta['type']}")
    dtype = np.dtype(DTYPES[meta['type']])
    height, width = int(meta['height']), int(meta['width'])
    channels = int(meta['channels'])
    stride = int(meta['row_stride'])
    if height <= 0 or width <= 0 or channels <= 0:
        raise ValueError(
            f'invalid buffer dimensions: {width}x{height}x{channels}')
    if stride < width:
        raise ValueError(
            f'row_stride {stride} is smaller than width {width}')
    count = height * stride * channels
    if len(raw) < count * dtype.itemsize:
        raise ValueError(
            f'buffer payload is {len(raw)} bytes; expected at least '
            f'{count * dtype.itemsize}')
    arr = np.frombuffer(raw, dtype=dtype, count=count)
    arr = arr.reshape(height, stride, channels)[:, :width, :]
    if meta.get('transpose_buffer'):
        arr = arr.transpose(1, 0, 2)
    return arr


def crop_region(arr: np.ndarray,
                region: tuple[int, int, int, int]) -> np.ndarray:
    """
    Crop (x, y, w, h), in display coordinates (x = column, y = row).
    """
    if len(region) != 4:
        raise ValueError('region must be [x, y, w, h]')
    x, y, w, h = (int(v) for v in region)
    rows, cols = arr.shape[:2]
    if w <= 0 or h <= 0 or x < 0 or y < 0 or x + w > cols or y + h > rows:
        raise ValueError(
            f'region (x={x}, y={y}, w={w}, h={h}) is outside the '
            f'{cols}x{rows} buffer')
    return arr[y:y + h, x:x + w, :]


# (session pid, symbol, stop generation) -> (metadata, decoded array)
CacheKey = tuple[int, str, int]
CacheValue = tuple[dict, np.ndarray]


class BufferCache:
    """Small LRU keyed by (session pid, symbol, stop generation)."""

    def __init__(self, capacity: int = 4):
        self._capacity = capacity
        self._entries: OrderedDict[CacheKey, CacheValue] = OrderedDict()

    def get(self, key: CacheKey) -> CacheValue | None:
        if key not in self._entries:
            return None
        self._entries.move_to_end(key)
        return self._entries[key]

    def put(self, key: CacheKey, value: CacheValue) -> None:
        self._entries[key] = value
        self._entries.move_to_end(key)
        while len(self._entries) > self._capacity:
            self._entries.popitem(last=False)
