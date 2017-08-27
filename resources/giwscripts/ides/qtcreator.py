# QtCreator integration module.

"""
Hacks into Dumper and changes its fetchVariables method to a wrapper that calls
stop_event_handler. This allows us to retrieve the list of locals every time
the user changes the local stack frame during a debug session from QtCreator
interface.
"""
def registerSymbolFetchHook(retrieveSymbolsCallback):
    try:
        imp = __import__('gdbbridge')

        originalFetchVariables = None
        def fetchVariablesWrapper(self, args):
            ret = originalFetchVariables(self, args)

            retrieveSymbolsCallback(None)

            return ret

        originalFetchVariables = imp.Dumper.fetchVariables
        imp.Dumper.fetchVariables = fetchVariablesWrapper
        return True

    except Exception as err:
        print(err)
        return False
