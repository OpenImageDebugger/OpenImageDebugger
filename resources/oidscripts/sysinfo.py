# -*- coding: utf-8 -*-

"""
System and memory related methods
"""

import subprocess
import re
from sys import platform

from oidscripts import symbols


def _get_available_memory_linux():
    # type: () -> int
    with open('/proc/meminfo', 'r') as mem:
        available_memory = 0
        for i in mem:
            sline = i.split()
            if str(sline[0]) in ('MemFree:', 'Buffers:', 'Cached:'):
                available_memory += int(sline[1]) * 1024
    return available_memory


def _get_available_memory_darwin():
    # type: () -> int
    # Get process info
    ps = subprocess.Popen(['/bin/ps', '-caxm', '-orss,comm'],
                          stdout=subprocess.PIPE).communicate()[0].decode()
    vm = subprocess.Popen(['vm_stat'], stdout=subprocess.PIPE).communicate()[
        0].decode()

    # Iterate processes
    process_lines = ps.split('\n')
    sep = re.compile(r'[\s]+')
    rss_total = 0  # kB
    for row in range(1, len(process_lines)):
        row_text = process_lines[row].strip()
        row_elements = sep.split(row_text)
        try:
            rss = float(row_elements[0]) * 1024
        except:
            rss = 0  # ignore...
        rss_total += rss

    # Process vm_stat
    vm_lines = vm.split('\n')
    sep = re.compile(r':[\s]+')
    vm_stats = {}
    for row in range(1, len(vm_lines) - 2):
        row_text = vm_lines[row].strip()
        row_elements = sep.split(row_text)
        vm_stats[(row_elements[0])] = int(row_elements[1].strip('\.')) * 4096
    return vm_stats["Pages free"]


def get_available_memory():
    """
    Get available memory, in bytes
    """
    if platform == 'linux' or platform == 'linux2':
        return _get_available_memory_linux()
    elif platform == 'darwin':
        return _get_available_memory_darwin()
    else:
        raise Exception('Platform not supported')


def get_buffer_size(height, channels, typevalue, rowstride):
    """
    Compute the buffer size in bytes
    """
    channel_size = 1
    if (typevalue == symbols.OID_TYPES_UINT16 or
            typevalue == symbols.OID_TYPES_INT16):
        channel_size = 2  # 2 bytes per element
    elif (typevalue == symbols.OID_TYPES_INT32 or
          typevalue == symbols.OID_TYPES_FLOAT32):
        channel_size = 4  # 4 bytes per element
    elif typevalue == symbols.OID_TYPES_FLOAT64:
        channel_size = 8  # 8 bytes per element

    return channel_size * channels * rowstride * height
