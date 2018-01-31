# -*- coding: utf-8 -*-

"""
This module is concerned with the analysis of each variable found by the
debugger, as well as identifying and describing the buffers that should be
plotted in the ImageWatch window.
"""

import re

from giwscripts import symbols


# OpenCV constants
CV_CN_MAX = 512
CV_CN_SHIFT = 3
CV_MAT_CN_MASK = ((CV_CN_MAX - 1) << CV_CN_SHIFT)
CV_DEPTH_MAX = (1 << CV_CN_SHIFT)
CV_MAT_TYPE_MASK = (CV_DEPTH_MAX * CV_CN_MAX - 1)


def get_buffer_metadata(obj_name, picked_obj, debugger_bridge):
    """
    Default values created for OpenCV Mat structures. Change it according to
    your needs.
    """
    buffer = debugger_bridge.get_casted_pointer('char', picked_obj['data'])
    if buffer == 0x0:
        raise Exception('Received null buffer!')

    width = int(picked_obj['cols'])
    height = int(picked_obj['rows'])
    if str(picked_obj.type) == "cv::Mat":
        flags = int(picked_obj['flags']) # cv::Mat
    else:
        flags = int(picked_obj['type']) # CvMat

    channels = ((((flags) & CV_MAT_CN_MASK) >> CV_CN_SHIFT) + 1)
    if str(picked_obj.type) == "cv::Mat":
        row_stride = int(int(picked_obj['step']['buf'][0])/channels) # cv::Mat
    else:
        row_stride = int(int(picked_obj['step'])/channels) # CvMat

    if channels >= 3:
        pixel_layout = 'bgra'
    else:
        pixel_layout = 'rgba'

    cvtype = ((flags) & CV_MAT_TYPE_MASK)

    type_value = (cvtype & 7)

    if (type_value == symbols.GIW_TYPES_UINT16 or
        type_value == symbols.GIW_TYPES_INT16):
        row_stride = int(row_stride / 2)
    elif (type_value == symbols.GIW_TYPES_INT32 or
          type_value == symbols.GIW_TYPES_FLOAT32):
        row_stride = int(row_stride / 4)
    elif type_value == symbols.GIW_TYPES_FLOAT64:
        row_stride = int(row_stride / 8)

    return {
        'display_name': str(picked_obj.type) + ' ' + obj_name,
        'pointer': buffer,
        'width': width,
        'height': height,
        'channels': channels,
        'type': type_value,
        'row_stride': row_stride,
        'pixel_layout': pixel_layout
    }


def is_symbol_observable(symbol):
    """
    Returns true if the given symbol is of observable type (the type of the
    buffer you are working with). Make sure to check for pointers of your type
    as well
    """
    # Check if symbol type is the expected buffer
    symbol_type = str(symbol.type)
    type_regex = r'(const\s+)?cv::Mat(\s+?[*&])?'
    type2_regex = r'(const\s+)?CvMat(\s+?[*&])?'
    return re.match(type_regex, symbol_type) is not None or re.match(type2_regex, symbol_type) is not None
