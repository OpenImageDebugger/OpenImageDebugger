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
    vm = subprocess.Popen(['vm_stat'], stdout=subprocess.PIPE).communicate()[
        0].decode()

    # Process vm_stat
    vm_lines = vm.split('\n')
    sep = re.compile(r':[\s]+')
    vm_stats = {}

    psre = 'page size of ([0-9]+) bytes'
    match = re.search(psre, vm_lines[0])
    page_size = int(match.group(1)) if match else 16384

    for row in range(1, len(vm_lines) - 2):
        row_text = vm_lines[row].strip()
        row_elements = sep.split(row_text)
        vm_stats[(row_elements[0])] = int(row_elements[1].strip('.')) * page_size
    return vm_stats["Pages free"]

def _get_available_memory_win32():
    # type: () -> int

    # We are using windll from ctypes
    import ctypes

    # Binding return struct from GlobalMemoryStatusEx
    class MEMORYSTATUSEX(ctypes.Structure):
        # dwLength should be set to the size of the struct
        # dwMemoryLoad is in %
        # The remaining members are in bytes
        _fields_ = [
            ("dwLength", ctypes.c_ulong),
            ("dwMemoryLoad", ctypes.c_ulong),
            ("ullTotalPhys", ctypes.c_ulonglong),
            ("ullAvailPhys", ctypes.c_ulonglong),
            ("ullTotalPageFile", ctypes.c_ulonglong),
            ("ullAvailPageFile", ctypes.c_ulonglong),
            ("ullTotalVirtual", ctypes.c_ulonglong),
            ("ullAvailVirtual", ctypes.c_ulonglong),
            ("sullAvailExtendedVirtual", ctypes.c_ulonglong),
        ]

        def __init__(self):
            self.dwLength = ctypes.sizeof(self)
            super(MEMORYSTATUSEX, self).__init__()

    stat = MEMORYSTATUSEX()
    # TODO: add a try-except block
    ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(stat))
    return int(stat.ullAvailPhys)

def get_available_memory():
    """
    Get available memory, in bytes
    """
    if platform == 'linux' or platform == 'linux2':
        return _get_available_memory_linux()
    elif platform == 'darwin':
        return _get_available_memory_darwin()
    elif platform == 'win32':
        return _get_available_memory_win32()
    else:
        raise ValueError('Platform %s not supported' % platform)


def get_type_name(typevalue):
    """
    Get the human-readable name of matrix type
    """
    if (typevalue == symbols.OID_TYPES_UINT8):
        return '8U'
    if (typevalue == symbols.OID_TYPES_UINT16):
        return '16U'
    if (typevalue == symbols.OID_TYPES_INT16):
        return '16S'
    if (typevalue == symbols.OID_TYPES_INT32):
        return '32S'
    if (typevalue == symbols.OID_TYPES_FLOAT32):
        return '32F'
    if typevalue == symbols.OID_TYPES_FLOAT64:
        return '64F'

    return '?'


def get_type_name_full(channels, typevalue):
    """
    Get the human-readable name of matrix type
    """
    return "%sC%d" % (get_type_name(typevalue), channels)


def get_channel_size(typevalue):
    """
    Compute the channel size in bytes
    """
    if (typevalue == symbols.OID_TYPES_UINT16 or
        typevalue == symbols.OID_TYPES_INT16):
        return 2  # 2 bytes per element
    if (typevalue == symbols.OID_TYPES_INT32 or
        typevalue == symbols.OID_TYPES_FLOAT32):
        return 4  # 4 bytes per element
    if typevalue == symbols.OID_TYPES_FLOAT64:
        return 8  # 8 bytes per element
    
    return 1


def get_buffer_size(height, channels, typevalue, rowstride):
    """
    Compute the buffer size in bytes
    """
    channel_size = get_channel_size(typevalue)
    return channel_size * channels * rowstride * height
