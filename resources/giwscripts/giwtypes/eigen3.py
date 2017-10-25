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

        # first we need to parse the underlying type and size
        # from our picked_ojb type name
        type_and_size_expression = picked_obj.type.name
        m = re.search('(float|short|double|int), (\d+), (\d+), (\d+)',type_and_size_expression)
        if m:
            current_type = m.group(1)
            height = int(m.group(2))
            width = int(m.group(3))
            is_row_major = int(m.group(4))
        else:
            raise Exception('Invalid or not implemented type!')

        if not is_row_major:
            raise Exception('You are trying to display a col major matrix. This is not supported')

        # assing the GIW type according to underlying type
        if current_type == 'short':
            type_value = symbols.GIW_TYPES_INT16
        elif current_type == 'float':
            type_value = symbols.GIW_TYPES_FLOAT32
        elif current_type == 'double':
            type_value = symbols.GIW_TYPES_FLOAT64
        elif current_type == 'int':
            type_value = symbols.GIW_TYPES_INT32

        # differentiate between Map and Matrix in handling
        if 'Map' in str(picked_obj.type):

            buffer = debugger_bridge.get_casted_pointer(current_type, picked_obj['m_data'])
        else:
            buffer = debugger_bridge.get_casted_pointer(current_type, picked_obj['m_storage'].address)


        if buffer == 0x0:
            raise Exception('Received null buffer!')


        # set row stride and pixel layout
        # Note that Eigen::Matrix is ColMajor by default
        # Therefore you may observe transposed matrices
        pixel_layout = 'bgra'
        row_stride = width

        return {
            'display_name': str(picked_obj.type) + ' ' + obj_name,
            'pointer': buffer,
            'width': width,
            'height': height,
            'channels': 1,
            'type': type_value,
            'row_stride': row_stride,
            'pixel_layout': pixel_layout
        }

    def is_symbol_observable(self, symbol):
        """
        Returns true if the given symbol is of observable type (the type of the
        buffer you are working with). Make sure to check for pointers of your type
        as well
        """
        # Check if symbol type is the expected buffer
        symbol_type = str(symbol.type)
        type_regex = r'(const\s+)?Eigen::(\s+?[*&])?'
        return re.match(type_regex, symbol_type) is not None

