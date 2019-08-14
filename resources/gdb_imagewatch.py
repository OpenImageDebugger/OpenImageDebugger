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
    debugger.HandleCommand("command script add -f gdb_imagewatch.lldb_stop_hook_handler HandleHookStopOnTarget")
    debugger.HandleCommand('target stop-hook add -o "HandleHookStopOnTarget"')


def register_ide_hooks(event_handler):
    # type: (giwscripts.events.GdbImageWatchEvents) -> None
    # TODO my __init__.py should probably contain references to the scripts for the above to work
    import traceback
    from giwscripts.ides import qtcreator

    ide_initializers = [qtcreator.register_symbol_fetch_hook]
    error_traces = []

    for initializer in ide_initializers:
        try:
            initializer(event_handler.refresh_handler)
            return
        except Exception as err:
            error_traces.append(traceback.format_exc())

    print('[gdb-imagewatch] Info: Could not activate hooks for any IDEs')
    print('\n'.join(error_traces))


def get_debugger_bridge():
    """
    Instantiate the debugger bridge. If we ever decide to support other
    debuggers, this will be the place to instantiate its bridge object.
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
    # Add script path to Python PATH so submodules can be found
    script_path = os.path.dirname(os.path.realpath(__file__))
    sys.path.append(script_path)

    # Load dependency modules
    from giwscripts import events
    from giwscripts import giwwindow
    from giwscripts import test

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
        test.giwtest(script_path)
    else:
        # Setup GDB interface
        debugger = get_debugger_bridge()
        window = giwwindow.GdbImageWatchWindow(script_path, debugger)
        event_handler = events.GdbImageWatchEvents(window, debugger)

        register_ide_hooks(event_handler)

        debugger.register_event_handlers(event_handler)


main()
