# -*- coding: utf-8 -*-

"""
This module is concerned with the analysis of each variable found by the
debugger, as well as identifying and describing the buffers that should be
plotted in the ImageWatch window.
"""

import abc

from giwscripts import debuggers


class TypeInspectorInterface(object):
    """
    This interface defines methods to be implemented by type inspectors that
    extract information required for plotting buffers.
    """

    @abc.abstractmethod
    def get_buffer_metadata(self,  # type: TypeInspectorInterface
                            obj_name,  # type: str
                            picked_obj,  # type: DebuggerSymbolReference
                            debugger_bridge  # type: BridgeInterface
                            ):
        # type: (...) -> dict
        """
        Given the variable name obj_name, the debugger symbol object
        picked_obj and the debugger interface object debugger_bridge,
        this method must return a dictionary containing the following fields:

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
    def is_symbol_observable(self, symbol_obj, symbol_name):
        # type: (TypeInspectorInterface, DebuggerSymbolReference, str) -> bool
        """
        Given the debugger symbol object symbol_obj, and its name
        symbol_name, this method must return True if the symbol corresponds
        to an observable variable (i.e. if its type corresponds to the type
        of the buffers that you want to plot).
        """
        pass
