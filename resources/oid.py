#!/usr/bin/python3

# -*- coding: utf-8 -*-

"""
Open Image Debugger entry point. Can be called with --test for opening the plugin
window with a couple of sample buffers; otherwise, should be invoked by the
debugger.
"""

import argparse
import os
import sys

# TODO there is probably a smarter way to do this
# Add script path to Python PATH so submodules can be found
script_path = os.path.dirname(os.path.realpath(__file__))
sys.path.append(script_path)

# Load dependency modules
from oidscripts.oidwindow import OpenImageDebuggerWindow
from oidscripts.test import oidtest
from oidscripts.debuggers.interfaces import BridgeInterface
from oidscripts.events import OpenImageDebuggerEvents


def import_gdb(type_bridge):
    from oidscripts.debuggers import gdbbridge
    return gdbbridge.GdbBridge(type_bridge)


def import_lldb(type_bridge):
    from oidscripts.debuggers import lldbbridge
    return lldbbridge.LldbBridge(type_bridge)


def lldb_stop_hook_handler(debugger, command, result, dict):
    from oidscripts.debuggers import lldbbridge
    lldbbridge.instance.stop_hook(debugger, command, result, dict)


def __lldb_init_module(debugger, internal_dict):
    from oidscripts.debuggers import lldbbridge

    def ide_prevents_stop_hook():
        from oidscripts.ides import qtcreator
        ide_checkers = [qtcreator.prevents_stop_hook]

        for stop_hook_check in ide_checkers:
            if stop_hook_check():
                return True

        return False

    if ide_prevents_stop_hook():
        return

    debugger.HandleCommand("command script add -f "
                           "oid.lldb_stop_hook_handler "
                           "HandleHookStopOnTarget")
    debugger.HandleCommand('target stop-hook add -o "HandleHookStopOnTarget"')


def register_ide_hooks(debugger,  # type: BridgeInterface
                       event_handler  # type: OpenImageDebuggerEvents
                       ):
    # type: (...) -> None
    """
    Check if OID was started from an IDE and sets up the required plugins
    """
    import traceback
    from oidscripts.ides import qtcreator

    ide_initializers = [qtcreator.register_symbol_fetch_hook]
    error_traces = []

    for initializer in ide_initializers:
        try:
            initializer(debugger, event_handler)
            return
        except Exception as err:
            error_traces.append(traceback.format_exc())

    print('[OpenImageDebugger] Info: Could not activate hooks for any IDEs')
    # To find out more about this issue, uncomment the line below:
    # print('\n'.join(error_traces))


def get_debugger_bridge():
    """
    Instantiate the debugger bridge.
    """
    import traceback
    from oidscripts import typebridge

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

    print('[OpenImageDebugger] Error: Could not instantiate any debugger bridge')
    print('\n'.join(error_traces))
    exit(1)


def main():
    """
    Main entry point.
    """
    args = None
    parser = argparse.ArgumentParser()
    parser.add_argument('--test',
                        help='Open a test window with sample buffers',
                        action='store_true')
    args = parser.parse_args()

    if args is not None and args.test:
        # Test application
        oidtest(script_path)
    else:
        # Setup GDB interface
        debugger = get_debugger_bridge()
        window = OpenImageDebuggerWindow(script_path, debugger)
        event_handler = OpenImageDebuggerEvents(window, debugger)

        register_ide_hooks(debugger, event_handler)

        debugger.register_event_handlers(event_handler)


main()
