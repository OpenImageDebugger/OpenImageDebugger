import gdb

from giwscripts import sysinfo
from giwscripts.debuggers.interfaces import BridgeInterface

class GdbBridge(BridgeInterface):

    def __init__(self, type_inspector):
        self._type_inspector = type_inspector
        self._commands = dict(plot=PlotterCommand(self))

    def queue_request(self, callable_request):
        return gdb.post_event(callable_request)

    def get_buffer_metadata(self, variable):
        picked_obj = gdb.parse_and_eval(variable)

        buffer, width, height, channels, type, step, pixel_layout = self._type_inspector.get_buffer_info(picked_obj, self)

        bytes = sysinfo.get_buffer_size(width, height, channels, type, step)

        # Check if buffer is initialized
        if buffer == 0x0:
            raise Exception('Invalid null buffer')
        if bytes == 0:
            raise Exception('Invalid buffer of zero bytes')
        elif bytes >= sysinfo.get_memory_usage()['free']:
            raise Exception('Invalid buffer size larger than available memory')

        # Check if buffer is valid. If it isn't, this function will throw an exception
        gdb.execute('x '+str(int(buffer)))

        inferior = gdb.selected_inferior()
        mem = inferior.read_memory(buffer, bytes)

        return [mem, width, height, channels, type, step, pixel_layout]

    def register_event_handlers(self, event_handler):
        gdb.events.stop.connect(event_handler.stop_handler)
        self._commands['plot'].set_command_listener(event_handler.plot_handler)

    def get_fields_from_type(self, this_type, observable_symbols):
        for field_name, field_val in this_type.iteritems():
            if field_val.is_base_class:
                observable_symbols.update(self.get_fields_from_type(field_val.type, observable_symbols))
            elif (not field_name in observable_symbols) and (self._type_inspector.is_symbol_observable(field_val)):
                try:
                    observable_symbols[field_name] = self.get_buffer_metadata(field_name)
                except:
                    print('[gdb-imagewatch] Info: Member "' + field_name + '" is not observable')
                    pass
        return observable_symbols

    def get_casted_pointer(self, typename, gdb_object):
        typename_obj = gdb.lookup_type(typename)
        typename_pointer_obj = typename_obj.pointer()
        return gdb_object.cast(typename_pointer_obj)

    def get_available_symbols(self):
        frame = gdb.selected_frame()
        block = frame.block()
        observable_symbols = dict()

        while not block is None:
            for symbol in block:
                if (symbol.is_argument or symbol.is_variable):
                    name = symbol.name
                    # Get struct/class fields
                    if name == 'this':
                        # The GDB API is a bit convoluted, so I have to do some contortion in order
                        # to get the class type from the this object so I can iterate over its fields
                        this_type = gdb.parse_and_eval(symbol.name).dereference().type
                        observable_symbols.update(self.get_fields_from_type(this_type, observable_symbols))
                    elif (not name in observable_symbols) and (self._type_inspector.is_symbol_observable(symbol)):
                        try:
                            observable_symbols[name] = self.get_buffer_metadata(name)
                        except Exception as err:
                            print('[gdb-imagewatch] Info: Field "' + name + '" is not observable')
                        pass
                    pass
                pass

            block = block.superblock
            pass

        return observable_symbols

class PlotterCommand(gdb.Command):
    def __init__(self, gdb_bridge):
        super(PlotterCommand, self).__init__("plot",
                                             gdb.COMMAND_DATA,
                                             gdb.COMPLETE_SYMBOL)
        self._gdb_bridge = gdb_bridge
        self._command_listener = None
        pass

    def set_command_listener(self, callback):
        self._command_listener = callback

    def invoke(self, arg, from_tty):
        args = gdb.string_to_argv(arg)
        var_name = str(args[0])

        if not self._command_listener is None:
            self._command_listener(var_name)
        pass

    pass

