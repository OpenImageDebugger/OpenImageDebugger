# -*- coding: utf-8 -*-

"""
This module is the bridge between all type modules defined in oidtypes and the
debugger. For every symbol found by the debugger, this module queries for a
candidate oidtype and forwards the required calls from the debugger to it.
"""

import importlib
import pkgutil

from oidscripts import oidtypes

from oidscripts.oidtypes.interface import TypeInspectorInterface

from oidscripts.oidtypes import declarative


class TypeBridge(object):
    """
    Class responsible for loading and interfacing with all modules implementing
    the TypeInspectorInterface
    """
    def __init__(self):
        self._type_inspectors = []

        # Import all modules within oidtypes
        for (_, mod_name, _) in pkgutil.iter_modules(oidtypes.__path__):
            importlib.import_module('.oidtypes.' + mod_name, __package__)

        # Registration order is precedence order (first match wins):
        # workspace/user JSON entries, then user Python inspectors, then
        # the builtin JSON entries.
        self._type_inspectors.extend(declarative.load_user_inspectors())

        for inspector_class in TypeInspectorInterface.__subclasses__():
            if inspector_class is declarative.DeclarativeInspector:
                # Instantiated per JSON entry above/below, never bare.
                continue
            self._type_inspectors.append(inspector_class())

        self._type_inspectors.extend(declarative.load_builtin_inspectors())

    def get_buffer_metadata(self, symbol_name, picked_obj, debugger_bridge):
        """
        Returns the metadata related to a variable, which are required for the
        purpose of plotting it in the oidwindow
        """
        for module in self._type_inspectors:
            if module.is_symbol_observable(picked_obj, symbol_name):
                return module.get_buffer_metadata(symbol_name,
                                                  picked_obj,
                                                  debugger_bridge)

        return None

    def is_symbol_observable(self, symbol_obj, symbol_name):
        """
        Returns true if any available module is able to process this particular
        symbol
        """
        for module in self._type_inspectors:
            if module.is_symbol_observable(symbol_obj, symbol_name):
                return True

        return False
