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
    
    Android Studio uses LLDB but doesn't provide the same integration
    hooks as QtCreator. We prevent stop hooks to avoid GIL issues when
    IDE hooks fail to activate.
    
    Note: This prevents stop hooks, but event listeners (registered via
    register_symbol_fetch_hook()) are still used to detect breakpoint hits
    and trigger buffer updates. Event listeners work better in Android Studio
    than stop hooks.
    
    Detection strategy:
    1. Check if QtCreator's theDumper is NOT present (Android Studio doesn't have it)
    2. Check for Android-related environment variables
    3. If QtCreator is not available, be more aggressive about prevention
    4. Since IDE hooks will fail anyway, prevent stop hooks to avoid GIL issues
    """
    import os
    import lldb
    
    # QtCreator is not present (no theDumper)
    qtcreator_not_present = not hasattr(lldb, 'theDumper')
    
    # Check for Android Studio environment indicators
    # These are commonly set when working with Android development
    has_android_env = (os.getenv('ANDROID_STUDIO') is not None or
                      os.getenv('ANDROID_HOME') is not None or
                      os.getenv('ANDROID_SDK_ROOT') is not None or
                      os.getenv('ANDROID_NDK_HOME') is not None)
    
    # FIXED: More aggressive detection - if QtCreator is not present and we're likely
    # in an IDE (Android env vars set), prevent stop hooks. This prevents GIL errors
    # when IDE hooks fail. For standalone LLDB (no Android env), stop hooks are still
    # allowed since that's a supported use case.
    #
    # The key insight: If QtCreator hooks aren't available AND we have Android indicators,
    # we're in Android Studio and IDE hooks will fail, so we should prevent stop hooks
    # to avoid the GIL error that occurs when stop hooks fire from non-main threads.
    # Event listeners (registered separately) will handle breakpoint detection instead.
    should_prevent = qtcreator_not_present and has_android_env
    
    return should_prevent


def register_symbol_fetch_hook(debugger, event_handler):
    # type: (BridgeInterface, OpenImageDebuggerEvents) -> None
    """
    Register LLDB event listener for Android Studio integration.
    
    Uses LLDB's SBListener API to detect when the process stops (e.g., on breakpoint hit)
    and triggers buffer updates and symbol list refresh. This works in Android Studio
    where QtCreator-style hooks are not available.
    
    Note: The LldbBridge class also has event listener infrastructure that will
    automatically register when the process becomes available. This function
    provides an additional registration attempt for immediate availability.
    """
    import lldb
    from oidscripts.logger import log
    
    try:
        lldb_debugger = debugger.get_lldb_backend()
        if not lldb_debugger:
            log.warning('Android Studio: Could not get LLDB debugger instance')
            return
        
        # Get the target - we'll need to wait for it to be available
        target = lldb_debugger.GetSelectedTarget()
        if not target.IsValid():
            log.info('Android Studio: Target not yet available, LldbBridge will register listener when process starts')
            return
        
        # Try to get process
        try:
            process = target.GetProcess()
            if process.IsValid():
                # Register listener if process is available
                _register_process_listener(process, event_handler, debugger)
            else:
                log.info('Android Studio: Process not yet available, LldbBridge will register listener when process starts')
        except Exception as e:
            log.debug('Android Studio: Could not register listener yet: %s', str(e))
            # LldbBridge will handle registration when process becomes available
        
        log.info('Android Studio: Event listener registration attempted')
    except Exception as e:
        log.warning('Android Studio: Failed to register event listener: %s', str(e))
        # Don't raise - allow graceful fallback to polling and LldbBridge's automatic registration


def _register_process_listener(process, event_handler, debugger):
    # type: (lldb.SBProcess, OpenImageDebuggerEvents, BridgeInterface) -> None
    """
    Register an event listener for the given process to detect state changes.
    """
    import lldb
    import threading
    from oidscripts.logger import log
    
    try:
        # Get the process broadcaster
        broadcaster = process.GetBroadcaster()
        if not broadcaster.IsValid():
            log.warning('Android Studio: Process broadcaster is not valid')
            return
        
        # Create a listener
        listener = lldb.SBListener('oid-android-studio-listener')
        
        # Register for state change events
        rc = broadcaster.AddListener(listener, lldb.SBProcess.eBroadcastBitStateChanged)
        if not rc:
            log.warning('Android Studio: Failed to add listener to process broadcaster')
            return
        
        log.info('Android Studio: Event listener registered successfully')
        
        # Start a background thread to monitor events
        def event_monitor_thread():
            import json
            import time
            event = lldb.SBEvent()
            last_state = None
            event_count = 0
            while True:
                try:
                    # Wait for state change events (timeout: 1 second)
                    if listener.WaitForEventForBroadcasterWithType(
                            1, broadcaster, lldb.SBProcess.eBroadcastBitStateChanged, event):
                        # #region agent log
                        event_count += 1
                        with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                            f.write(json.dumps({"sessionId":"debug-session","runId":"run1","hypothesisId":"H4","location":"android_studio.py:138","message":"android_event_received","data":{"event_count":event_count},"timestamp":int(time.time()*1000)}) + '\n')
                        # #endregion
                        
                        # Check if process is stopped
                        if process.IsValid():
                            state = process.GetState()
                            # #region agent log
                            with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                                f.write(json.dumps({"sessionId":"debug-session","runId":"run1","hypothesisId":"H4","location":"android_studio.py:143","message":"android_state_check","data":{"current_state":state,"last_state":last_state,"transition_detected":state==lldb.eStateStopped and last_state!=lldb.eStateStopped},"timestamp":int(time.time()*1000)}) + '\n')
                            # #endregion
                            
                            # Only queue stop handler when transitioning TO stopped state
                            # not when already stopped (prevents repeated events)
                            if state == lldb.eStateStopped and last_state != lldb.eStateStopped:
                                # Process just stopped (e.g., breakpoint hit)
                                # Queue the stop handler to run on the event loop thread
                                # This avoids GIL issues by using the existing queue mechanism
                                log.debug('Android Studio: Process stopped, queuing stop handler')
                                # #region agent log
                                with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                                    f.write(json.dumps({"sessionId":"debug-session","runId":"run1","hypothesisId":"H4","location":"android_studio.py:151","message":"android_queuing_handler","data":{"event_count":event_count},"timestamp":int(time.time()*1000)}) + '\n')
                                # #endregion
                                debugger.queue_request(lambda: event_handler.stop_handler())
                            last_state = state
                            
                            if state == lldb.eStateExited or state == lldb.eStateDetached:
                                # Process terminated, exit thread
                                break
                    else:
                        # Timeout - check if process is still valid
                        if not process.IsValid():
                            # Process terminated, exit thread
                            break
                        # Do NOT update last_state on timeout - only update on actual events
                        # This prevents false transitions when timeout happens while stopped
                except Exception as e:
                    log.debug('Android Studio: Event listener error: %s', str(e))
                    # Continue monitoring despite errors
                    time.sleep(0.1)
        
        monitor_thread = threading.Thread(target=event_monitor_thread, daemon=True)
        monitor_thread.start()
        log.info('Android Studio: Event monitor thread started')
        
    except Exception as e:
        log.warning('Android Studio: Failed to register process listener: %s', str(e))
