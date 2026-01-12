# -*- coding: utf-8 -*-

"""
Android Studio integration module.
"""
from oidscripts.debuggers.interfaces import BridgeInterface
from oidscripts.events import OpenImageDebuggerEvents


def prevents_stop_hook():
    # type: () -> bool
    """
    Check if running under Android Studio or another IDE that doesn't support
    QtCreator-style hooks.
    """
    import os
    import lldb

    qtcreator_not_present = not hasattr(lldb, 'theDumper')
    has_android_env = (os.getenv('ANDROID_STUDIO') is not None or
                      os.getenv('ANDROID_HOME') is not None or
                      os.getenv('ANDROID_SDK_ROOT') is not None or
                      os.getenv('ANDROID_NDK_HOME') is not None)

    return qtcreator_not_present and has_android_env


def register_symbol_fetch_hook(debugger, event_handler):
    # type: (BridgeInterface, OpenImageDebuggerEvents) -> None
    """
    Register LLDB event listener for Android Studio integration.
    """
    import lldb
    from oidscripts.logger import log

    try:
        lldb_debugger = debugger.get_lldb_backend()
        if not lldb_debugger:
            log.warning('Android Studio: Could not get LLDB debugger instance')
            return

        target = lldb_debugger.GetSelectedTarget()
        if not target.IsValid():
            log.info('Android Studio: Target not yet available')
            return

        try:
            process = target.GetProcess()
            if process.IsValid():
                _register_process_listener(process, event_handler, debugger)
            else:
                log.info('Android Studio: Process not yet available')
        except Exception as e:
            log.debug('Android Studio: Could not register listener yet: %s', str(e))

        log.info('Android Studio: Event listener registration attempted')
    except Exception as e:
        log.warning('Android Studio: Failed to register event listener: %s', str(e))


def _register_process_listener(process, event_handler, debugger):
    # type: (lldb.SBProcess, OpenImageDebuggerEvents, BridgeInterface) -> None
    """
    Register an event listener for the given process to detect state changes.
    """
    import lldb
    import threading
    from oidscripts.logger import log

    try:
        broadcaster = process.GetBroadcaster()
        if not broadcaster.IsValid():
            log.warning('Android Studio: Process broadcaster is not valid')
            return

        listener = lldb.SBListener('oid-android-studio-listener')

        rc = broadcaster.AddListener(listener,
                                    lldb.SBProcess.eBroadcastBitStateChanged)
        if not rc:
            log.warning('Android Studio: Failed to add listener to process broadcaster')
            return

        log.info('Android Studio: Event listener registered successfully')

        def event_monitor_thread():
            import time
            event = lldb.SBEvent()
            last_state = None
            while True:
                try:
                    if listener.WaitForEventForBroadcasterWithType(1, broadcaster,
                                                                    lldb.SBProcess.eBroadcastBitStateChanged,
                                                                    event):
                        if process.IsValid():
                            state = process.GetState()

                            if state == lldb.eStateStopped and last_state != lldb.eStateStopped:
                                log.debug('Android Studio: Process stopped, queuing stop handler')
                                debugger.queue_request(lambda: event_handler.stop_handler())
                            last_state = state

                            if state == lldb.eStateExited or state == lldb.eStateDetached:
                                break
                    else:
                        if not process.IsValid():
                            break
                except Exception as e:
                    log.debug('Android Studio: Event listener error: %s', str(e))
                    time.sleep(0.1)

        monitor_thread = threading.Thread(target=event_monitor_thread,
                                          daemon=True)
        monitor_thread.start()
        log.info('Android Studio: Event monitor thread started')

    except Exception as e:
        log.warning('Android Studio: Failed to register process listener: %s',
                   str(e))
