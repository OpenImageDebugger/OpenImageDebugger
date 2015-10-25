#!/usr/bin/python3

import sys
import ctypes
import os
from ctypes import cdll

script_path = os.path.dirname(os.path.realpath(__file__))
print(script_path)
lib = cdll.LoadLibrary(script_path+'/libwatch.so')
lib.showBin.argtypes = [ctypes.py_object, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int]

"""
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
    lib.showBin(mem, width, height, channels, 0)

    exit()
    pass
"""
"""

import gdb
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
        #buffer = v['data'].cast(char_pointer_type)
        buffer = gdb.parse_and_eval(args[0]+'.ptr()').cast(char_pointer_type)
        width = picked_obj['w']
        height = picked_obj['h']
        channels = picked_obj['channels']
        type = picked_obj['type']
      
        tex_size = channels
        if type==0:
            tex_size *= 4 # float type
        elif type==1:
            tex_size *= 1 # uint8_t type
            pass

        bytes = tex_size * width*height
        inferior = gdb.selected_inferior()
        print(str(width)+', '+str(height)+', '+str(channels))
        mem = inferior.read_memory(buffer, bytes)

        lib.showBin(mem, width, height, channels, type)

        pass

    pass

PlotterCommand()

