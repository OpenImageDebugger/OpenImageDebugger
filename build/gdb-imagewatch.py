#!/usr/bin/python3

import sys
import ctypes
import os
from ctypes import cdll

script_path = os.path.dirname(os.path.realpath(__file__))
lib = cdll.LoadLibrary(script_path+'/libwatch.so')
lib.plot_binary.argtypes = [ctypes.py_object, # Buffer ptr
                            ctypes.py_object, # Variable name
                            ctypes.c_int, # Buffer width
                            ctypes.c_int, # Buffer height
                            ctypes.c_int, # Number of channels
                            ctypes.c_int] # Type (0=float32, 1=uint8)

"""
if __name__ == "__main__":
    import numpy
    import math
    width=100
    height=80
    channels=3
    tex = [None]*width*height*channels
    for y in range(0,height):
        for x in range(0,width):
            for c in range(0, channels):
                tex[y*channels*width+channels*x+c] = math.cos(x/width*math.pi+c/channels*math.pi/4.0)*255
                pass
            pass
        pass

    tex_arr = numpy.asarray(tex, numpy.float32)
    mem = memoryview(tex_arr)
    lib.plot_binary(mem, width, height, channels, 0)

    exit()
    pass
"""

def plot_thread(mem, var_name, width, height, channels, type):
    lib.plot_binary(mem, var_name, width, height, channels, type)
    pass

import gdb
import threading
class PlotterCommand(gdb.Command):
    def __init__(self):
        super(PlotterCommand, self).__init__("plot",
                                             gdb.COMMAND_DATA,
                                             gdb.COMPLETE_SYMBOL)
        pass

    def invoke(self, arg, from_tty):
        args = gdb.string_to_argv(arg)

        picked_obj = gdb.parse_and_eval(args[0])

        char_type = gdb.lookup_type("char")
        char_pointer_type = char_type.pointer()
        buffer = gdb.parse_and_eval(args[0]+'.ptr()').cast(char_pointer_type)
        width = int(picked_obj['w'])
        height = int(picked_obj['h'])
        channels = int(picked_obj['channels'])
        type = int(picked_obj['type'])
      
        texel_size = channels
        if type==0:
            texel_size *= 4 # float type
        elif type==1:
            texel_size *= 1 # uint8_t type
            pass

        bytes = texel_size * width*height
        inferior = gdb.selected_inferior()
        mem = inferior.read_memory(buffer, bytes)

        plot_thread_instance=threading.Thread(target=plot_thread, args=(mem, args[0], width, height, channels, type))
        plot_thread_instance.daemon=True
        plot_thread_instance.start()
        pass

    pass

PlotterCommand()

