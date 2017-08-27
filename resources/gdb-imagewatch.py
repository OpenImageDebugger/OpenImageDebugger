#!/usr/bin/python3

# -*- coding: utf-8 -*-

"""
GDB-ImageWatch entry point. Can be called with --test for opening the watcher
window with a couple of sample buffers; otherwise, should be invoked by the
debugger (GDB).
"""

import sys
import argparse

import os


def get_debugger_bridge():
    """
    Instantiate the debugger bridge. If we ever decide to support other
    debuggers, this will be the place to instantiate its bridge object.
    """
    from giwscripts.debuggers import gdbbridge
    from giwscripts import giwtype

    try:
        return gdbbridge.GdbBridge(giwtype)
    except Exception as err:
        print(err)

    print('[gdb-imagewatch] Error: Could not instantiate any debugger bridge')
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
    from giwscripts.ides import qtcreator

    parser = argparse.ArgumentParser()
    parser.add_argument('--test',
                        help='Open a test window with sample buffers',
                        action='store_true')
    args = parser.parse_args()

    if args.test:
        # Test application
        test.giwtest(script_path)
    else:
        # Setup GDB interface
        debugger = get_debugger_bridge()
        window = giwwindow.GdbImageWatchWindow(script_path, debugger)
        event_handler = events.GdbImageWatchEvents(window, debugger)

        if not qtcreator.register_symbol_fetch_hook(event_handler.stop_handler):
            debugger.register_event_handlers(event_handler)


if __name__ == "__main__":
    main()
