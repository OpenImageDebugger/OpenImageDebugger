import io

import numpy as np
import pytest
from PIL import Image as PILImage

from oidmcp.render import render_view
from conftest import make_meta


def png_pixels(png_bytes):
    return np.asarray(PILImage.open(io.BytesIO(png_bytes)).convert('RGB'))


def gray_meta(arr):
    return make_meta(arr.shape[1], arr.shape[0], channels=1, type_value=5,
                     raw=arr.astype(np.float32).tobytes())


def test_minmax_normalization_spans_full_range():
    arr = np.linspace(0.0, 1.0, 16, dtype=np.float32).reshape(4, 4, 1)
    png, info = render_view(arr, gray_meta(arr))
    pixels = png_pixels(png)
    assert pixels.min() == 0
    assert pixels.max() == 255
    assert info['normalization']['mode'] == 'per-channel-minmax'
    assert info['normalization']['bounds'] == [[0.0, 1.0]]


def test_explicit_vmin_vmax():
    arr = np.full((4, 4, 1), 5.0, dtype=np.float32)
    png, info = render_view(arr, gray_meta(arr), vmin=0.0, vmax=10.0)
    pixels = png_pixels(png)
    assert abs(int(pixels[0, 0, 0]) - 127) <= 1
    assert info['normalization']['mode'] == 'explicit'


def test_region_zoom_keeps_full_buffer_bounds():
    arr = np.zeros((8, 8, 1), dtype=np.float32)
    arr[0, 0, 0] = 100.0          # global max, outside the region
    arr[4:, 4:, 0] = 50.0
    png, info = render_view(arr, gray_meta(arr), region=(4, 4, 4, 4))
    pixels = png_pixels(png)
    # 50 of 100 → mid gray, NOT full white (bounds from full buffer)
    assert abs(int(pixels[-1, -1, 0]) - 127) <= 1
    assert info['region'] == [4, 4, 4, 4]


def test_channel_selection_renders_grayscale():
    arr = np.zeros((4, 4, 3), dtype=np.float32)
    arr[:, :, 2] = 1.0
    meta = make_meta(4, 4, channels=3, type_value=5, raw=arr.tobytes())
    png, info = render_view(arr, meta, channel=2)
    pixels = png_pixels(png)
    assert (pixels[..., 0] == pixels[..., 1]).all()
    assert (pixels[..., 1] == pixels[..., 2]).all()
    assert info['channel'] == 2


def test_channel_out_of_range():
    arr = np.zeros((4, 4, 1), dtype=np.float32)
    with pytest.raises(ValueError):
        render_view(arr, gray_meta(arr), channel=3)


def test_bgra_layout_swaps_red_and_blue():
    # Storage channel 0 holds blue values under 'bgra'. Explicit
    # vmin/vmax gives identity mapping (constant channels would
    # otherwise normalize to black).
    arr = np.zeros((4, 4, 3), dtype=np.uint8)
    arr[:, :, 0] = 255
    meta = make_meta(4, 4, channels=3, type_value=0, pixel_layout='bgra',
                     raw=arr.tobytes())
    pixels = png_pixels(render_view(arr, meta, vmin=0, vmax=255)[0])
    assert pixels[0, 0, 2] == 255      # blue in the PNG
    assert pixels[0, 0, 0] == 0        # not red


def test_nan_pixels_are_magenta():
    arr = np.linspace(0.0, 1.0, 16, dtype=np.float32).reshape(4, 4, 1)
    arr = arr.copy()
    arr[1, 1, 0] = np.nan
    png, info = render_view(arr, gray_meta(arr))
    pixels = png_pixels(png)
    # 4x4 is NN-upscaled 16x to 64x64: source pixel (1, 1) becomes the
    # block at rows/cols 16..31; source (0, 0) stays at (0, 0).
    assert list(pixels[16, 16]) == [255, 0, 255]
    assert list(pixels[0, 0]) != [255, 0, 255]
    assert info['nan_count'] == 1


def test_tiny_buffers_are_upscaled():
    arr = np.eye(2, dtype=np.float32).reshape(2, 2, 1)
    png, info = render_view(arr, gray_meta(arr))
    height, width = info['output_px']
    assert min(height, width) >= 64


def test_large_buffers_are_downscaled():
    arr = np.zeros((300, 700, 1), dtype=np.float32)
    png, info = render_view(arr, gray_meta(arr), max_px=100)
    height, width = info['output_px']
    assert max(height, width) <= 100
    assert width > height  # aspect preserved
