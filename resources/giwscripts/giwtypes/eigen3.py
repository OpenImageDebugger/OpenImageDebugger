# -*- coding: utf-8 -*-

"""
This module is concerned with the analysis of each variable found by the
debugger, as well as identifying and describing the buffers that should be
plotted in the ImageWatch window.
"""

import re

from giwscripts import symbols
from giwscripts.giwtypes import interface


class EigenXX(interface.TypeInspectorInterface):
    """
    Implementation for inspecting Eigen::Matrix and Eigen::Map
    """
    def get_buffer_metadata(self, obj_name, picked_obj, debugger_bridge):
        """
        Gets the buffer meta data from types of the Eigen Library
        Note that it only implements single channel matrix display,
        which should be quite common in Eigen.
        """

        type_str = str(picked_obj.type)
        is_eigen_map = 'Map' in type_str
        # First we need the python object for the actual matrix type. When
        # parsing a Map, the type is the first template parameter of the
        # wrapper type. Otherwise, it is the type field of the picked_obj
        if is_eigen_map:
            matrix_type_obj = picked_obj.type.template_argument(0)
        else:
            matrix_type_obj = picked_obj.type

        current_type = str(matrix_type_obj.template_argument(0))
        height = int(matrix_type_obj.template_argument(1))
        width = int(matrix_type_obj.template_argument(2))
        matrix_flag = int(matrix_type_obj.template_argument(3))
        transpose_buffer = ((matrix_flag&0x1) == 0)
        dynamic_buffer = False

        if height <= 0:
            # Buffer has dynamic width
            if is_eigen_map:
                height = int(picked_obj['m_rows']['m_value'])
            else:
                height = int(picked_obj['m_storage']['m_rows'])
            dynamic_buffer = True

        if width <= 0:
            # Buffer has dynamic height
            if is_eigen_map:
                width = int(picked_obj['m_cols']['m_value'])
            else:
                width = int(picked_obj['m_storage']['m_cols'])
            dynamic_buffer = True

        if transpose_buffer:
            width, height = height, width

        # Assign the GIW type according to underlying type
        if current_type == 'short':
            type_value = symbols.GIW_TYPES_INT16
        elif current_type == 'float':
            type_value = symbols.GIW_TYPES_FLOAT32
        elif current_type == 'double':
            type_value = symbols.GIW_TYPES_FLOAT64
        elif current_type == 'int':
            type_value = symbols.GIW_TYPES_INT32

        # Differentiate between Map and dynamic/static Matrices
        if is_eigen_map:
            buffer = debugger_bridge.get_casted_pointer(current_type,
                                                        picked_obj['m_data'])
        elif dynamic_buffer:
            buffer = debugger_bridge.get_casted_pointer(
                current_type, picked_obj['m_storage'])
        else:
            buffer = debugger_bridge.get_casted_pointer(
                current_type, picked_obj['m_storage']['m_data']['array'])

        if buffer == 0x0:
            raise Exception('Received null buffer!')

        # Set row stride and pixel layout
        pixel_layout = 'bgra'
        row_stride = width

        return {
            'display_name': obj_name + ' (' + str(matrix_type_obj) + ')',
            'pointer': buffer,
            'width': width,
            'height': height,
            'channels': 1,
            'type': type_value,
            'row_stride': row_stride,
            'pixel_layout': pixel_layout,
            'transpose_buffer': transpose_buffer
        }

    def is_symbol_observable(self, symbol, symbol_name):
        """
        Returns true if the given symbol is of observable type (the type of the
        buffer you are working with). Make sure to check for pointers of your
        type as well
        """
        # Check if symbol type is the expected buffer
        symbol_type = str(symbol.type)
        type_regex = r'(const\s+)?Eigen::(\s+?[*&])?'
        return re.match(type_regex, symbol_type) is not None
