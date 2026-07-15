import os
import sys

import numpy as np
import pytest

from oidmcp.analysis import (
    VALUES_CAP,
    compute_stats,
    dump_npy,
    extract_values,
)
from oidmcp.analysis import _current_user, _safe_dump_dir, _sanitize_tree
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


def test_stats_single_channel_has_no_color_label():
    # A single-channel buffer (e.g. an Eigen matrix) has no color
    # semantics, even though bridges default pixel_layout to 'bgra'.
    arr = np.zeros((2, 2, 1), dtype=np.float32)
    meta = make_meta(2, 2, channels=1, type_value=5, pixel_layout='bgra',
                     raw=arr.tobytes())
    stats = compute_stats(arr, meta)
    assert stats['per_channel'][0]['label'] is None


def test_stats_multi_channel_keeps_layout_labels():
    arr = np.zeros((2, 2, 3), dtype=np.float32)
    meta = make_meta(2, 2, channels=3, type_value=5, pixel_layout='bgra',
                     raw=arr.tobytes())
    stats = compute_stats(arr, meta)
    assert [c['label'] for c in stats['per_channel']] == ['b', 'g', 'r']


def test_stats_promotes_2d_array_to_single_channel():
    # A bare (H, W) matrix is a single-channel buffer; compute_stats must
    # not crash on the missing channel axis.
    arr = np.array([[1, 2, 3, 4], [5, 6, 7, 8]], dtype=np.float32)
    meta = make_meta(4, 2, channels=1, type_value=5, raw=arr.tobytes())
    stats = compute_stats(arr, meta)
    assert stats['channels'] == 1
    assert len(stats['per_channel']) == 1
    channel = stats['per_channel'][0]
    assert channel['min'] == pytest.approx(1.0)
    assert channel['max'] == pytest.approx(8.0)
    assert channel['label'] is None


def test_stats_2d_array_with_region():
    arr = np.zeros((4, 4), dtype=np.float32)
    arr[2:, 2:] = 9.0
    meta = make_meta(4, 4, channels=1, type_value=5, raw=arr.tobytes())
    stats = compute_stats(arr, meta, region=(2, 2, 2, 2))
    assert stats['per_channel'][0]['min'] == pytest.approx(9.0)


def test_stats_rejects_non_2d_3d_array():
    arr = np.arange(4, dtype=np.float32)   # 1-D
    meta = make_meta(4, 1, channels=1, type_value=5, raw=arr.tobytes())
    with pytest.raises(ValueError) as excinfo:
        compute_stats(arr, meta)
    assert '1' in str(excinfo.value)


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


def test_sanitize_tree_recurses_into_dicts():
    tree = {
        'min': float('nan'),
        'max': float('inf'),
        'nested': {'low': float('-inf'), 'ok': 1.5},
        'rows': [{'v': float('nan')}, [float('inf'), 2.0]],
    }
    assert _sanitize_tree(tree) == {
        'min': 'NaN',
        'max': 'Inf',
        'nested': {'low': '-Inf', 'ok': 1.5},
        'rows': [{'v': 'NaN'}, ['Inf', 2.0]],
    }


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


def test_dump_npy_roundtrip(dump_dir):
    arr = np.arange(24, dtype=np.int32).reshape(4, 3, 2)
    path = dump_npy(arr, 'img->data[0]', 7, path='out.npy')
    # Writes are confined to the hardened per-user dump directory.
    assert (os.path.dirname(path)
            == str(dump_dir / ('oid-dumps-' + _current_user())))
    np.testing.assert_array_equal(np.load(path), arr)
    # The atomic write leaves no temp file behind.
    assert not [name for name in os.listdir(os.path.dirname(path))
                if name.endswith('.tmp')]


def test_dump_npy_appends_missing_npy_suffix(dump_dir):
    arr = np.arange(6, dtype=np.int32).reshape(2, 3, 1)
    path = dump_npy(arr, 'img->data[0]', 1, path='out')
    assert path.endswith('/out.npy')
    np.testing.assert_array_equal(np.load(path), arr)


def test_dump_npy_default_path_is_sanitized(dump_dir):
    arr = np.zeros((2, 2, 1), dtype=np.uint8)
    path = dump_npy(arr, 'img->data[0]', 3)
    assert path.endswith('.npy')
    name = os.path.basename(path)
    assert '(' not in name and '>' not in name and '[' not in name
    # The default dir is per-user, so another user cannot pre-own it.
    assert (os.path.basename(os.path.dirname(path))
            == 'oid-dumps-' + _current_user())
    np.testing.assert_array_equal(np.load(path), arr)


def test_dump_npy_refuses_to_overwrite_existing_file(dump_dir):
    original = np.arange(6, dtype=np.int32).reshape(2, 3, 1)
    target = dump_npy(original, 'img', 1, path='out.npy')
    with pytest.raises(FileExistsError) as excinfo:
        dump_npy(np.zeros((2, 3, 1), dtype=np.int32), 'img', 1,
                 path='out.npy')
    assert 'overwrite' in str(excinfo.value)
    # The existing file is left untouched.
    np.testing.assert_array_equal(np.load(target), original)


def test_dump_npy_overwrite_flag_replaces_existing_file(dump_dir):
    dump_npy(np.arange(6, dtype=np.int32).reshape(2, 3, 1), 'img', 1,
             path='out.npy')
    replacement = np.full((2, 3, 1), 9, dtype=np.int32)
    path = dump_npy(replacement, 'img', 1, path='out.npy', overwrite=True)
    np.testing.assert_array_equal(np.load(path), replacement)


def test_dump_npy_overwrite_guard_uses_final_suffixed_path(dump_dir):
    # The guard checks the resolved '.npy' target, not the raw argument.
    dump_npy(np.zeros((1, 1, 1), dtype=np.uint8), 'img', 1, path='out.npy')
    with pytest.raises(FileExistsError):
        dump_npy(np.zeros((1, 1, 1), dtype=np.uint8), 'img', 1,
                 path='out')   # no explicit suffix


def test_dump_npy_default_path_refuses_silent_overwrite(dump_dir):
    arr = np.zeros((2, 2, 1), dtype=np.uint8)
    first = dump_npy(arr, 'img', 3)
    with pytest.raises(FileExistsError):
        dump_npy(arr, 'img', 3)
    assert dump_npy(arr, 'img', 3, overwrite=True) == first


@pytest.mark.parametrize('bad_path', [
    'sub/out.npy',        # relative path with a separator
    '/etc/escape.npy',    # absolute path
    '..',                 # parent-directory reference
    '.',                  # the dump directory itself
    '',                   # empty name
])
def test_dump_npy_rejects_non_bare_filename(dump_dir, bad_path):
    with pytest.raises(ValueError) as excinfo:
        dump_npy(np.zeros((1, 1, 1), dtype=np.uint8), 'img', 1,
                 path=bad_path)
    assert 'bare filename' in str(excinfo.value)


@pytest.mark.skipif(sys.platform == 'win32', reason='POSIX symlinks')
def test_dump_npy_refuses_symlink_at_target(dump_dir):
    directory = _safe_dump_dir()
    victim = dump_dir / 'victim.npy'
    os.symlink(str(victim), str(directory / 'link.npy'))
    with pytest.raises(ValueError) as excinfo:
        dump_npy(np.zeros((1, 1, 1), dtype=np.uint8), 'img', 1,
                 path='link.npy', overwrite=True)
    assert 'symlink' in str(excinfo.value)
    # Nothing was written through the link.
    assert not victim.exists()


@pytest.mark.skipif(sys.platform == 'win32', reason='POSIX symlinks')
def test_dump_npy_default_dir_rejects_symlink(dump_dir):
    elsewhere = dump_dir / 'elsewhere'
    elsewhere.mkdir(mode=0o700)
    os.symlink(str(elsewhere),
               str(dump_dir / ('oid-dumps-' + _current_user())))
    arr = np.zeros((2, 2, 1), dtype=np.uint8)
    with pytest.raises(RuntimeError) as excinfo:
        dump_npy(arr, 'img', 1)
    assert 'symlink' in str(excinfo.value)
