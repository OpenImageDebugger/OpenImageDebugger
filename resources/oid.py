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
    # #region agent log
    import json, threading, traceback
    try:
        with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
            f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H2,H3','location':'oid.py:37','message':'stop_hook_handler_called_UNEXPECTED','data':{'thread':threading.current_thread().name,'stack':traceback.format_stack()[-3:-1]},'timestamp':int(__import__('time').time()*1000)})+'\n')
    except: pass
    # #endregion
    try:
        from oidscripts.debuggers import lldbbridge
        # #region agent log
        try:
            with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H4','location':'oid.py:44','message':'checking_instance','data':{'instance_is_none':lldbbridge.instance is None},'timestamp':int(__import__('time').time()*1000)})+'\n')
        except: pass
        # #endregion
        if lldbbridge.instance is not None:
            # #region agent log
            try:
                with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                    f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H4','location':'oid.py:47','message':'calling_stop_hook','data':{},'timestamp':int(__import__('time').time()*1000)})+'\n')
            except: pass
            # #endregion
            lldbbridge.instance.stop_hook(debugger, command, result, dict)
    except Exception as e:
        # #region agent log
        try:
            with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H2,H3','location':'oid.py:51','message':'stop_hook_exception','data':{'error':str(e),'type':type(e).__name__},'timestamp':int(__import__('time').time()*1000)})+'\n')
        except: pass
        # #endregion
        # Silently ignore errors in stop hook to prevent GIL issues
        # The event loop will still detect frame changes via polling
        pass


def __lldb_init_module(debugger, internal_dict):
    # #region agent log
    import json
    try:
        with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
            f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H1','location':'oid.py:52','message':'lldb_init_module_entry','data':{},'timestamp':int(__import__('time').time()*1000)})+'\n')
    except: pass
    # #endregion
    from oidscripts.debuggers import lldbbridge
    from oidscripts.logger import log

    def ide_prevents_stop_hook():
        from oidscripts.ides import qtcreator
        # #region agent log
        try:
            with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H1','location':'oid.py:58','message':'ide_prevents_check_entry','data':{},'timestamp':int(__import__('time').time()*1000)})+'\n')
        except: pass
        # #endregion
        try:
            from oidscripts.ides import android_studio
            ide_checkers = [qtcreator.prevents_stop_hook,
                           android_studio.prevents_stop_hook]
        except ImportError:
            ide_checkers = [qtcreator.prevents_stop_hook]

        for stop_hook_check in ide_checkers:
            try:
                result = stop_hook_check()
                # #region agent log
                try:
                    with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                        f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H1','location':'oid.py:68','message':'ide_check_result','data':{'checker':stop_hook_check.__module__+'.'+stop_hook_check.__name__,'result':result},'timestamp':int(__import__('time').time()*1000)})+'\n')
                except: pass
                # #endregion
                if result:
                    log.info('Stop hook prevented by IDE detection')
                    return True
            except Exception as e:
                # #region agent log
                try:
                    with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                        f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H1','location':'oid.py:73','message':'ide_check_exception','data':{'error':str(e)},'timestamp':int(__import__('time').time()*1000)})+'\n')
                except: pass
                # #endregion
                # If detection fails, log but continue
                log.debug('IDE detection check failed: %s', str(e))

        # #region agent log
        try:
            with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H1','location':'oid.py:76','message':'ide_prevents_check_false','data':{},'timestamp':int(__import__('time').time()*1000)})+'\n')
        except: pass
        # #endregion
        return False

    # FIXED: Check if IDE hooks will fail before registering stop hook
    # If QtCreator is not available, IDE hooks will fail, and we should prevent
    # stop hooks to avoid GIL issues when they fire from non-main threads
    import lldb
    qtcreator_not_available = not hasattr(lldb, 'theDumper')
    
    prevents = ide_prevents_stop_hook()
    # #region agent log
    try:
        with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
            f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H1,H6','location':'oid.py:139','message':'stop_hook_prevention_decision','data':{'prevents':prevents,'qtcreator_not_available':qtcreator_not_available},'timestamp':int(__import__('time').time()*1000)})+'\n')
    except: pass
    # #endregion
    
    # FIXED: If QtCreator is not available, IDE hooks will fail, so prevent stop hooks
    # to avoid GIL errors. This is a safety measure when running in unsupported IDEs.
    if prevents or qtcreator_not_available:
        # #region agent log
        try:
            with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H1,H6','location':'oid.py:149','message':'preventing_stop_hook','data':{'prevents':prevents,'qtcreator_not_available':qtcreator_not_available,'reason':'qtcreator_not_available' if qtcreator_not_available else 'ide_detection'},'timestamp':int(__import__('time').time()*1000)})+'\n')
        except: pass
        # #endregion
        if qtcreator_not_available and not prevents:
            log.info('Stop hook prevented: QtCreator not available, IDE hooks will fail')
        # #region agent log
        try:
            with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H1,H6','location':'oid.py:155','message':'returning_early_no_stop_hook','data':{'will_not_register':True},'timestamp':int(__import__('time').time()*1000)})+'\n')
        except: pass
        # #endregion
        return
    
    # #region agent log
    try:
        with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
            f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H1','location':'oid.py:163','message':'prevention_check_failed_will_register','data':{'prevents':prevents,'qtcreator_not_available':qtcreator_not_available},'timestamp':int(__import__('time').time()*1000)})+'\n')
    except: pass
    # #endregion

    try:
        # #region agent log
        try:
            with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H1','location':'oid.py:85','message':'registering_stop_hook','data':{},'timestamp':int(__import__('time').time()*1000)})+'\n')
        except: pass
        # #endregion
        debugger.HandleCommand("command script add -f "
                               "oid.lldb_stop_hook_handler "
                               "HandleHookStopOnTarget")
        debugger.HandleCommand('target stop-hook add -o "HandleHookStopOnTarget"')
        # #region agent log
        try:
            with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H1','location':'oid.py:90','message':'stop_hook_registered','data':{},'timestamp':int(__import__('time').time()*1000)})+'\n')
        except: pass
        # #endregion
    except Exception as e:
        # #region agent log
        try:
            with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H1','location':'oid.py:93','message':'stop_hook_registration_failed','data':{'error':str(e)},'timestamp':int(__import__('time').time()*1000)})+'\n')
        except: pass
        # #endregion
        log.warning('Failed to register stop hook: %s', str(e))


def register_ide_hooks(debugger,  # type: BridgeInterface
                       event_handler  # type: OpenImageDebuggerEvents
                       ):
    # type: (...) -> None
    """
    Check if OID was started from an IDE and sets up the required plugins
    """
    # #region agent log
    import json
    try:
        with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
            f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H6','location':'oid.py:88','message':'register_ide_hooks_entry','data':{},'timestamp':int(__import__('time').time()*1000)})+'\n')
    except: pass
    # #endregion
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
            # #region agent log
            try:
                with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                    f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H6','location':'oid.py:109','message':'trying_ide_initializer','data':{'initializer':initializer.__module__+'.'+initializer.__name__},'timestamp':int(__import__('time').time()*1000)})+'\n')
            except: pass
            # #endregion
            initializer(debugger, event_handler)
            # #region agent log
            try:
                with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                    f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H6','location':'oid.py:113','message':'ide_initializer_success','data':{'initializer':initializer.__module__+'.'+initializer.__name__},'timestamp':int(__import__('time').time()*1000)})+'\n')
            except: pass
            # #endregion
            return
        except Exception as e:
            # #region agent log
            try:
                with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                    f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H6','location':'oid.py:118','message':'ide_initializer_failed','data':{'initializer':initializer.__module__+'.'+initializer.__name__,'error':str(e)},'timestamp':int(__import__('time').time()*1000)})+'\n')
            except: pass
            # #endregion
            error_traces.append(traceback.format_exc())

    # #region agent log
    try:
        with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
            f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H6','location':'oid.py:123','message':'all_ide_hooks_failed','data':{},'timestamp':int(__import__('time').time()*1000)})+'\n')
    except: pass
    # #endregion
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
        # #region agent log
        try:
            with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H3','location':'oid.py:293','message':'main_entry_non_test','data':{'thread':__import__('threading').current_thread().name},'timestamp':int(__import__('time').time()*1000)})+'\n')
        except: pass
        # #endregion
        # Setup GDB interface
        debugger = get_debugger_bridge()
        # #region agent log
        try:
            with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H3','location':'oid.py:297','message':'creating_window','data':{},'timestamp':int(__import__('time').time()*1000)})+'\n')
        except: pass
        # #endregion
        window = OpenImageDebuggerWindow(script_path, debugger)
        # #region agent log
        try:
            with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H3','location':'oid.py:300','message':'window_created','data':{},'timestamp':int(__import__('time').time()*1000)})+'\n')
        except: pass
        # #endregion
        
        # FIXED: Initialize window from main thread before event handlers start.
        # This ensures oid_initialize() is called from a thread with proper Python
        # thread state, avoiding GIL errors when called from the event loop thread.
        # #region agent log
        try:
            with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H3','location':'oid.py:307','message':'initializing_window_from_main_thread','data':{},'timestamp':int(__import__('time').time()*1000)})+'\n')
        except: pass
        # #endregion
        window.initialize_window()
        # #region agent log
        try:
            with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H3','location':'oid.py:310','message':'window_initialized_from_main_thread','data':{'is_ready':window.is_ready()},'timestamp':int(__import__('time').time()*1000)})+'\n')
        except: pass
        # #endregion
        
        event_handler = OpenImageDebuggerEvents(window, debugger)

        register_ide_hooks(debugger, event_handler)

        debugger.register_event_handlers(event_handler)
        # #region agent log
        try:
            with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H3','location':'oid.py:320','message':'main_complete','data':{},'timestamp':int(__import__('time').time()*1000)})+'\n')
        except: pass
        # #endregion


main()
