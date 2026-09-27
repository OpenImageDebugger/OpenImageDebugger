# -*- coding: utf-8 -*-

"""JSON resolver entry point for hosts that embed a debugger's Python.

A host with an embedded interpreter (lldb, gdb) can import this module and
call resolve()/list_observable() to obtain buffer metadata as JSON, without
reimplementing type inspection. Every payload ends with a sentinel so a
caller can detect a result its transport truncated: several debug adapters
clip long expression results silently, and a clipped JSON document can
still parse into something plausible but wrong.
"""

import json

from oidscripts import sysinfo

SENTINEL = '|OIDEND'

CONTRACT_KEYS = ('display_name', 'width', 'height', 'channels', 'type',
                 'row_stride', 'pixel_layout', 'transpose_buffer')

# Sized so a page fits a transport's ~1024-character string ceiling.
DEFAULT_OBSERVABLE_PAGE_LIMIT = 8

# Only stops an absurd request; 128 items is roughly 16KB.
MAX_OBSERVABLE_PAGE_LIMIT = 128


def _emit(payload):
    # Compact separators: bytes count against the page ceiling. ensure_ascii
    # stays on: a non-ASCII type name must cross as \uXXXX, not raw bytes.
    return json.dumps(payload, separators=(',', ':')) + SENTINEL


def _default_host():
    # Imported lazily so the module is testable without a live debugger.
    from oidscripts.oid_resolve_host import current_host
    return current_host()


def resolve(name, host=None):
    """Resolve one symbol to a buffer-metadata JSON payload."""
    try:
        host = host if host is not None else _default_host()
        metadata = host.buffer_metadata(name)
    except Exception as exc:                       # noqa: BLE001
        return _emit({'error': str(exc)})

    if metadata is None:
        return _emit({'error': 'no inspector matched %r' % name})

    try:
        out = {key: metadata[key] for key in CONTRACT_KEYS}

        # Bridges disagree: one yields an int, one a debugger value object.
        pointer = int(metadata['pointer'])
        if pointer == 0:
            return _emit({'error': 'null buffer pointer for %r' % name})
        out['pointer'] = pointer

        # Authoritative size. Deriving it from width instead of row_stride
        # under-reads every padded or strided buffer.
        out['byte_count'] = sysinfo.get_buffer_size(
            out['height'], out['channels'], out['type'], out['row_stride'])
        if out['byte_count'] <= 0:
            return _emit({'error': 'buffer of zero bytes for %r' % name})

        # Inside the guard: an unencodable value must yield an error payload.
        return _emit(out)
    except Exception as exc:                        # noqa: BLE001
        return _emit({'error': 'malformed metadata for %r: %s' % (name, exc)})


def list_observable(offset=0, limit=DEFAULT_OBSERVABLE_PAGE_LIMIT, host=None):
    """A page of plottable symbols, so a long list cannot overrun a
    transport's string ceiling.

    offset and limit are caller-supplied and clamped rather than trusted:
    a negative offset would silently read from the end of the list, a
    negative limit would silently read as an empty page, and an unbounded
    limit would defeat the reason this function pages at all. Clamping is
    silent -- a caller asking for too much gets a valid page back, not an
    error.
    """
    try:
        offset = max(0, offset)
        limit = min(max(1, limit), MAX_OBSERVABLE_PAGE_LIMIT)
        host = host if host is not None else _default_host()
        symbols = host.observable_symbols()
        page = symbols[offset:offset + limit]
        total = len(symbols)
        return _emit({'items': page, 'total': total, 'offset': offset})
    except Exception as exc:                       # noqa: BLE001
        return _emit({'error': str(exc)})
