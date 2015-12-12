#!/usr/bin/python3

import sys
import ctypes
import os
from ctypes import cdll
import pysigset, signal
import threading
import time

##
# Load imagewatch library and set up its API
script_path = os.path.dirname(os.path.realpath(__file__))
lib = cdll.LoadLibrary(script_path+'/libgdb-imagewatch.so')
lib.plot_binary.argtypes = [ctypes.py_object, # Buffer ptr
                            ctypes.py_object, # Variable name
                            ctypes.c_int, # Buffer width
                            ctypes.c_int, # Buffer height
                            ctypes.c_int, # Number of channels
                            ctypes.c_int, # Type (0=float32, 1=uint8)
                            ctypes.c_int] # Step size (in pixels)
lib.update_plot.argtypes = [ctypes.py_object, # Buffer ptr
                            ctypes.py_object, # Variable name
                            ctypes.c_int, # Buffer width
                            ctypes.c_int, # Buffer height
                            ctypes.c_int, # Number of channels
                            ctypes.c_int, # Type (0=float32, 1=uint8)
                            ctypes.c_int] # Step size (in pixels)
lib.get_observed_variables.argtypes = [ctypes.py_object] # Observed variables
                                                         # set
FETCH_BUFFER_CBK_TYPE = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_char_p)
lib.initialize_window.argtypes = [
                              FETCH_BUFFER_CBK_TYPE # Python function to be called
                              ]                # when the user requests a symbol
                                               # name from the viewer interface
lib.update_plot.rettype = ctypes.c_bool # Buffer ptr
lib.update_available_variables.argtypes = [
                              ctypes.py_object # List of available variables in
                              ]                # the current context

def request_buffer_update(variable):
    picked_obj = gdb.parse_and_eval(variable)

    buffer, width, height, channels, type, step = get_buffer_info(picked_obj)

    bytes = get_buffer_size(width, height, channels, type, step)
    inferior = gdb.selected_inferior()
    mem = inferior.read_memory(buffer, bytes)

    lib.update_plot(mem, variable, width, height, channels, type, step)
    pass

class MainThreadPlotVariableRunner():
    def __init__(self, variable):
        self.variable = variable;
        pass
    def __call__(self):
        request_buffer_update(self.variable)
        pass
    pass

def plot_variable_cbk(requested_symbol):
    variable = requested_symbol.decode('utf-8')
    gdb.post_event(MainThreadPlotVariableRunner(variable))

    return 0

def initialize_window():
    ##
    # Initialize imagewatch window
    with pysigset.suspended_signals(signal.SIGCHLD):
        # By default, my new threads will be the ones receiving the precious
        # signals from the operating system. These signals should go to GDB so
        # it could do its thing, and since it doesnt get them, my thread will
        # make gdb hang. The solution is to configure the new thread to forward
        # the signal back to the main thread, which is done by pysigset.
        wnd_thread_instance=threading.Thread(target=lib.initialize_window, args=(FETCH_BUFFER_CBK_TYPE(plot_variable_cbk),))
        wnd_thread_instance.daemon=True
        wnd_thread_instance.start()
        pass
    pass

##
# Test application
if len(sys.argv)==2 and sys.argv[1] == '--test':
    import numpy
    import math

    initialize_window()

    width=10
    height=10
    channels=3
    tex = [None]*width*height*channels
    tex2 = [None]*width*height*channels
    for y in range(0,height):
        for x in range(0,width):
            for c in range(0, channels):
                tex[y*channels*width+channels*x+c] = math.cos(x/width*math.pi+
                                                   c/channels*math.pi/4.0)*255
                tex2[y*channels*width+channels*x+c] = 10*x+10*y
                pass
            pass
        pass

    tex_arr = numpy.asarray(tex, numpy.uint8)
    tex_arr2 = numpy.asarray(tex2, numpy.uint8)
    mem = memoryview(tex_arr)
    mem2 = memoryview(tex_arr2)
    step = width
    lib.plot_binary(mem, 'python_test', width, height, channels, 1, step)
    lib.plot_binary(mem2, 'python_test2', width, height, channels, 1, step)

    lib.update_available_variables(['test', 'sample', 'variable', 'hello_world'])

    while lib.is_running():
        time.sleep(0.5)

    lib.terminate()
    exit()
    pass

import gdb

##
# Default values created for OpenCV Mat structures. Change it according to your
# needs.
def get_buffer_info(picked_obj):
    # OpenCV constants
    CV_CN_MAX = 512
    CV_CN_SHIFT = 3
    CV_MAT_CN_MASK = ((CV_CN_MAX - 1) << CV_CN_SHIFT)
    CV_DEPTH_MAX = (1 << CV_CN_SHIFT)
    CV_MAT_TYPE_MASK = (CV_DEPTH_MAX*CV_CN_MAX - 1)
    CV_8U = 0
    CV_32F = 5

    char_type = gdb.lookup_type("char")
    char_pointer_type = char_type.pointer()
    buffer = picked_obj['data'].cast(char_pointer_type)
    if buffer==0x0:
        raise Exception('Received null buffer!')

    width = int(picked_obj['cols'])
    height = int(picked_obj['rows'])
    flags = int(picked_obj['flags'])

    channels = ((((flags) & CV_MAT_CN_MASK) >> CV_CN_SHIFT) + 1)
    step = int(int(picked_obj['step']['buf'][0])/channels)

    cvtype = ((flags) & CV_MAT_TYPE_MASK)

    if (cvtype&7) == CV_8U:
        type = 1 # uint8_t
    elif (cvtype&7) == (CV_32F):
        type = 0 # float32
        step = int(step/4)
        pass

    return (buffer, width, height, channels, type, step)

def get_buffer_size(width, height, channels, type, step):
    texel_size = channels
    if type==0:
        texel_size *= 4 # float type
    elif type==1:
        texel_size *= 1 # uint8_t type
        pass

    return texel_size * step*height

class PlotterCommand(gdb.Command):
    def __init__(self):
        super(PlotterCommand, self).__init__("plot",
                                             gdb.COMMAND_DATA,
                                             gdb.COMPLETE_SYMBOL)
        pass

    def invoke(self, arg, from_tty):
        args = gdb.string_to_argv(arg)
        var_name = str(args[0])

        picked_obj = gdb.parse_and_eval(args[0])

        buffer, width, height, channels, type, step = get_buffer_info(picked_obj)
      
        bytes = get_buffer_size(width, height, channels, type, step)
        inferior = gdb.selected_inferior()
        mem = inferior.read_memory(buffer, bytes)

        lib.plot_binary(mem, var_name, width, height, channels, type, step)
        pass

    pass

def is_symbol_observable(symbol):
    symbol_type = str(symbol.type)
    return symbol_type == 'cv::Mat' or symbol_type == 'cv::Mat *'

def update_observable_suggestions():
    frame = gdb.selected_frame()
    block = frame.block()
    observable_symbols = list()
    while block:
        for symbol in block:
            if (symbol.is_argument or symbol.is_variable):
                name = symbol.name
                if (not name in observable_symbols) and (is_symbol_observable(symbol)):
                    observable_symbols.append(name)
                    pass
                pass
            pass

        block = block.superblock
        pass

    if lib.is_running():
        lib.update_available_variables(observable_symbols)

    pass

# Update all buffers on each stop event
def stop_event_handler(event):
    if not lib.is_running():
        initialize_window()
        while not lib.is_running():
            time.sleep(0.1)
            pass
        pass

    update_observable_suggestions()

    observed_variables = set()
    lib.get_observed_variables(observed_variables)
    for variable in observed_variables:
        try:
            request_buffer_update(variable)
        except:
            pass
        pass
    pass

##
# Setup GDB interface
PlotterCommand()
gdb.events.stop.connect(stop_event_handler)
