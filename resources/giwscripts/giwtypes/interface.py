# -*- coding: utf-8 -*-

"""
This module is concerned with the analysis of each variable found by the
debugger, as well as identifying and describing the buffers that should be
plotted in the ImageWatch window.
"""

import abc


class TypeInspectorInterface():
    """
    This interface defines methods to be implemented by type inspectors that
    extract information required for plotting buffers.
    """
    @abc.abstractmethod
    def get_buffer_metadata(self, obj_name, picked_obj, debugger_bridge):
        """
        Given the variable name obj_name (str), the debugger object with its
        internl fields and type picked_obj and the debugger bridge object
        debugger_bridge (which implement the methods in
        giwscripts.debuggers.interface.BridgeInterface), this method must
        return a dict containing the following fields:

         * display_name
         * pointer
         * width
         * height
         * channels
         * type
         * row_stride
         * pixel_layout
         * transpose_buffer

         For information about these fields, consult the documentation for
         giw_plot_buffer in the file $ROOT/src/giw_window.h. The module
         giwtypes.opencv shows an example implementation for the OpenCV Mat
         type.
        """
        pass

    @abc.abstractmethod
    def is_symbol_observable(self, symbol_obj):
        """
        Given the debugger with its internal fields and type symbol_obj, this
        method must return True if symbol_obj corresponds to an observable
        variable (i.e. if its type corresponds to the type of the buffers that
        you want to plot).
        """
        pass
