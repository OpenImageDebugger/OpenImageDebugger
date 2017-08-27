import math
import numpy
import time

from giwscripts import giwwindow
from giwscripts import symbols

def giwtest():

    window = giwwindow.GdbImageWatchWindow(None)
    window.initialize_window()

    width=400
    height=200
    channels1=3
    channels2=1
    tex = [None]*width*height*channels1
    tex2 = [None]*width*height*channels2

    def genColor(x, y, Ka, Kb, Fa, Fb):
        return ((Fa(x * Fb(y/Ka)/Kb)+1)*255/2)

    for y in range(0,height):
        for x in range(0,width):
            tex[y*channels1*width+channels1*x] = genColor(x, y, 20, 80, math.cos, math.cos)
            tex[y*channels1*width+channels1*x + 1] = genColor(x, y, 50, 200, math.sin, math.cos)
            tex[y*channels1*width+channels1*x + 2] = genColor(x, y, 30, 120, math.cos, math.cos)

            for c in range(0, channels2):
                tex2[y*channels2*width+channels2*x+c] = math.exp(math.cos(x/5.0) *
                                                                 math.sin(y/5.0))
                pass
            pass
        pass

    tex_arr = numpy.asarray(tex, numpy.uint8)
    tex_arr2 = numpy.asarray(tex2, numpy.float32)
    mem = memoryview(tex_arr)
    mem2 = memoryview(tex_arr2)
    step = width
    window._lib.plot_binary(mem, 'python_test', width, height, channels1, symbols.GIW_TYPES_UINT8, step, 'rgba')
    window._lib.plot_binary(mem2, 'python_test2', width, height, channels2, symbols.GIW_TYPES_FLOAT32, step, 'rgba')


    while window._lib.is_running():
        time.sleep(0.5)

    window._lib.terminate()
    exit()
    pass
