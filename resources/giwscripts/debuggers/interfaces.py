# -*- coding: utf-8 -*-
import abc

"""
Interfaces to be implemented by code that shall interact with the debugger
"""


class BridgeInterface(object):
    __metaclass__ = abc.ABCMeta

    """
    This interface defines the methods to be implemented by a debugger bridge
    """

    @abc.abstractmethod
    def queue_request(self, callable_request):
        # type: (Callable[[None],None]) -> None
        """
        Given a callable object 'callable_request', request the debugger
        backend to execute this object in its main execution thread.
        If the debugger backend is thread safe (which is not the case for GDB),
        this may simply execute the 'callable_request' object.
        """
        raise NotImplementedError("Method is not implemented")

    @abc.abstractmethod
    def get_buffer_metadata(self, variable):
        # type: (str) -> dict
        """
        Given a string defining a variable name, must return the following
        information about it:

        type: (str) -> {
            variable_name:str,
            displat_name:str,
            pointer:Union(memoryview, buffer),
            width:int,
            height:int,
            channels:int,
            type:int (see symbols.py),
            row_stride:int,
            pixel_layout:str,
        }
        """
        raise NotImplementedError("Method is not implemented")

    @abc.abstractmethod
    def get_backend_name(self):
        # type: () -> str
        """
        Get the name of the underlying debugger.
        """
        raise NotImplementedError("Method is not implemented")

    @abc.abstractmethod
    def register_event_handlers(self, events):
        """
        Register (callable) listeners to events defined in the dict 'events':
        TODO update this doc. no longer a dict? what are the params for callables? and return types? what are these event handlers? how do they interact with this inteface?
         'stop': When a breakpoint is hit
         exit_handler
         plot_handler
        """
        raise NotImplementedError("Method is not implemented")

    @abc.abstractmethod
    def get_casted_pointer(self, typename, debugger_object):
        """
        Given the string 'typename' specifying any arbitrary type name of the
        program being debugger, and an object referring to a pointer object,
        this function shall return the corresponding object from the underlying
        debugger API corresponding to the cast of the input pointer to a
        pointer of the specified type.
        """
        raise NotImplementedError("Method is not implemented")

    @abc.abstractmethod
    def get_available_symbols(self):
        """
        Get all visible symbols in the current context of debugging.
        """
        raise NotImplementedError("Method is not implemented")


class BridgeEventHandlerInterface(object):
    __metaclass__ = abc.ABCMeta

    """
    This interface defines the events that can be raised by the debugger bridge
    """

    @abc.abstractmethod
    def stop_handler(self, event):
        """
        Handler to be called whenever the debugger stops (e.g. when a
        breakpoint is hit).
        """
        raise NotImplementedError("Method is not implemented")

    @abc.abstractmethod
    def exit_handler(self, event):
        """
        Event raised whenever the inferior has exited.
        """
        raise NotImplementedError("Method is not implemented")

    @abc.abstractmethod
    def refresh_handler(self, event):
        """
        Handler to be called by the IDE whenever the user performs some action
        during a debug session that may require the list of available variables
        to change (such as changing position in the stack).
        """
        raise NotImplementedError("Method is not implemented")

    @abc.abstractmethod
    def plot_handler(self, variable_name):
        """
        Handler to be called whenever the user actively calls the 'plot'
        command from the debugger console.
        """
        raise NotImplementedError("Method is not implemented")


class DebuggerSymbolReference(object):
    __metaclass__ = abc.ABCMeta

    """
    You can interact with fields provided by the debugger bridge through 
    objects that implement this interface.
    """

    @abc.abstractmethod
    def __str__(self):
        # type: () -> str
        """
        :return: Runtime content of the symbol in string format
        """
        raise NotImplementedError("Method is not implemented")

    @abc.abstractmethod
    def __int__(self):
        # type: () -> int
        """
        :return: Runtime content of the symbol in int format
        """
        raise NotImplementedError("Method is not implemented")

    @abc.abstractmethod
    def __float__(self):
        # type: () -> float
        """
        :return: Runtime content of the symbol in float format
        """
        raise NotImplementedError("Method is not implemented")

    @abc.abstractmethod
    def __getitem__(self, member):
        # type: (Union[str, int]) -> DebuggerSymbolReference
        """
        :param member: Member name or index value (in case of arrays)
        :return: Debugger interface for requested member field
        """
        raise NotImplementedError("Method is not implemented")

