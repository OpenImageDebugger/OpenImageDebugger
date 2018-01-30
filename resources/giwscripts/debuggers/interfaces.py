# -*- coding: utf-8 -*-

"""
Interfaces to be implemented by code that shall interact with the debugger
"""


class BridgeInterface():
    """
    This interface defines the methods to be implemented by a debugger bridge
    """
    def queue_request(self, callable_request):
        """
        Given a callable object 'callable_request', request the debugger
        backend to execute this object in its main execution thread.
        If the debugger backend is thread safe (which is not the case for GDB),
        this may simply execute the 'callable_request' object.
        """
        raise NotImplementedError("Method is not implemented")

    def get_buffer_metadata(self, variable):
        """
        Given a string defining a variable name, must return the following
        information about it:

        [mem, width, height, channels, type, step, pixel_layout]
        """
        raise NotImplementedError("Method is not implemented")

    def register_event_handlers(self, events):
        """
        Register (callable) listeners to events defined in the dict 'events':
         'stop': When a breakpoint is hit
        """
        raise NotImplementedError("Method is not implemented")

    def get_casted_pointer(self, typename, debugger_object):
        """
        Given the string 'typename' specifying any arbitrary type name of the
        program being debugger, and an object referring to a pointer object,
        this function shall return the corresponding object from the underlying
        debugger API corresponding to the cast of the input pointer to a
        pointer of the specified type.
        """
        raise NotImplementedError("Method is not implemented")

    def get_available_symbols(self):
        """
        Get all visible symbols in the current context of debugging.
        """
        raise NotImplementedError("Method is not implemented")


class BridgeEventHandlerInterface():
    """
    This interface defines the events that can be raised by the debugger bridge
    """
    def stop_handler(self, event):
        """
        Handler to be called whenever the debugger stops (e.g. when a
        breakpoint is hit).
        """
        raise NotImplementedError("Method is not implemented")

    def exit_handler(self, event):
        """
        Event raised whenever the inferior has exited.
        """
        raise NotImplementedError("Method is not implemented")

    def refresh_handler(self, event):
        """
        Handler to be called by the IDE whenever the user performs some action
        during a debug session that may require the list of available variables
        to change (such as changing position in the stack).
        """
        raise NotImplementedError("Method is not implemented")

    def plot_handler(self, variable_name):
        """
        Handler to be called whenever the user actively calls the 'plot'
        command from the debugger console.
        """
        raise NotImplementedError("Method is not implemented")
