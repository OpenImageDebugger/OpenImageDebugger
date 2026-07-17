"""Adapt viewer buffer metadata onto the shape the pixel pipeline expects.

The viewer's own ``get_buffer`` reply describes a buffer with viewer-native
field names (notably ``step`` for the row pitch, and no debugger stop
generation). ``decode_buffer``/``compute_stats``/``extract_values``/
``render_view`` were all written against the debugger bridge's metadata
shape (``row_stride``, ``stop_generation``, ...); ``to_bridge_meta`` maps
one onto the other so that pipeline can be reused unchanged for viewer
buffers.
"""

from __future__ import annotations


def to_bridge_meta(viewer_meta: dict) -> dict:
    """
    Map a viewer ``get_buffer`` metadata dict onto the bridge-style shape.

    ``type`` is already the raw BufferType int code shared with the
    debugger bridges (matching ``buffers.DTYPES``), so it is carried
    through unchanged. ``step`` becomes ``row_stride``. The viewer has no
    notion of a debugger stop generation, so ``stop_generation`` is always
    0 -- a buffer fetched from the viewer is always its current one.
    """
    return {
        'type': viewer_meta['type'],
        'height': viewer_meta['height'],
        'width': viewer_meta['width'],
        'channels': viewer_meta['channels'],
        'row_stride': viewer_meta['step'],
        'pixel_layout': viewer_meta.get('pixel_layout') or '',
        'transpose_buffer': bool(viewer_meta.get('transpose_buffer', False)),
        'display_name': viewer_meta.get('display_name'),
        'stop_generation': 0,
    }
