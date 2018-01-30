# -*- coding: utf-8 -*-

"""
System and memory related methods
"""

from giwscripts import symbols


def get_memory_usage():
    """
    Get node total memory and memory usage, in bytes
    """
    with open('/proc/meminfo', 'r') as mem:
        ret = {}
        tmp = 0
        for i in mem:
            sline = i.split()
            if str(sline[0]) == 'MemTotal:':
                ret['total'] = int(sline[1]) * 1024
            elif str(sline[0]) in ('MemFree:', 'Buffers:', 'Cached:'):
                tmp += int(sline[1]) * 1024
        ret['free'] = tmp
        ret['used'] = int(ret['total']) - int(ret['free'])
    return ret


def get_buffer_size(height, channels, typevalue, rowstride):
    """
    Compute the buffer size in bytes
    """
    channel_size = 1
    if (typevalue == symbols.GIW_TYPES_UINT16 or
        typevalue == symbols.GIW_TYPES_INT16):
        channel_size = 2  # 2 bytes per element
    elif (typevalue == symbols.GIW_TYPES_INT32 or
          typevalue == symbols.GIW_TYPES_FLOAT32):
        channel_size = 4  # 4 bytes per element
    elif typevalue == symbols.GIW_TYPES_FLOAT64:
        channel_size = 8  # 8 bytes per element

    return channel_size * channels * rowstride * height
