# -*- coding: utf-8 -*-

"""
Code responsible with directly interacting with GDB
"""

import gdb

from giwscripts import sysinfo
from giwscripts.debuggers.interfaces import BridgeInterface


class GdbBridge(BridgeInterface):
    """
    GDB Bridge class, exposing the common expected interface for the ImageWatch
    to access the required buffer data and interact with the debugger.
    """
    def __init__(self, type_inspector):
        self._type_inspector = type_inspector
        self._commands = dict(plot=PlotterCommand(self))

    def queue_request(self, callable_request):
        return gdb.post_event(callable_request)

    def get_buffer_metadata(self, variable):
        picked_obj = gdb.parse_and_eval(variable)

        buffer_metadata = self._type_inspector.get_buffer_metadata(
            variable, picked_obj, self)

        bufsize = sysinfo.get_buffer_size(
            buffer_metadata['height'],
            buffer_metadata['channels'],
            buffer_metadata['type'],
            buffer_metadata['row_stride']
        )

        # Check if buffer is initialized
        if buffer_metadata['pointer'] == 0x0:
            raise Exception('Invalid null buffer pointer')
        if bufsize == 0:
            raise Exception('Invalid buffer of zero bytes')
        elif bufsize >= sysinfo.get_memory_usage()['free'] / 10:
            raise Exception('Invalid buffer size larger than available memory')

        # Check if buffer is valid. If it isn't, this function will throw an
        # exception
        gdb.execute('x '+str(int(buffer_metadata['pointer'])))

        inferior = gdb.selected_inferior()
        buffer_metadata['variable_name'] = variable
        buffer_metadata['pointer'] = inferior.read_memory(
            buffer_metadata['pointer'], bufsize)

        return buffer_metadata

    def register_event_handlers(self, event_handler):
        gdb.events.stop.connect(event_handler.stop_handler)
        gdb.events.exited.connect(event_handler.exit_handler)
        self._commands['plot'].set_command_listener(event_handler.plot_handler)

    def get_fields_from_type(self, this_type, observable_symbols):
        """
        Given a class/struct type "this_type", fetch all members of that
        particular type and test them against the observable buffer check.
        """
        for field_name, field_val in this_type.iteritems():
            if field_val.is_base_class:
                type_fields = self.get_fields_from_type(field_val.type,
                                                        observable_symbols)
                observable_symbols.update(type_fields)
            elif ((field_name not in observable_symbols) and
                  (self._type_inspector.is_symbol_observable(field_val))):
                try:
                    observable_symbols.add(field_name)
                except Exception:
                    print('[gdb-imagewatch] Info: Member %s is not observable'
                          % field_name)
        return observable_symbols

    def get_casted_pointer(self, typename, gdb_object):
        typename_obj = gdb.lookup_type(typename)
        typename_pointer_obj = typename_obj.pointer()
        return gdb_object.cast(typename_pointer_obj)

    def get_available_symbols(self):
        frame = gdb.selected_frame()
        block = frame.block()
        observable_symbols = set()

        while block is not None:
            for symbol in block:
                if symbol.is_argument or symbol.is_variable:
                    name = symbol.name
                    # Get struct/class fields
                    if name == 'this':
                        # The GDB API is a bit convoluted, so I have to do some
                        # contortion in order to get the class type from the
                        # this object so I can iterate over its fields
                        this_type = gdb.parse_and_eval(symbol.name) \
                            .dereference().type
                        type_fields = self.get_fields_from_type(
                            this_type,
                            observable_symbols)
                        observable_symbols.update(type_fields)
                    elif ((name not in observable_symbols) and
                          (self._type_inspector.is_symbol_observable(symbol))):
                        try:
                            observable_symbols.add(name)
                        except Exception:
                            print('[gdb-imagewatch] Info: Field %s is not'
                                  'observable' % name)

            block = block.superblock

        return observable_symbols


class PlotterCommand(gdb.Command):
    """
    Implements the 'plot' command for the GDB command line mode
    """
    def __init__(self, gdb_bridge):
        super(PlotterCommand, self).__init__("plot",
                                             gdb.COMMAND_DATA,
                                             gdb.COMPLETE_SYMBOL)
        self._gdb_bridge = gdb_bridge
        self._command_listener = None

    def set_command_listener(self, callback):
        """
        Called by the GDB bridge in order to configure which callback must be
        called when the user calls the function 'plot' in the debugger console.
        """
        self._command_listener = callback

    def invoke(self, arg, from_tty):
        """
        Called by GDB whenever the plot command is invoked.
        """
        args = gdb.string_to_argv(arg)
        var_name = str(args[0])

        if self._command_listener is not None:
            self._command_listener(var_name)
