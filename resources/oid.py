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
from oidscripts.logger import log

def import_gdb(type_bridge):
    from oidscripts.debuggers import gdbbridge
    return gdbbridge.GdbBridge(type_bridge)

def import_lldb(type_bridge):
    from oidscripts.debuggers import lldbbridge
    return lldbbridge.LldbBridge(type_bridge)

def lldb_stop_hook_handler(debugger, command, result, dict):
    """
    LLDB stop hook handler. This is called when the debugger stops.
    We need to ensure proper GIL management to avoid mutex lock failures.
    
    WARNING: This should NOT be called if the fix is working correctly.
    If this is called, it means the stop hook was registered despite prevention.
    """
    try:
        from oidscripts.debuggers import lldbbridge
        if lldbbridge.instance is not None:
            lldbbridge.instance.stop_hook(debugger, command, result, dict)
    except Exception as e:
        # Silently ignore errors in stop hook to prevent GIL issues
        # The event loop will still detect frame changes via polling
        pass

def __lldb_init_module(debugger, internal_dict):
    from oidscripts.debuggers import lldbbridge
    from oidscripts.logger import log

    def ide_prevents_stop_hook():
        from oidscripts.ides import qtcreator
        try:
            from oidscripts.ides import android_studio
            ide_checkers = [qtcreator.prevents_stop_hook,
                           android_studio.prevents_stop_hook]
        except ImportError:
            ide_checkers = [qtcreator.prevents_stop_hook]

        for stop_hook_check in ide_checkers:
            try:
                result = stop_hook_check()
                if result:
                    log.info('Stop hook prevented by IDE detection')
                    return True
            except Exception as e:
                # If detection fails, log but continue
                log.debug('IDE detection check failed: %s', str(e))

        return False

    # FIXED: Check if IDE hooks will fail before registering stop hook
    # If QtCreator is not available, IDE hooks will fail, and we should prevent
    # stop hooks to avoid GIL issues when they fire from non-main threads
    import lldb
    qtcreator_not_available = not hasattr(lldb, 'theDumper')
    
    prevents = ide_prevents_stop_hook()
    
    # FIXED: If QtCreator is not available, IDE hooks will fail, so prevent stop hooks
    # to avoid GIL errors. This is a safety measure when running in unsupported IDEs.
    if prevents or qtcreator_not_available:
        if qtcreator_not_available and not prevents:
            log.info('Stop hook prevented: QtCreator not available, IDE hooks will fail')
        return

    try:
        debugger.HandleCommand("command script add -f "
                               "oid.lldb_stop_hook_handler "
                               "HandleHookStopOnTarget")
        debugger.HandleCommand('target stop-hook add -o "HandleHookStopOnTarget"')
    except Exception as e:
        log.warning('Failed to register stop hook: %s', str(e))

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
    
    # Try to add Android Studio initializer if available
    try:
        from oidscripts.ides import android_studio
        ide_initializers.append(android_studio.register_symbol_fetch_hook)
    except ImportError:
        pass
    
    error_traces = []

    for initializer in ide_initializers:
        try:
            initializer(debugger, event_handler)
            return
        except Exception as e:
            error_traces.append(traceback.format_exc())

    log.info('Could not activate hooks for any IDEs')
    # To find out more about this issue, uncomment the line below:
    # log.info('\n'.join(error_traces))

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
        except Exception:
            error_traces.append(traceback.format_exc())

    log.error('Could not instantiate any debugger bridge')
    log.error('\n'.join(error_traces))
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
        # 
        # Setup GDB interface
        debugger = get_debugger_bridge()
        # 
        window = OpenImageDebuggerWindow(script_path, debugger)
        # 
        
        # FIXED: Initialize window from main thread before event handlers start.
        # This ensures oid_initialize() is called from a thread with proper Python
        # thread state, avoiding GIL errors when called from the event loop thread.
        window.initialize_window()
        
        event_handler = OpenImageDebuggerEvents(window, debugger)

        register_ide_hooks(debugger, event_handler)

        debugger.register_event_handlers(event_handler)

main()
