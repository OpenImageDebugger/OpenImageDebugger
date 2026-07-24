# -*- coding: utf-8 -*-

"""
Debugger-prompt diagnostic: run one symbol through every registered inspector
and print each result side by side, so a maintainer can compare the JSON
built-ins against any surviving Python inspector at a live prompt:

    (gdb) python from oidscripts import conformance
    (gdb) python conformance.compare('img')

`oid.py` calls register() at bridge setup so compare() can reach the live
bridge without the user wiring anything.
"""

from oidscripts.oidtypes.declarative import DeclarativeInspector

_debugger_bridge = None
_type_bridge = None


def register(debugger_bridge, type_bridge):
    """Record the live bridges for later compare() calls."""
    global _debugger_bridge, _type_bridge
    _debugger_bridge = debugger_bridge
    _type_bridge = type_bridge


def _label(inspector):
    if isinstance(inspector, DeclarativeInspector):
        return 'JSON:%s (%s)' % (inspector.name, inspector.source)
    return 'python:%s' % type(inspector).__name__


def compare(symbol_name):
    """Print each inspector's metadata (or error) for the given symbol."""
    if _debugger_bridge is None or _type_bridge is None:
        print('conformance: no debugger bridge registered')
        return
    # A failed symbol lookup (unknown symbol, no stopped frame) is reported
    # once and returns, never raised: the diagnostic must not become a
    # traceback before any inspector output is printed.
    try:
        picked_obj = _debugger_bridge.evaluate_expression(symbol_name)
    except Exception as error:  # noqa: BLE001
        print('conformance: ERROR evaluating %r: %s' % (symbol_name, error))
        return
    for inspector in _type_bridge.get_inspectors():
        label = _label(inspector)
        # Each inspector's own failure is reported against its label, never
        # raised, so one bad inspector cannot hide the others.
        try:
            metadata = inspector.get_buffer_metadata(
                symbol_name, picked_obj, _debugger_bridge)
        except Exception as error:  # noqa: BLE001
            print('%s -> ERROR: %s' % (label, error))
        else:
            print('%s -> %r' % (label, metadata))
