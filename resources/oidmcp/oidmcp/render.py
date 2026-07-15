"""Render decoded buffers to PNG for agent eyes."""

from __future__ import annotations

import io

import numpy as np
from PIL import Image as PILImage

from .buffers import crop_region

MAGENTA = np.array([255, 0, 255], dtype=np.uint8)
MIN_OUTPUT_PX = 64


def _channel_bounds(arr, vmin, vmax):
    """Per-storage-channel (lo, hi) over the FULL buffer."""
    if vmin is not None and vmax is not None:
        return [(float(vmin), float(vmax))] * arr.shape[2], 'explicit'
    bounds = []
    floating = np.issubdtype(arr.dtype, np.floating)
    for index in range(arr.shape[2]):
        channel = arr[:, :, index]
        if floating:
            finite = channel[np.isfinite(channel)]
            if finite.size == 0:
                bounds.append((0.0, 0.0))
                continue
            bounds.append((float(finite.min()), float(finite.max())))
        else:
            bounds.append((float(channel.min()), float(channel.max())))
    return bounds, 'per-channel-minmax'


def _channel_gray(channel, lo, hi):
    """Normalize one 2-D channel to uint8 [0, 255]."""
    if hi <= lo:
        return np.zeros(channel.shape, dtype=np.uint8)
    scaled = (channel.astype(np.float64) - lo) / (hi - lo)
    scaled = np.clip(np.nan_to_num(scaled, nan=0.0,
                                   posinf=1.0, neginf=0.0), 0.0, 1.0)
    return (scaled * 255.0 + 0.5).astype(np.uint8)


def _nn_scale(img, max_px):
    height, width = img.shape[:2]
    longest = max(height, width)
    if longest > max_px:
        factor = max_px / longest
        out_h = max(1, int(height * factor))
        out_w = max(1, int(width * factor))
        rows = (np.arange(out_h) / factor).astype(int)
        cols = (np.arange(out_w) / factor).astype(int)
        return img[rows][:, cols]
    if longest < MIN_OUTPUT_PX:
        repeat = -(-MIN_OUTPUT_PX // longest)  # ceil
        return np.repeat(np.repeat(img, repeat, axis=0), repeat, axis=1)
    return img


def render_view(arr, meta, region=None, channel=None,
                vmin=None, vmax=None, max_px=512):
    """
    Render an (H, W, C) display-oriented array to (png_bytes, info).
    """
    channels = arr.shape[2]
    layout = (meta.get('pixel_layout') or 'rgba')[:channels]
    if channel is not None and not 0 <= channel < channels:
        raise ValueError(
            f'channel {channel} out of range; buffer has {channels}')
    if (vmin is None) != (vmax is None):
        raise ValueError('vmin and vmax must be given together')

    bounds, mode = _channel_bounds(arr, vmin, vmax)
    view = crop_region(arr, region) if region is not None else arr

    if channel is not None:
        used = [channel]
    elif channels == 1:
        used = [0]
    elif channels == 2:
        used = [0, 1]
    else:
        used = [layout.index(sem) if sem in layout else fallback
                for sem, fallback in (('r', 0), ('g', 1), ('b', 2))]

    grays = {i: _channel_gray(view[:, :, i], *bounds[i]) for i in set(used)}
    if channel is not None or channels == 1:
        rgb = np.stack([grays[used[0]]] * 3, axis=-1)
    elif channels == 2:
        zero = np.zeros_like(grays[0])
        rgb = np.stack([grays[0], grays[1], zero], axis=-1)
    else:
        rgb = np.stack([grays[i] for i in used], axis=-1)

    floating = np.issubdtype(arr.dtype, np.floating)
    nan_count = int(np.isnan(arr).sum()) if floating else 0
    inf_count = int(np.isinf(arr).sum()) if floating else 0
    if floating:
        mask = np.isnan(view[:, :, sorted(set(used))]).any(axis=2)
        rgb = rgb.copy()
        rgb[mask] = MAGENTA

    rgb = _nn_scale(rgb, int(max_px))
    output = io.BytesIO()
    PILImage.fromarray(rgb, 'RGB').save(output, format='PNG')

    info = {
        'display_name': meta.get('display_name'),
        'width': arr.shape[1],
        'height': arr.shape[0],
        'channels': channels,
        'dtype': str(arr.dtype),
        'row_stride': meta.get('row_stride'),
        'pixel_layout': meta.get('pixel_layout'),
        'transpose_buffer': bool(meta.get('transpose_buffer')),
        'stop_generation': meta.get('stop_generation'),
        'region': list(region) if region is not None else None,
        'channel': channel,
        'normalization': {'mode': mode,
                          'bounds': [[lo, hi] for lo, hi in bounds]},
        'nan_count': nan_count,
        'inf_count': inf_count,
        'alpha': 'ignored (view a single channel to inspect alpha)'
                 if channels == 4 and channel is None else None,
        'output_px': [rgb.shape[0], rgb.shape[1]],
    }
    return output.getvalue(), info
