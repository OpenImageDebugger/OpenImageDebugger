# -*- coding: utf-8 -*-

"""
QtCreator integration module.
"""
from giwscripts.debuggers.interfaces import BridgeInterface
from giwscripts.events import GdbImageWatchEvents


def gdb_fetch_hook(event_handler):
    # type: (GdbImageWatchEvents) -> None
    """
    Hacks into Dumper and changes its fetchVariables method to a wrapper that
    calls stop_event_handler. This allows us to retrieve the list of locals
    every time the user changes the local stack frame during a debug session
    from QtCreator interface.
    """
    imp = __import__('gdbbridge')

    original_fetch_variables = None

    def fetch_variables_wrapper(self, args):
        """
        Acts as a proxy to QTCreator 'fetchVariables' method, calling the
        event handler 'stop' method every time 'fetchVariables' is called.
        """
        ret = original_fetch_variables(self, args)

        event_handler.stop_handler(None)

        return ret

    original_fetch_variables = imp.Dumper.fetchVariables
    imp.Dumper.fetchVariables = fetch_variables_wrapper


def lldb_fetch_hook(debugger, event_handler):
    # type: (BridgeInterface, GdbImageWatchEvents) -> None
    import lldb

    imp = __import__('lldbbridge')
    original_handle_event = imp.Dumper.handleEvent

    def get_lldb_backend(*args):
        return lldb.theDumper.debugger

    def handle_event_wrapper(self, args):
        event_handler.stop_handler()
        original_handle_event(self, args)

    debugger.get_lldb_backend = get_lldb_backend
    imp.Dumper.handleEvent = handle_event_wrapper


def prevents_stop_hook():
    # type: () -> bool
    import lldb
    is_running_from_qtcreator = hasattr(lldb, 'theDumper')
    return is_running_from_qtcreator


def register_symbol_fetch_hook(debugger, event_handler):
    # type: (BridgeInterface, GdbImageWatchEvents) -> None
    backend_name = debugger.get_backend_name()
    if backend_name == 'gdb':
        gdb_fetch_hook(event_handler)
    elif backend_name == 'lldb':
        lldb_fetch_hook(debugger, event_handler)
    else:
        raise Exception('Invalid debugger provided to qtcreator integration module')
