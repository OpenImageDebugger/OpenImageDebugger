# -*- coding: utf-8 -*-
import abc

__not_implemented_error = NotImplementedError("Method is not implemented")

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
        raise __not_implemented_error

    @abc.abstractmethod
    def get_buffer_metadata(self, variable):
        # type: (str) -> dict
        """
        Given a string defining a variable name, must return the following
        information about it:

        type: (str) -> {
            variable_name:str,
            display_name:str,
            pointer:Union(memoryview, buffer),
            width:int,
            height:int,
            channels:int,
            type:int (see symbols.py),
            row_stride:int,
            pixel_layout:str,
        }
        """
        raise __not_implemented_error

    @abc.abstractmethod
    def get_backend_name(self):
        # type: () -> str
        """
        Get the name of the underlying debugger.
        """
        raise __not_implemented_error

    @abc.abstractmethod
    def register_event_handlers(self, events):
        """
        Register (callable) listeners to events defined in the dict 'events':
        TODO update this doc. no longer a dict? what are the params for callables? and return types? what are these event handlers? how do they interact with this inteface?
         'stop': When a breakpoint is hit
         exit_handler
         plot_handler
        """
        raise __not_implemented_error

    @abc.abstractmethod
    def get_casted_pointer(self, typename, debugger_object):
        """
        Given the string 'typename' specifying any arbitrary type name of the
        program being debugger, and an object referring to a pointer object,
        this function shall return the corresponding object from the underlying
        debugger API corresponding to the cast of the input pointer to a
        pointer of the specified type.
        """
        raise __not_implemented_error

    @abc.abstractmethod
    def get_available_symbols(self):
        """
        Get all visible symbols in the current context of debugging.
        """
        raise __not_implemented_error


class BridgeEventHandlerInterface(object):
    __metaclass__ = abc.ABCMeta

    """This interface defines the events that may be raised by the debugger 
    and can be handled by OpenImageDebugger """

    @abc.abstractmethod
    def stop_handler(self):
        """
        Handler to be called whenever the debugger stops (e.g. when a
        breakpoint is hit). May also be called when the user requests a change
        in the stack position to the debugger.
        """
        raise __not_implemented_error

    @abc.abstractmethod
    def exit_handler(self):
        """
        Event raised whenever the inferior has exited.
        """
        raise __not_implemented_error

    @abc.abstractmethod
    def plot_handler(self, variable_name):
        """
        Handler to be called whenever the user actively calls the 'plot'
        command from the debugger console.
        """
        raise __not_implemented_error


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
        raise __not_implemented_error

    @abc.abstractmethod
    def __int__(self):
        # type: () -> int
        """
        :return: Runtime content of the symbol in int format
        """
        raise __not_implemented_error

    @abc.abstractmethod
    def __float__(self):
        # type: () -> float
        """
        :return: Runtime content of the symbol in float format
        """
        raise __not_implemented_error

    @abc.abstractmethod
    def __getitem__(self, member):
        # type: (Union[str, int]) -> DebuggerSymbolReference
        """
        :param member: Member name or index value (in case of arrays)
        :return: Debugger interface for requested member field
        """
        raise __not_implemented_error

