# -*- coding: utf-8 -*-

"""
This module is concerned with the analysis of each variable found by the
debugger, as well as identifying and describing the buffers that should be
plotted in the OpenImageDebugger window.
"""

import abc


def debug_buffer_metadata(func):
    def wrapper(self, obj_name, picked_obj, debugger_bridge):
        try:
            # metadata is used for debugging purposed, so we are supressing the next lgtm alert
            metadata = func(self, obj_name, picked_obj, debugger_bridge) # lgtm [py/unused-local-variable]

            print('[%s] [%s] was parsed by oidtype [%s]' %
                  (str(picked_obj.type), obj_name, type(self).__name__))
        except Exception as error:
            print('[%s] [%s] raised exception when parsed by oidtype [%s]:' %
                  (str(picked_obj.type), obj_name, type(self).__name__))
            print('    %s' % str(error))

            raise error

    return wrapper


def debug_symbol_observable(func):
    def wrapper(self, symbol_obj, symbol_name):
        is_observable = func(self, symbol_obj, symbol_name)

        if is_observable:
            is_observable_str = 'is observable'
        else:
            is_observable_str = 'is NOT observable'

        print('[' + str(symbol_obj.type) + '] [' + symbol_name + '] ' +
              is_observable_str + ' by [' + type(self).__name__ + ']')

        return is_observable

    return wrapper


class TypeInspectorInterface(object):
    """
    This interface defines methods to be implemented by type inspectors that
    extract information required for plotting buffers.
    """

    @abc.abstractmethod
    def get_buffer_metadata(self,
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
         oid_plot_buffer in the file $ROOT/src/oid_window.h. The module
         oidtypes.opencv shows an example implementation for the OpenCV Mat
         type.
        """
        pass

    @abc.abstractmethod
    def is_symbol_observable(self, symbol_obj, symbol_name):
        # type: (DebuggerSymbolReference, str) -> bool
        """
        Given the debugger symbol object symbol_obj, and its name
        symbol_name, this method must return True if the symbol corresponds
        to an observable variable (i.e. if its type corresponds to the type
        of the buffers that you want to plot).
        """
        pass
