#!/usr/bin/python3

import sys
import ctypes
import os
from ctypes import cdll
import pysigset, signal

script_path = os.path.dirname(os.path.realpath(__file__))
lib = cdll.LoadLibrary(script_path+'/libgdb-imagewatch.so')
lib.plot_binary.argtypes = [ctypes.py_object, # Buffer ptr
                            ctypes.py_object, # Variable name
                            ctypes.c_int, # Buffer width
                            ctypes.c_int, # Buffer height
                            ctypes.c_int, # Number of channels
                            ctypes.c_int] # Type (0=float32, 1=uint8)
lib.update_plot.argtypes = [ctypes.py_object, # Buffer ptr
                            ctypes.py_object, # Variable name
                            ctypes.c_int, # Buffer width
                            ctypes.c_int, # Buffer height
                            ctypes.c_int, # Number of channels
                            ctypes.c_int] # Type (0=float32, 1=uint8)

"""
if __name__ == "__main__":
    import numpy
    import math
    width=2
    height=2
    channels=1
    tex = [None]*width*height*channels
    for y in range(0,height):
        for x in range(0,width):
            for c in range(0, channels):
                tex[y*channels*width+channels*x+c] = y/10+x/10#math.cos(x/width*math.pi+c/channels*math.pi/4.0)*255
                pass
            pass
        pass

    tex_arr = numpy.asarray(tex, numpy.float32)
    mem = memoryview(tex_arr)
    lib.plot_binary(mem, 'python_test', width, height, channels, 0)

    exit()
    pass
"""

import gdb
import threading

observed_variables = set()
def plot_thread(mem, var_name, width, height, channels, type):
    observed_variables.add(str(var_name))
    lib.plot_binary(mem, var_name, width, height, channels, type)
    observed_variables.remove(str(var_name))
    pass

def get_buffer_info(picked_obj):
    char_type = gdb.lookup_type("char")
    char_pointer_type = char_type.pointer()
    buffer = picked_obj['data'].cast(char_pointer_type)
    width = int(picked_obj['w'])
    height = int(picked_obj['h'])
    channels = int(picked_obj['channels'])
    type = int(picked_obj['type'])

    return (buffer, width, height, channels, type)

def get_buffer_size(width, height, channels, type):
    texel_size = channels
    if type==0:
        texel_size *= 4 # float type
    elif type==1:
        texel_size *= 1 # uint8_t type
        pass

    return texel_size * width*height

class PlotterCommand(gdb.Command):
    def __init__(self):
        super(PlotterCommand, self).__init__("plot",
                                             gdb.COMMAND_DATA,
                                             gdb.COMPLETE_SYMBOL)
        pass

    def invoke(self, arg, from_tty):
        args = gdb.string_to_argv(arg)

        if str(args[0]) in observed_variables:
            print('Variable '+str(args[0])+ ' is already being visualized!')
            return

        picked_obj = gdb.parse_and_eval(args[0])

        buffer, width, height, channels, type = get_buffer_info(picked_obj)
      
        bytes = get_buffer_size(width, height, channels, type)
        inferior = gdb.selected_inferior()
        mem = inferior.read_memory(buffer, bytes)

        ##
        # By default, my new threads will be the ones receiving the precious
        # signals from the operating system. These signals should go to GDB so
        # it can do its thing, and since it doesnt get them, my threads will
        # make gdb hang. The solution is to configure the new thread to forward
        # the signal back to the main thread, which is done by pysigset.
        with pysigset.suspended_signals(signal.SIGCHLD):
            plot_thread_instance=threading.Thread(target=plot_thread, args=(mem, args[0], width, height, channels, type))
            plot_thread_instance.daemon=True
            plot_thread_instance.start()
        pass

    pass

# Update all buffers on each stop event
def stop_event_handler(event):
    for variable in observed_variables:
        picked_obj = gdb.parse_and_eval(variable)

        buffer, width, height, channels, type = get_buffer_info(picked_obj)
      
        bytes = get_buffer_size(width, height, channels, type)
        inferior = gdb.selected_inferior()
        mem = inferior.read_memory(buffer, bytes)

        print(variable,width,height,channels,type)
        lib.update_plot(mem, variable, width, height, channels, type)
        pass
    pass

##
# Setup GDB interface
PlotterCommand()
gdb.events.stop.connect(stop_event_handler)
