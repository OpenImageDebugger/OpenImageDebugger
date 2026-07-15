import numpy as np
import pytest

from oidmcp.analysis import (
    VALUES_CAP,
    compute_stats,
    dump_npy,
    extract_values,
)
from conftest import make_meta


def float_meta(arr):
    return make_meta(arr.shape[1], arr.shape[0], channels=arr.shape[2],
                     type_value=5, raw=arr.astype(np.float32).tobytes())


def test_stats_per_channel_nan_aware():
    arr = np.zeros((2, 4, 2), dtype=np.float32)
    arr[:, :, 0] = [[1, 2, 3, 4], [5, 6, 7, 8]]
    arr[0, 0, 1] = np.nan
    arr[0, 1, 1] = np.inf
    stats = compute_stats(arr, float_meta(arr))
    first, second = stats['per_channel']
    assert first['min'] == pytest.approx(1.0)
    assert first['max'] == pytest.approx(8.0)
    assert first['mean'] == pytest.approx(4.5)
    assert first['nan'] == 0
    assert second['nan'] == 1 and second['inf'] == 1
    assert stats['dtype'] == 'float32'


def test_stats_all_nan_channel_reports_none():
    arr = np.full((2, 2, 1), np.nan, dtype=np.float32)
    stats = compute_stats(arr, float_meta(arr))
    channel = stats['per_channel'][0]
    assert channel['min'] is None and channel['mean'] is None
    assert channel['nan'] == 4


def test_stats_with_region():
    arr = np.zeros((4, 4, 1), dtype=np.float32)
    arr[2:, 2:, 0] = 9.0
    stats = compute_stats(arr, float_meta(arr), region=(2, 2, 2, 2))
    assert stats['per_channel'][0]['min'] == pytest.approx(9.0)
    assert stats['region'] == [2, 2, 2, 2]


def test_values_returns_exact_numbers():
    arr = (np.arange(12, dtype=np.float32) ** 2).reshape(3, 4, 1)
    result = extract_values(arr, x=1, y=1, w=2, h=2)
    assert result['values'] == [[[25.0], [36.0]], [[81.0], [100.0]]]
    assert result['dtype'] == 'float32'


def test_values_encodes_non_finite_as_strings():
    arr = np.array([[[np.nan], [np.inf]], [[-np.inf], [1.0]]],
                   dtype=np.float32)
    result = extract_values(arr, x=0, y=0, w=2, h=2)
    flat = [v[0] for row in result['values'] for v in row]
    assert flat == ['NaN', 'Inf', '-Inf', 1.0]


def test_values_channel_selection():
    arr = np.zeros((2, 2, 3), dtype=np.float32)
    arr[:, :, 1] = 7.0
    result = extract_values(arr, x=0, y=0, w=2, h=2, channel=1)
    assert result['values'] == [[7.0, 7.0], [7.0, 7.0]]


def test_values_cap_enforced():
    arr = np.zeros((64, 64, 1), dtype=np.float32)
    extract_values(arr, x=0, y=0, w=32, h=32)   # 1024: exactly at cap
    with pytest.raises(ValueError) as excinfo:
        extract_values(arr, x=0, y=0, w=33, h=32)
    assert str(VALUES_CAP) in str(excinfo.value)


def test_dump_npy_roundtrip(tmp_path):
    arr = np.arange(24, dtype=np.int32).reshape(4, 3, 2)
    path = dump_npy(arr, 'img->data[0]', 7, path=str(tmp_path / 'out.npy'))
    loaded = np.load(path)
    np.testing.assert_array_equal(loaded, arr)


def test_dump_npy_appends_missing_npy_suffix(tmp_path):
    arr = np.arange(6, dtype=np.int32).reshape(2, 3, 1)
    path = dump_npy(arr, 'img->data[0]', 1, path=str(tmp_path / 'out'))
    loaded = np.load(path)
    np.testing.assert_array_equal(loaded, arr)


def test_dump_npy_default_path_is_sanitized(tmp_path, monkeypatch):
    monkeypatch.setenv('TMPDIR', str(tmp_path))
    import importlib
    import tempfile
    importlib.reload(tempfile)
    arr = np.zeros((2, 2, 1), dtype=np.uint8)
    path = dump_npy(arr, 'img->data[0]', 3)
    assert path.endswith('.npy')
    name = path.rsplit('/', 1)[-1]
    assert '(' not in name and '>' not in name and '[' not in name
    np.testing.assert_array_equal(np.load(path), arr)
