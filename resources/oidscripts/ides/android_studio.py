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

    qtcreator_not_present = not hasattr(lldb, "theDumper")
    has_android_env = (
        os.getenv("ANDROID_STUDIO") is not None
        or os.getenv("ANDROID_HOME") is not None
        or os.getenv("ANDROID_SDK_ROOT") is not None
        or os.getenv("ANDROID_NDK_HOME") is not None
    )

    return qtcreator_not_present and has_android_env


def register_symbol_fetch_hook(debugger, event_handler):
    # type: (BridgeInterface, OpenImageDebuggerEvents) -> None
    """
    Register LLDB event listener for Android Studio integration.

    Note: For LLDB backends, LldbBridge already registers a process listener
    in _check_frame_modification(). This Android Studio integration should NOT
    register a duplicate listener when using LldbBridge, as it would cause
    duplicate stop events. Instead, we rely on LldbBridge's listener which
    properly uses the event queue mechanism.
    """
    import lldb
    import threading
    import time
    from oidscripts.logger import log

    # Check if we're using LldbBridge - if so, skip registering our own listener
    # since LldbBridge already handles process state changes via its own listener
    if debugger.get_backend_name() == "lldb":
        log.info(
            "Android Studio: Using LldbBridge which already handles process listeners, "
            "skipping duplicate listener registration"
        )
        return

    def retry_register_listener():
        """Retry registering the process listener until successful."""
        max_retries = 300  # Try for up to 30 seconds (300 * 0.1s)
        retry_count = 0

        while retry_count < max_retries:
            try:
                lldb_debugger = debugger.get_lldb_backend()
                if not lldb_debugger:
                    time.sleep(0.1)
                    retry_count += 1
                    continue

                target = lldb_debugger.GetSelectedTarget()
                if not target.IsValid():
                    time.sleep(0.1)
                    retry_count += 1
                    continue

                process = target.GetProcess()
                if process.IsValid():
                    # Successfully got a valid process, register listener
                    _register_process_listener(process, event_handler, debugger)
                    log.info(
                        "Android Studio: Process listener registered successfully after %d retries",
                        retry_count,
                    )
                    return
                else:
                    # Process not available yet, keep retrying
                    if retry_count % 50 == 0:  # Log every 5 seconds
                        log.debug(
                            "Android Studio: Process not yet available, retrying... (attempt %d)",
                            retry_count,
                        )
                    time.sleep(0.1)
                    retry_count += 1
            except Exception as e:
                log.debug(
                    "Android Studio: Error during listener registration retry: %s",
                    str(e),
                )
                time.sleep(0.1)
                retry_count += 1

        log.warning(
            "Android Studio: Failed to register process listener after %d retries",
            max_retries,
        )

    # Start retry thread in background
    retry_thread = threading.Thread(target=retry_register_listener, daemon=True)
    retry_thread.start()
    log.info("Android Studio: Started process listener registration retry thread")


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
            log.warning("Android Studio: Process broadcaster is not valid")
            return

        listener = lldb.SBListener("oid-android-studio-listener")

        rc = broadcaster.AddListener(listener, lldb.SBProcess.eBroadcastBitStateChanged)
        if not rc:
            log.warning("Android Studio: Failed to add listener to process broadcaster")
            return

        log.info("Android Studio: Event listener registered successfully")

        def event_monitor_thread():
            import time

            event = lldb.SBEvent()
            last_state = None
            while True:
                try:
                    if listener.WaitForEventForBroadcasterWithType(
                        1, broadcaster, lldb.SBProcess.eBroadcastBitStateChanged, event
                    ):
                        if process.IsValid():
                            state = process.GetState()

                            if (
                                state == lldb.eStateStopped
                                and last_state != lldb.eStateStopped
                            ):
                                log.debug(
                                    "Android Studio: Process stopped, queuing stop handler"
                                )
                                debugger.queue_request(
                                    lambda: event_handler.stop_handler()
                                )
                            last_state = state

                            if (
                                state == lldb.eStateExited
                                or state == lldb.eStateDetached
                            ):
                                break
                    else:
                        if not process.IsValid():
                            break
                except Exception as e:
                    log.debug("Android Studio: Event listener error: %s", str(e))
                    time.sleep(0.1)

        monitor_thread = threading.Thread(target=event_monitor_thread, daemon=True)
        monitor_thread.start()
        log.info("Android Studio: Event monitor thread started")

    except Exception as e:
        log.warning("Android Studio: Failed to register process listener: %s", str(e))
