"""Exact-number inspection: stats, value crops, lossless dumps."""

from __future__ import annotations

import getpass
import math
import os
import re
import tempfile
from pathlib import Path

import numpy as np

from .buffers import crop_region

VALUES_CAP = 1024


def _finite_or_none(value):
    value = float(value)
    return value if math.isfinite(value) else None


def _channel_stats(channel: np.ndarray, floating: bool, label) -> dict:
    """Nan-aware stat dict for one 2-D channel (all-NaN -> None stats)."""
    finite = channel[np.isfinite(channel)] if floating else channel
    empty = finite.size == 0
    return {
        'label': label,
        'min': None if empty else _finite_or_none(finite.min()),
        'max': None if empty else _finite_or_none(finite.max()),
        'mean': None if empty else _finite_or_none(finite.mean()),
        'std': None if empty else _finite_or_none(finite.std()),
        'nan': int(np.isnan(channel).sum()) if floating else 0,
        'inf': int(np.isinf(channel).sum()) if floating else 0,
        'zeros': int((channel == 0).sum()),
        'count': int(channel.size),
    }


def _as_hwc(arr: np.ndarray) -> np.ndarray:
    """
    Normalize a buffer to (H, W, C). A bare (H, W) matrix is a
    single-channel buffer, so promote it to (H, W, 1); reject anything
    that is neither 2-D nor 3-D with a clear error.
    """
    if arr.ndim == 2:
        return arr[:, :, np.newaxis]
    if arr.ndim != 3:
        raise ValueError(
            f'expected a 2-D or 3-D buffer, got {arr.ndim} dimensions')
    return arr


def compute_stats(arr: np.ndarray, meta: dict,
                   region: tuple | None = None) -> dict:
    arr = _as_hwc(arr)
    view = crop_region(arr, region) if region is not None else arr
    floating = np.issubdtype(arr.dtype, np.floating)
    layout = (meta.get('pixel_layout') or '')
    if arr.shape[2] == 1:
        # A single-channel buffer (an Eigen matrix, a mask) has no
        # color-channel semantics; do not label it after the layout.
        layout = ''
    per_channel = []
    for index in range(view.shape[2]):
        channel = view[:, :, index]
        label = layout[index] if index < len(layout) else None
        stats = {'channel': index, **_channel_stats(channel, floating, label)}
        per_channel.append(stats)
    return {
        'width': arr.shape[1],
        'height': arr.shape[0],
        'channels': arr.shape[2],
        'dtype': str(arr.dtype),
        'region': list(region) if region is not None else None,
        'per_channel': per_channel,
    }


def _json_safe(value):
    if isinstance(value, float) and not math.isfinite(value):
        if math.isnan(value):
            return 'NaN'
        return 'Inf' if value > 0 else '-Inf'
    return value


def _sanitize_tree(node):
    if isinstance(node, list):
        return [_sanitize_tree(item) for item in node]
    if isinstance(node, dict):
        return {key: _sanitize_tree(value) for key, value in node.items()}
    return _json_safe(node)


def extract_values(arr: np.ndarray, x: int, y: int, w: int, h: int,
                    channel: int | None = None) -> dict:
    """
    Exact values for a crop; capped so results stay readable.
    """
    per_pixel = 1 if channel is not None else arr.shape[2]
    total = w * h * per_pixel
    if total > VALUES_CAP:
        raise ValueError(
            f'{w}x{h} crop holds {total} values, over the {VALUES_CAP} '
            f'cap; request a smaller region or a single channel')
    view = crop_region(arr, (x, y, w, h))
    if channel is not None:
        if not 0 <= channel < arr.shape[2]:
            raise ValueError(
                f'channel {channel} out of range; buffer has '
                f'{arr.shape[2]}')
        view = view[:, :, channel]
    return {
        'x': x, 'y': y, 'w': w, 'h': h,
        'channel': channel,
        'dtype': str(arr.dtype),
        'values': _sanitize_tree(view.tolist()),
    }


def _current_user() -> str:
    try:
        return getpass.getuser()
    except Exception:
        return str(os.getuid()) if hasattr(os, 'getuid') else 'user'


def _safe_dump_dir() -> Path:
    """
    Per-user default dump directory, hardened against a shared tempdir:
    reject a pre-created symlink or a directory owned by another user,
    and force 0o700 (mkdir ignores the mode when the dir pre-exists).
    """
    directory = Path(tempfile.gettempdir()) / ('oid-dumps-' + _current_user())
    if directory.is_symlink():
        raise RuntimeError('dump directory %s is a symlink' % directory)
    directory.mkdir(mode=0o700, exist_ok=True)
    if hasattr(os, 'getuid'):
        info = os.stat(directory)
        if info.st_uid != os.getuid():
            raise RuntimeError(
                'dump directory %s is owned by another user' % directory)
        os.chmod(directory, 0o700)
    return directory


def dump_npy(arr: np.ndarray, symbol: str, stop_generation: int,
             path: str | None = None, overwrite: bool = False) -> str:
    """
    Save the full decoded buffer (padding already stripped) as .npy.

    Every write is confined to the hardened per-user dump directory
    (``_safe_dump_dir()``); ``path`` may only pick a bare filename
    inside it, never an arbitrary location. The write itself is atomic
    and symlink-safe.

    Never clobbers an existing file: if the resolved target already
    exists, raise unless ``overwrite`` is set, so an unintended ``path``
    cannot silently destroy the caller's data.
    """
    directory = _safe_dump_dir()
    if path is None:
        name = re.sub(r'[^A-Za-z0-9_.-]', '_', symbol)
        filename = f'{name}_gen{stop_generation}.npy'
    else:
        # The client picks only a name INSIDE the hardened dump dir; a
        # path separator, '..', or an absolute path could escape it.
        if (os.path.isabs(path)
                or os.path.basename(path) != path
                or path in ('.', '..', '')):
            raise ValueError(
                'dump path must be a bare filename within the dump '
                'directory')
        filename = path if path.endswith('.npy') else f'{path}.npy'
    target = os.path.join(str(directory), filename)
    if os.path.islink(target):
        raise ValueError(
            f'{target} is a symlink; refusing to write through it')
    if not overwrite and os.path.lexists(target):
        raise FileExistsError(
            f'{target} already exists; pass overwrite=true to replace it')
    # Atomic, symlink-safe write: fill a temp file in the same 0700 dir,
    # then os.replace() the directory entry (which never follows a
    # symlink at the target).
    fd, tmp = tempfile.mkstemp(dir=str(directory), suffix='.npy.tmp')
    try:
        with os.fdopen(fd, 'wb') as handle:
            np.save(handle, np.ascontiguousarray(arr))
        os.replace(tmp, target)
    except BaseException:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        raise
    return os.path.abspath(target)
