# -*- coding: utf-8 -*-

"""
Module developed for quick testing the ImageWatch shared library
"""

import sys
import math
import time
import threading
import array

from giwscripts import giwwindow
from giwscripts import symbols
from giwscripts.debuggers.interfaces import BridgeInterface


def giwtest(script_path):
    """
    Entry point for the testing mode.
    """

    dummy_debugger = DummyDebugger()

    window = giwwindow.GdbImageWatchWindow(script_path, dummy_debugger)
    window.initialize_window()

    try:
        # Wait for window to initialize
        while not window.is_ready():
            time.sleep(0.1)

        window.set_available_symbols(dummy_debugger.get_available_symbols())

        for buffer in dummy_debugger.get_available_symbols():
            window.plot_variable(buffer)

        while window.is_ready():
            dummy_debugger.run_event_loop()
            time.sleep(0.1)

    except KeyboardInterrupt:
        window.terminate()
        exit(0)

    dummy_debugger.kill()


def _gen_color(pos, k, f_a, f_b):
    """
    Generates a color for the pixel at (pos[0], pos[1]) with coefficients k[0]
    and k[1], and colouring functions f_a and f_b that map R->[-1, 1].
    """
    p0 = float(pos[0])
    p1 = float(pos[1])
    return (f_a(p0 * f_b(p1/k[0])/k[1]) + 1.0) * 255.0 / 2.0


def _gen_buffers(width, height):
    """
    Generate sample buffers
    """
    channels = [3, 1]

    types = [{'array': 'B', 'giw': symbols.GIW_TYPES_UINT8},
             {'array': 'f', 'giw': symbols.GIW_TYPES_FLOAT32}]

    tex1 = [0] * width * height * channels[0]
    tex2 = [0] * width * height * channels[1]

    c_x = width * 2.0 / 3.0
    c_y = height * 0.5
    scale = 3.0 / width

    for pos_y in range(0, height):
        for pos_x in range(0, width):
            # Buffer 1: Coloured set
            pixel_pos = [pos_x, pos_y]

            buffer_pos = pos_y * channels[0] * width + channels[0] * pos_x

            tex1[buffer_pos + 0] = _gen_color(
                pixel_pos, [20.0, 80.0], math.cos, math.cos)
            tex1[buffer_pos + 1] = _gen_color(
                pixel_pos, [50.0, 200.0], math.sin, math.cos)
            tex1[buffer_pos + 2] = _gen_color(
                pixel_pos, [30.0, 120.0], math.cos, math.cos)

            # Buffer 2: Mandelbrot set
            pixel_pos = complex((pos_x-c_x), (pos_y-c_y)) * scale
            buffer_pos = pos_y * channels[1] * width + channels[1] * pos_x

            mandel_z = complex(0, 0)
            for _ in range(0, 20):
                mandel_z = mandel_z * mandel_z + pixel_pos

            z_norm_squared = mandel_z.real * mandel_z.real +\
                             mandel_z.imag * mandel_z.imag
            z_threshold = 5.0

            for channel in range(0, channels[1]):
                tex2[buffer_pos + channel] = z_threshold - min(z_threshold,
                                                               z_norm_squared)

    tex_arr1 = array.array(types[0]['array'], [int(val) for val in tex1])
    tex_arr2 = array.array(types[1]['array'], tex2)

    if sys.version_info[0] == 2:
        mem1 = buffer(tex_arr1)
        mem2 = buffer(tex_arr2)
    else:
        mem1 = memoryview(tex_arr1)
        mem2 = memoryview(tex_arr2)

    rowstride = width

    return {
        'sample_buffer_1': {
            'variable_name': 'sample_buffer_1',
            'display_name': 'uint8* sample_buffer_1',
            'pointer': mem1,
            'width': width,
            'height': height,
            'channels': channels[0],
            'type': types[0]['giw'],
            'row_stride': rowstride,
            'pixel_layout': 'rgba',
            'transpose_buffer': False
        },
        'sample_buffer_2': {
            'variable_name': 'sample_buffer_2',
            'display_name': 'float* sample_buffer_2',
            'pointer': mem2,
            'width': width,
            'height': height,
            'channels': channels[1],
            'type': types[1]['giw'],
            'row_stride': rowstride,
            'pixel_layout': 'rgba',
            'transpose_buffer': False
        }
    }


class DummyDebugger(BridgeInterface):
    """
    Very simple implementation of a debugger bridge for the sake of the test
    mode.
    """
    def __init__(self):
        width = 400
        height = 200
        self._buffers = _gen_buffers(width, height)
        self._buffer_names = [name for name in self._buffers]

        self._is_running = True
        self._incoming_request_queue = []

    def run_event_loop(self):
        if self._is_running:
            request_queue = self._incoming_request_queue
            self._incoming_request_queue = []

            while len(request_queue) > 0:
                latest_request = request_queue.pop(-1)
                latest_request()

    def kill(self):
        """
        Request consumer thread to finish its execution
        """
        self._is_running = False

    def get_casted_pointer(self, typename, debugger_object):
        """
        No need to cast anything in this example
        """
        return debugger_object

    def register_event_handlers(self, events):
        """
        No need to register events in this example
        """
        pass

    def get_available_symbols(self):
        """
        Return the names of the available sample buffers
        """
        return self._buffer_names

    def get_buffer_metadata(self, var_name):
        """
        Search in the list of available buffers and return the requested one
        """
        if var_name in self._buffers:
            return self._buffers[var_name]

        return None

    def queue_request(self, callable_request):
        self._incoming_request_queue.append(callable_request)
