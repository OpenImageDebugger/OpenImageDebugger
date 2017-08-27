#!/usr/bin/python3

import sys
import argparse

# Add script path to Python PATH so submodules can be found
import os, sys
sys.path.append(os.path.dirname(os.path.realpath(__file__)))

from giwscripts import events
from giwscripts import giwtype
from giwscripts import giwwindow
from giwscripts import test
from giwscripts.ides import qtcreator

def get_debugger_bridge():
    # This must be imported here, or the --test mode won't work
    from giwscripts.debuggers import gdbbridge
    try:
        return gdbbridge.GdbBridge(giwtype)
    except Exception as err:
        print(err)
        pass

    print('[gdb-imagewatch] Error: Could not instantiate any debugger bridge')
    exit(1)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--test',
                        help='Open a test window with sample buffers',
                        action='store_true')
    args = parser.parse_args()

    if args.test:
        # Test application
        test.giwtest()
    else:
        # Setup GDB interface
        debugger = get_debugger_bridge()
        window = giwwindow.GdbImageWatchWindow(debugger)
        event_handler = events.GdbImageWatchEvents(window, debugger)

        if not qtcreator.registerSymbolFetchHook(event_handler.stop_handler):
            debugger.register_event_handlers(event_handler)

if __name__ == "__main__":
    main()
