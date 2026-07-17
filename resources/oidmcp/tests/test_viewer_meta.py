import numpy as np
import pytest

from oidmcp.analysis import compute_stats, extract_values
from oidmcp.buffers import decode_buffer
from oidmcp.render import render_view
from oidmcp.viewer_meta import to_bridge_meta


def _gradient_viewer_meta():
    """4x3 float32 single-channel gradient (values row*10+col), viewer shape."""
    arr = (np.arange(3)[:, None] * 10 + np.arange(4)[None, :]) \
        .astype(np.float32)
    viewer_meta = {
        'width': 4,
        'height': 3,
        'channels': 1,
        'step': 4,
        'type': 5,
        'pixel_layout': '',
        'transpose_buffer': False,
        'display_name': 'grad',
    }
    return viewer_meta, arr.tobytes()


def test_to_bridge_meta_maps_fields():
    viewer_meta, _raw = _gradient_viewer_meta()
    bridge_meta = to_bridge_meta(viewer_meta)
    assert bridge_meta == {
        'type': 5,
        'height': 3,
        'width': 4,
        'channels': 1,
        'row_stride': 4,
        'pixel_layout': '',
        'transpose_buffer': False,
        'display_name': 'grad',
        'stop_generation': 0,
    }


def test_bridge_meta_feeds_decode_and_analysis_pipeline():
    viewer_meta, raw = _gradient_viewer_meta()
    bridge_meta = to_bridge_meta(viewer_meta)

    arr = decode_buffer(bridge_meta, raw)
    assert arr.shape == (3, 4, 1)
    assert arr.dtype == np.float32
    assert arr[2, 3, 0] == pytest.approx(23.0)

    stats = compute_stats(arr, bridge_meta)
    assert stats['width'] == 4
    assert stats['height'] == 3
    assert stats['channels'] == 1
    assert stats['per_channel'][0]['min'] == pytest.approx(0.0)
    assert stats['per_channel'][0]['max'] == pytest.approx(23.0)

    values = extract_values(arr, 0, 0, 2, 2)
    assert values['values'] == [[[0.0], [1.0]], [[10.0], [11.0]]]

    png_bytes, info = render_view(arr, bridge_meta)
    assert png_bytes[:8] == b'\x89PNG\r\n\x1a\n'
    assert info['width'] == 4
    assert info['height'] == 3
    assert info['row_stride'] == 4
    assert info['stop_generation'] == 0


def test_transpose_buffer_defaults_false_when_absent():
    bridge_meta = to_bridge_meta({
        'width': 2, 'height': 2, 'channels': 1, 'step': 2, 'type': 5,
        'pixel_layout': '', 'display_name': 'x',
    })
    assert bridge_meta['transpose_buffer'] is False
    assert bridge_meta['stop_generation'] == 0
