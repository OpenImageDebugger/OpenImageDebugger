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
    should_prevent = qtcreator_not_present and has_android_env
    
    return should_prevent


def register_symbol_fetch_hook(debugger, event_handler):
    # type: (BridgeInterface, OpenImageDebuggerEvents) -> None
    """
    Android Studio doesn't support symbol fetch hooks the same way QtCreator does.
    This is a no-op to allow the hook registration to fail gracefully.
    """
    raise NotImplementedError('Android Studio symbol fetch hooks not implemented')
