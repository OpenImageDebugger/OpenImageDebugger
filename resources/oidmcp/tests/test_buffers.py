import numpy as np
import pytest

from oidmcp.buffers import BufferCache, crop_region, decode_buffer
from conftest import make_meta


def test_decode_uint8_rgb():
    arr = np.arange(4 * 3 * 3, dtype=np.uint8).reshape(3, 4, 3)
    meta = make_meta(4, 3, channels=3, type_value=0, raw=arr.tobytes())
    decoded = decode_buffer(meta, arr.tobytes())
    assert decoded.shape == (3, 4, 3)
    assert decoded.dtype == np.uint8
    np.testing.assert_array_equal(decoded, arr)


def test_decode_strips_row_padding():
    # 4 visible pixels per row, stride 6 pixels, 2 rows, 1 channel
    padded = np.arange(2 * 6, dtype=np.float32).reshape(2, 6, 1)
    meta = make_meta(4, 2, channels=1, type_value=5, row_stride=6,
                     raw=padded.tobytes())
    decoded = decode_buffer(meta, padded.tobytes())
    assert decoded.shape == (2, 4, 1)
    np.testing.assert_array_equal(decoded, padded[:, :4, :])


def test_decode_transposed_matches_display_orientation():
    # An 8-row x 16-col col-major matrix: metadata carries the stored
    # layout (width=8, height=16, stride=8) and transpose_buffer=True.
    matrix = (np.arange(8)[:, None] * 100 + np.arange(16)[None, :]) \
        .astype(np.float32)                      # display: (8, 16)
    stored = matrix.T.copy()                     # stored:  (16, 8)
    meta = make_meta(8, 16, channels=1, type_value=5,
                     transpose_buffer=True, raw=stored.tobytes())
    decoded = decode_buffer(meta, stored.tobytes())
    assert decoded.shape == (8, 16, 1)
    np.testing.assert_array_equal(decoded[:, :, 0], matrix)


@pytest.mark.parametrize('type_value,dtype', [
    (0, np.uint8), (2, np.uint16), (3, np.int16),
    (4, np.int32), (5, np.float32), (6, np.float64),
])
def test_decode_all_dtypes(type_value, dtype):
    arr = np.ones((2, 2, 1), dtype=dtype)
    meta = make_meta(2, 2, channels=1, type_value=type_value,
                     raw=arr.tobytes())
    assert decode_buffer(meta, arr.tobytes()).dtype == dtype


def test_decode_rejects_short_payload():
    meta = make_meta(4, 4, channels=1, type_value=5)
    with pytest.raises(ValueError):
        decode_buffer(meta, b'\x00' * 8)


def test_crop_region():
    arr = np.arange(6 * 4, dtype=np.int32).reshape(4, 6, 1)
    cropped = crop_region(arr, (1, 2, 3, 2))
    np.testing.assert_array_equal(cropped, arr[2:4, 1:4, :])
    for bad in [(-1, 0, 2, 2), (0, 0, 7, 1), (5, 3, 2, 2), (0, 0, 0, 1)]:
        with pytest.raises(ValueError):
            crop_region(arr, bad)


def test_crop_region_rejects_wrong_length_region():
    arr = np.zeros((4, 6, 1), dtype=np.int32)
    with pytest.raises(ValueError) as excinfo:
        crop_region(arr, (1, 2, 3))
    assert 'region must be [x, y, w, h]' in str(excinfo.value)


def test_cache_is_lru_with_capacity():
    cache = BufferCache(capacity=2)
    cache.put('a', 1)
    cache.put('b', 2)
    assert cache.get('a') == 1      # refreshes 'a'
    cache.put('c', 3)               # evicts 'b'
    assert cache.get('b') is None
    assert cache.get('a') == 1
    assert cache.get('c') == 3
