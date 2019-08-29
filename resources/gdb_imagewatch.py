#!/usr/bin/python3

# -*- coding: utf-8 -*-

"""
GDB-ImageWatch entry point. Can be called with --test for opening the watcher
window with a couple of sample buffers; otherwise, should be invoked by the
debugger (GDB).
"""

import argparse
import os
import sys

# TODO there is probably a smarter way to do this
# Add script path to Python PATH so submodules can be found
script_path = os.path.dirname(os.path.realpath(__file__))
sys.path.append(script_path)

# Load dependency modules
from giwscripts.giwwindow import GdbImageWatchWindow
from giwscripts.test import giwtest
from giwscripts.debuggers.interfaces import BridgeInterface
from giwscripts.events import GdbImageWatchEvents


def import_gdb(type_bridge):
    from giwscripts.debuggers import gdbbridge
    return gdbbridge.GdbBridge(type_bridge)


def import_lldb(type_bridge):
    from giwscripts.debuggers import lldbbridge
    return lldbbridge.LldbBridge(type_bridge)


def lldb_stop_hook_handler(debugger, command, result, dict):
    from giwscripts.debuggers import lldbbridge
    lldbbridge.instance.stop_hook(debugger, command, result, dict)


def __lldb_init_module(debugger, internal_dict):
    from giwscripts.debuggers import lldbbridge

    def ide_prevents_stop_hook():
        from giwscripts.ides import qtcreator
        ide_checkers = [qtcreator.prevents_stop_hook]

        for stop_hook_check in ide_checkers:
            if stop_hook_check():
                return True

        return False

    if ide_prevents_stop_hook():
        return

    debugger.HandleCommand("command script add -f "
                           "gdb_imagewatch.lldb_stop_hook_handler "
                           "HandleHookStopOnTarget")
    debugger.HandleCommand('target stop-hook add -o "HandleHookStopOnTarget"')


def register_ide_hooks(debugger,  # type: BridgeInterface
                       event_handler  # type: GdbImageWatchEvents
                       ):
    # type: (...) -> None
    """
    Check if GIW was started from an IDE and sets up the required plugins
    """
    import traceback
    from giwscripts.ides import qtcreator

    ide_initializers = [qtcreator.register_symbol_fetch_hook]
    error_traces = []

    for initializer in ide_initializers:
        try:
            initializer(debugger, event_handler)
            return
        except Exception as err:
            error_traces.append(traceback.format_exc())

    print('[gdb-imagewatch] Info: Could not activate hooks for any IDEs')
    print('\n'.join(error_traces))


def get_debugger_bridge():
    """
    Instantiate the debugger bridge.
    """
    import traceback
    from giwscripts import typebridge

    debugger_bridges = [
        import_lldb,
        import_gdb,
    ]

    type_bridge_object = typebridge.TypeBridge()
    error_traces = []

    for bridge in debugger_bridges:
        try:
            return bridge(type_bridge_object)
        except Exception as err:
            error_traces.append(traceback.format_exc())

    print('[gdb-imagewatch] Error: Could not instantiate any debugger bridge')
    print('\n'.join(error_traces))
    exit(1)


def main():
    """
    Main entry point.
    """
    args = None
    try:
        parser = argparse.ArgumentParser()
        parser.add_argument('--test',
                            help='Open a test window with sample buffers',
                            action='store_true')
        args = parser.parse_args()
    except:
        pass

    if args is not None and args.test:
        # Test application
        giwtest(script_path)
    else:
        # Setup GDB interface
        debugger = get_debugger_bridge()
        window = GdbImageWatchWindow(script_path, debugger)
        event_handler = GdbImageWatchEvents(window, debugger)

        register_ide_hooks(debugger, event_handler)

        debugger.register_event_handlers(event_handler)


main()
