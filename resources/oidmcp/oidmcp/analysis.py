"""Exact-number inspection: stats, value crops, lossless dumps."""

from __future__ import annotations

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


def compute_stats(arr, meta, region=None):
    view = crop_region(arr, region) if region is not None else arr
    floating = np.issubdtype(arr.dtype, np.floating)
    layout = (meta.get('pixel_layout') or '')
    per_channel = []
    for index in range(view.shape[2]):
        channel = view[:, :, index]
        finite = channel[np.isfinite(channel)] if floating else channel
        empty = finite.size == 0
        per_channel.append({
            'channel': index,
            'label': layout[index] if index < len(layout) else None,
            'min': None if empty else _finite_or_none(finite.min()),
            'max': None if empty else _finite_or_none(finite.max()),
            'mean': None if empty else _finite_or_none(finite.mean()),
            'std': None if empty else _finite_or_none(finite.std()),
            'nan': int(np.isnan(channel).sum()) if floating else 0,
            'inf': int(np.isinf(channel).sum()) if floating else 0,
            'zeros': int((channel == 0).sum()),
            'count': int(channel.size),
        })
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
    return _json_safe(node)


def extract_values(arr, x, y, w, h, channel=None):
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


def dump_npy(arr, symbol, stop_generation, path=None):
    """
    Save the full decoded buffer (padding already stripped) as .npy.
    """
    if path is None:
        directory = Path(tempfile.gettempdir()) / 'oid-dumps'
        directory.mkdir(mode=0o700, exist_ok=True)
        name = re.sub(r'[^A-Za-z0-9_.-]', '_', symbol)
        path = str(directory / f'{name}_gen{stop_generation}.npy')
    np.save(path, np.ascontiguousarray(arr))
    return os.path.abspath(path)
