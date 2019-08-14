# -*- coding: utf-8 -*-

"""
QtCreator integration module.
"""


def register_symbol_fetch_hook(retrieve_symbols_callback):
    # type: (Callable[[None],None]) -> None
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
        event handler 'stop' method everytime 'fetchVariables' is called.
        """
        ret = original_fetch_variables(self, args)

        retrieve_symbols_callback(None)

        return ret

    original_fetch_variables = imp.Dumper.fetchVariables
    imp.Dumper.fetchVariables = fetch_variables_wrapper

