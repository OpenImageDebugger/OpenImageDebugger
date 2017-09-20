# -*- coding: utf-8 -*-

"""
Module developed for quick testing the ImageWatch shared library
"""

import math
import time
import threading
import queue

import numpy

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
            time.sleep(0.5)

    except KeyboardInterrupt:
        window.terminate()
        exit(0)

    dummy_debugger.kill()


def _gen_color(pos, k, f_a, f_b):
    """
    Generates a color for the pixel at (pos[0], pos[1]) with coefficients k[0]
    and k[1], and colouring functions f_a and f_b that map R->[-1, 1].
    """
    return (f_a(pos[0] * f_b(pos[1]/k[0])/k[1]) + 1) * 255 / 2


def _gen_buffers(width, height):
    """
    Generate sample buffers
    """
    channels = [3, 1]

    types = [{'numpy': numpy.uint8, 'giw': symbols.GIW_TYPES_UINT8},
             {'numpy': numpy.float32, 'giw': symbols.GIW_TYPES_FLOAT32}]

    tex1 = [0] * width * height * channels[0]
    tex2 = [0] * width * height * channels[1]

    for pos_y in range(0, height):
        for pos_x in range(0, width):
            pixel_pos = [pos_x, pos_y]

            buffer_pos = pos_y * channels[0] * width + channels[0] * pos_x

            tex1[buffer_pos + 0] = _gen_color(
                pixel_pos, [20, 80], math.cos, math.cos)
            tex1[buffer_pos + 1] = _gen_color(
                pixel_pos, [50, 200], math.sin, math.cos)
            tex1[buffer_pos + 2] = _gen_color(
                pixel_pos, [30, 120], math.cos, math.cos)

            buffer_pos = pos_y * channels[1] * width + channels[1] * pos_x

            for channel in range(0, channels[1]):
                tex2[buffer_pos + channel] = (
                    math.exp(math.cos(pos_x / 5.0) *
                             math.sin(pos_y / 5.0)))

    tex_arr1 = numpy.asarray(tex1, types[0]['numpy'])
    tex_arr2 = numpy.asarray(tex2, types[1]['numpy'])
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
            'pixel_layout': 'rgba'
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
            'pixel_layout': 'rgba'
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
        self._request_queue = queue.Queue()
        self._request_consumer_thread = threading.Thread(
            target=self._request_consumer,
            daemon=True)
        self._request_consumer_thread.start()

    def _request_consumer(self):
        while self._is_running:
            latest_request = self._request_queue.get(block=True, timeout=None)
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
        self._request_queue.put(callable_request)
