# -*- coding: utf-8 -*-

"""
This module is concerned with the analysis of each variable found by the
debugger, as well as identifying and describing the buffers that should be
plotted in the OpenImageDebugger window.
"""

import re

from oidscripts import symbols
from oidscripts.oidtypes import interface


class EigenXX(interface.TypeInspectorInterface):
    """
    Implementation for inspecting Eigen::Matrix and Eigen::Map
    """
    def _resolve_dynamic_size(self, picked_obj, is_eigen_map, height, width):
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

        return height, width, dynamic_buffer

    def _oid_type_for(self, current_type):
        # Assign the OpenImageDebugger type according to underlying type
        if current_type in ('unsigned char', 'uint8_t'):
            type_value = symbols.OID_TYPES_UINT8
        elif current_type in ('unsigned short', 'uint16_t'):
            type_value = symbols.OID_TYPES_UINT16
        elif current_type in ('short', 'int16_t'):
            type_value = symbols.OID_TYPES_INT16
        elif current_type == 'float':
            type_value = symbols.OID_TYPES_FLOAT32
        elif current_type == 'double':
            type_value = symbols.OID_TYPES_FLOAT64
        elif current_type in ('int', 'int32_t'):
            type_value = symbols.OID_TYPES_INT32
        else:
            # is_symbol_observable() matches any Eigen:: symbol, so
            # unsupported scalar types can reach this point; fail with an
            # actionable message instead of an UnboundLocalError.
            raise ValueError(
                'Unsupported Eigen scalar type: {}'.format(current_type))

        return type_value

    def _get_buffer_pointer(self, debugger_bridge, current_type, picked_obj,
                             is_eigen_map, dynamic_buffer):
        # Differentiate between Map and dynamic/static Matrices
        if is_eigen_map:
            buffer = debugger_bridge.get_casted_pointer(current_type,
                                                        picked_obj['m_data'])
        elif dynamic_buffer:
            # Dynamic storage keeps the heap buffer in its m_data pointer.
            # Pass that pointer directly: GDB's cast of the whole storage
            # struct happens to read its first member (m_data), but LLDB
            # would take the struct's address instead, so name m_data
            # explicitly to get the right pointer on both backends.
            buffer = debugger_bridge.get_casted_pointer(
                current_type, picked_obj['m_storage']['m_data'])
        else:
            buffer = debugger_bridge.get_casted_pointer(
                current_type, picked_obj['m_storage']['m_data']['array'])

        return buffer

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

        height, width, dynamic_buffer = self._resolve_dynamic_size(
            picked_obj, is_eigen_map, height, width)

        if transpose_buffer:
            width, height = height, width

        type_value = self._oid_type_for(current_type)

        buffer = self._get_buffer_pointer(
            debugger_bridge, current_type, picked_obj, is_eigen_map,
            dynamic_buffer)

        # Set row stride and pixel layout
        pixel_layout = 'bgra'

        # The inner stride only exists for Eigen::Map's Stride<> template
        # parameter; a plain matrix has no such argument, so fall back to a
        # dense row stride (== width) instead of failing.
        try:
            eigen_stride = int(
                picked_obj.type.template_argument(2).template_argument(1))
        except (IndexError, TypeError, AttributeError):
            eigen_stride = 0
        row_stride = eigen_stride if eigen_stride > 0 else width

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
